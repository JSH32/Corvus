#include "linp/asset/asset_manager.hpp"
#include "linp/asset/loaders.hpp"
#include "linp/log.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <unordered_map>

namespace Linp::Core {

static std::string normalizePath(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');

    // Remove double slashes
    size_t pos;
    while ((pos = path.find("//")) != std::string::npos) {
        path.erase(pos, 1);
    }

    // Remove trailing slash unless it's root
    if (path.length() > 1 && path.back() == '/') {
        path.pop_back();
    }

    return path;
}

static std::string stripLeadingSlash(const std::string& path) {
    if (!path.empty() && path[0] == '/') {
        return path.substr(1);
    }
    return path;
}

static std::string ensureLeadingSlash(const std::string& path) {
    if (path.empty() || path == "/")
        return "/";
    if (path[0] == '/')
        return path;
    return "/" + path;
}

static void ensureParentDirs(const std::string& path) {
    auto lastSlash = path.find_last_of('/');
    if (lastSlash != std::string::npos) {
        PHYSFS_mkdir(path.substr(0, lastSlash).c_str());
    }
}

static bool physfsCopyFile(const std::string& srcPath, const std::string& dstPath) {
    PHYSFS_File* in = PHYSFS_openRead(srcPath.c_str());
    if (!in)
        return false;

    ensureParentDirs(dstPath);
    PHYSFS_File* out = PHYSFS_openWrite(dstPath.c_str());
    if (!out) {
        PHYSFS_close(in);
        return false;
    }

    std::array<char, 64 * 1024> buffer {};
    PHYSFS_sint64               bytesRead;
    bool                        success = true;

    while ((bytesRead = PHYSFS_readBytes(in, buffer.data(), buffer.size())) > 0) {
        if (PHYSFS_writeBytes(out, buffer.data(), bytesRead) != bytesRead) {
            success = false;
            break;
        }
    }

    PHYSFS_close(out);
    PHYSFS_close(in);
    return success;
}

AssetManager::AssetManager(const std::string& assetRoot, const std::string& alias)
    : projectPath(assetRoot), physfsAlias(alias), watcherRunning(false) {
    if (!PHYSFS_mount(assetRoot.c_str(), alias.c_str(), 1)) {
        throw std::runtime_error("Failed to mount asset root: " + assetRoot);
    }
    if (!PHYSFS_setWriteDir(assetRoot.c_str())) {
        throw std::runtime_error("Failed to set PhysFS write directory: " + assetRoot);
    }

    registerLoaders(*this);
    LINP_CORE_INFO("AssetManager mounted '{}' at '/{}'", assetRoot, alias);
}

AssetManager::~AssetManager() {
    LINP_CORE_INFO("AssetManager shutting down...");
    stopFileWatcher();
    unloadAll();
    PHYSFS_unmount(projectPath.c_str());
    LINP_CORE_INFO("AssetManager shutdown complete");
}

// Convert user path to PhysFS path (adds mount prefix)
std::string AssetManager::toPhysFS(const std::string& userPath) const {
    std::string normalized = stripLeadingSlash(normalizePath(userPath));
    if (normalized.empty()) {
        return physfsAlias;
    }
    return physfsAlias + "/" + normalized;
}

// Convert user path to internal canonical format (VFS with leading slash)
std::string AssetManager::toInternal(const std::string& userPath) const {
    return ensureLeadingSlash(normalizePath(userPath));
}

std::string AssetManager::getFullPath(const std::string& relativePath) const {
    return toInternal(relativePath);
}

std::string AssetManager::getMetaFilePath(const std::string& assetPath) {
    return assetPath + ".meta";
}

std::string AssetManager::getFileExtension(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "";

    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

uint64_t AssetManager::getFileModTime(const std::string& internalPath) {
    PHYSFS_Stat stat;
    if (!PHYSFS_stat(toPhysFS(internalPath).c_str(), &stat)) {
        return 0;
    }
    return static_cast<uint64_t>(stat.modtime);
}

AssetType AssetManager::getAssetTypeFromExtension(const std::string& ext) {
    static const std::unordered_map<std::string, AssetType> extensionMap = {
        { ".scene", AssetType::Scene }, { ".png", AssetType::Texture },
        { ".jpg", AssetType::Texture }, { ".jpeg", AssetType::Texture },
        { ".bmp", AssetType::Texture }, { ".obj", AssetType::Model },
        { ".gltf", AssetType::Model },  { ".glb", AssetType::Model },
        { ".wav", AssetType::Audio },   { ".ogg", AssetType::Audio },
        { ".mp3", AssetType::Audio },   { ".vs", AssetType::Shader },
        { ".fs", AssetType::Shader },   { ".ttf", AssetType::Font },
        { ".otf", AssetType::Font },
    };

    auto it = extensionMap.find(ext);
    return (it != extensionMap.end()) ? it->second : AssetType::Unknown;
}

bool AssetManager::loadMetaFile(const std::string& assetInternalPath, AssetMetadata& outMeta) {
    std::string metaPath = toPhysFS(getMetaFilePath(assetInternalPath));

    PHYSFS_File* file = PHYSFS_openRead(metaPath.c_str());
    if (!file)
        return false;

    PHYSFS_sint64 size = PHYSFS_fileLength(file);
    if (size <= 0) {
        PHYSFS_close(file);
        return false;
    }

    std::string content(static_cast<size_t>(size), '\0');
    PHYSFS_readBytes(file, content.data(), size);
    PHYSFS_close(file);

    try {
        std::istringstream       stream(content);
        cereal::JSONInputArchive archive(stream);
        archive(cereal::make_nvp("asset", outMeta));

        // Ensure path is in internal format
        outMeta.path = toInternal(outMeta.path);
        return true;
    } catch (const std::exception& e) {
        LINP_CORE_ERROR("Failed to parse meta file '{}': {}", metaPath, e.what());
        return false;
    }
}

bool AssetManager::saveMetaFile(const std::string& assetInternalPath, const AssetMetadata& meta) {
    std::string metaPath = stripLeadingSlash(getMetaFilePath(assetInternalPath));

    try {
        std::ostringstream stream;
        {
            cereal::JSONOutputArchive archive(stream);
            archive(cereal::make_nvp("asset", meta));
        }
        std::string json = stream.str();

        ensureParentDirs(metaPath);

        PHYSFS_File* file = PHYSFS_openWrite(metaPath.c_str());
        if (!file) {
            LINP_CORE_ERROR("Failed to open meta file for writing: {}", metaPath);
            return false;
        }

        PHYSFS_writeBytes(file, json.c_str(), json.size());
        PHYSFS_close(file);
        return true;
    } catch (const std::exception& e) {
        LINP_CORE_ERROR("Failed to save meta file for '{}': {}", assetInternalPath, e.what());
        return false;
    }
}

std::vector<std::string> AssetManager::getDirectories(const std::string& userPath) const {
    std::vector<std::string> result;
    std::string              physfsPath = toPhysFS(userPath);

    char** entries = PHYSFS_enumerateFiles(physfsPath.c_str());
    if (!entries)
        return result;

    for (char** entry = entries; *entry; ++entry) {
        std::string fullPath = physfsPath + "/" + *entry;

        PHYSFS_Stat stat;
        if (PHYSFS_stat(fullPath.c_str(), &stat) && stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {

            // Return user-friendly path (no prefix, just relative path)
            std::string normalized = normalizePath(userPath);
            if (normalized.empty() || normalized == "/") {
                result.push_back(*entry);
            } else {
                result.push_back(stripLeadingSlash(normalized) + "/" + *entry);
            }
        }
    }

    PHYSFS_freeList(entries);
    return result;
}

std::vector<AssetMetadata> AssetManager::getAssetsInDirectory(const std::string& userPath) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    std::vector<AssetMetadata>  result;

    std::string internalPath = toInternal(userPath);

    for (const auto& [id, meta] : metadata) {
        // Check if asset is in this directory (direct child only)
        if (internalPath == "/") {
            // Root: only files with exactly one slash
            if (std::count(meta.path.begin(), meta.path.end(), '/') == 1) {
                result.push_back(meta);
            }
        } else {
            // Subdirectory: must start with path and have no additional slashes
            if (meta.path.rfind(internalPath + "/", 0) == 0) {
                std::string remainder = meta.path.substr(internalPath.length() + 1);
                if (remainder.find('/') == std::string::npos) {
                    result.push_back(meta);
                }
            }
        }
    }

    return result;
}

bool AssetManager::createDirectory(const std::string& userPath) {
    std::string physfsPath = stripLeadingSlash(normalizePath(userPath));

    if (!PHYSFS_mkdir(physfsPath.c_str())) {
        LINP_CORE_ERROR("Failed to create directory: {}", userPath);
        return false;
    }

    LINP_CORE_INFO("Created directory: {}", userPath);
    return true;
}

bool AssetManager::deleteDirectory(const std::string& userPath) {
    std::string physfsPath = stripLeadingSlash(normalizePath(userPath));

    if (!PHYSFS_delete(physfsPath.c_str())) {
        LINP_CORE_ERROR("Failed to delete directory: {}", userPath);
        return false;
    }

    LINP_CORE_INFO("Deleted directory: {}", userPath);
    return true;
}

// ============================================================================
// Asset Operations
// ============================================================================

bool AssetManager::copyAsset(const UUID& id, const std::string& newUserPath, bool includeMeta) {
    std::lock_guard<std::mutex> lock(assetMutex);

    auto it = metadata.find(id);
    if (it == metadata.end())
        return false;

    const AssetMetadata& srcMeta         = it->second;
    std::string          dstInternalPath = toInternal(newUserPath);

    // Copy the asset file
    std::string srcPhysfsPath = toPhysFS(srcMeta.path);
    std::string dstPhysfsPath = stripLeadingSlash(dstInternalPath);

    if (!physfsCopyFile(srcPhysfsPath, dstPhysfsPath)) {
        LINP_CORE_ERROR("Failed to copy asset: {} -> {}", srcMeta.path, dstInternalPath);
        return false;
    }

    // Create new metadata
    AssetMetadata newMeta = srcMeta;
    newMeta.id            = boost::uuids::random_generator()();
    newMeta.path          = dstInternalPath;
    newMeta.lastModified  = getFileModTime(dstInternalPath);

    saveMetaFile(newMeta.path, newMeta);
    metadata[newMeta.id]                = newMeta;
    pathToID[newMeta.path]              = newMeta.id;
    fileModificationTimes[newMeta.path] = newMeta.lastModified;

    LINP_CORE_INFO("Copied asset: {} -> {}", srcMeta.path, dstInternalPath);
    return true;
}

bool AssetManager::deleteAsset(const UUID& id) {
    std::lock_guard<std::mutex> lock(assetMutex);

    auto it = metadata.find(id);
    if (it == metadata.end())
        return false;

    const std::string& internalPath = it->second.path;
    std::string        physfsPath   = stripLeadingSlash(internalPath);

    // Delete asset and meta files
    PHYSFS_delete(physfsPath.c_str());
    PHYSFS_delete(stripLeadingSlash(getMetaFilePath(internalPath)).c_str());

    // Remove from tracking
    pathToID.erase(internalPath);
    assets.erase(id);
    metadata.erase(id);
    fileModificationTimes.erase(internalPath);

    LINP_CORE_INFO("Deleted asset: {}", internalPath);
    return true;
}

bool AssetManager::moveAsset(const UUID& id, const std::string& newUserPath) {
    std::lock_guard<std::mutex> lock(assetMutex);

    auto it = metadata.find(id);
    if (it == metadata.end())
        return false;

    const std::string oldInternalPath = it->second.path;
    const std::string newInternalPath = toInternal(newUserPath);

    // Copy to new location
    if (!physfsCopyFile(toPhysFS(oldInternalPath), stripLeadingSlash(newInternalPath))) {
        LINP_CORE_ERROR("Failed to move asset: {} -> {}", oldInternalPath, newInternalPath);
        return false;
    }

    // Delete old files
    PHYSFS_delete(stripLeadingSlash(oldInternalPath).c_str());
    PHYSFS_delete(stripLeadingSlash(getMetaFilePath(oldInternalPath)).c_str());

    // Update metadata
    pathToID.erase(oldInternalPath);
    it->second.path         = newInternalPath;
    it->second.lastModified = getFileModTime(newInternalPath);

    saveMetaFile(it->second.path, it->second);
    pathToID[it->second.path]              = id;
    fileModificationTimes[it->second.path] = it->second.lastModified;

    // Update loaded asset if present
    if (auto assetIt = assets.find(id); assetIt != assets.end()) {
        assetIt->second.path         = it->second.path;
        assetIt->second.lastModified = it->second.lastModified;
    }

    LINP_CORE_INFO("Moved asset: {} -> {}", oldInternalPath, newInternalPath);
    return true;
}

// ============================================================================
// Asset Scanning
// ============================================================================

void AssetManager::scanDirectory(const std::string& internalPath) {
    std::string physfsPath = toPhysFS(internalPath);

    char** entries = PHYSFS_enumerateFiles(physfsPath.c_str());
    if (!entries)
        return;

    for (char** entry = entries; *entry; ++entry) {
        std::string entryInternalPath
            = (internalPath == "/") ? "/" + std::string(*entry) : internalPath + "/" + *entry;

        PHYSFS_Stat stat;
        if (!PHYSFS_stat(toPhysFS(entryInternalPath).c_str(), &stat)) {
            continue;
        }

        if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
            scanDirectory(entryInternalPath);
            continue;
        }

        if (entryInternalPath.ends_with(".meta")) {
            continue;
        }

        // Load or create metadata
        AssetMetadata meta;
        if (!loadMetaFile(entryInternalPath, meta)) {
            meta.id           = boost::uuids::random_generator()();
            meta.path         = entryInternalPath;
            meta.type         = getAssetTypeFromExtension(getFileExtension(entryInternalPath));
            meta.lastModified = stat.modtime;
            saveMetaFile(meta.path, meta);
        } else {
            meta.lastModified = stat.modtime;
        }

        metadata[meta.id]                = meta;
        pathToID[meta.path]              = meta.id;
        fileModificationTimes[meta.path] = meta.lastModified;
    }

    PHYSFS_freeList(entries);
}

void AssetManager::scanAssets(const std::string& subDirectory, bool /*recursive*/) {
    LINP_CORE_INFO("Scanning assets in mount '/{}'", physfsAlias);
    scanDirectory(toInternal(subDirectory));
    LINP_CORE_INFO("Asset scan complete: {} assets indexed", metadata.size());
}

// ============================================================================
// Reference Counting
// ============================================================================

void AssetManager::incrementRef(const UUID& id) {
    auto it = assets.find(id);
    if (it != assets.end()) {
        it->second.refCount++;
    }
}

void AssetManager::decrementRef(const UUID& id) {
    auto it = assets.find(id);
    if (it != assets.end() && --it->second.refCount <= 0) {
        LINP_CORE_INFO("Asset ref count reached 0: {}", it->second.path);
    }
}

void AssetManager::unload(const UUID& id) {
    std::lock_guard<std::mutex> lock(assetMutex);
    assets.erase(id);
    LINP_CORE_INFO("Unloaded asset: {}", boost::uuids::to_string(id));
}

void AssetManager::unloadUnused() {
    std::lock_guard<std::mutex> lock(assetMutex);
    std::erase_if(assets, [](const auto& pair) { return pair.second.refCount <= 0; });
}

void AssetManager::unloadAll() {
    std::lock_guard<std::mutex> lock(assetMutex);
    assets.clear();
    LINP_CORE_INFO("Unloaded all assets");
}

// ============================================================================
// File Watcher
// ============================================================================

void AssetManager::checkFileChanges() {
    if (!watcherRunning.load())
        return;

    std::lock_guard<std::mutex> lock(assetMutex);

    for (auto& [internalPath, lastModTime] : fileModificationTimes) {
        uint64_t currentModTime = getFileModTime(internalPath);

        if (currentModTime > lastModTime) {
            lastModTime = currentModTime;
            LINP_CORE_INFO("Detected file change: {}", internalPath);

            auto it = pathToID.find(internalPath);
            if (it != pathToID.end()) {
                const UUID& assetId = it->second;
                for (auto& callback : assetReloadedCallbacks) {
                    try {
                        callback(assetId, internalPath);
                    } catch (...) { }
                }
            }
        }
    }
}

void AssetManager::fileWatcherLoop(int pollIntervalMs) {
    while (watcherRunning.load()) {
        checkFileChanges();
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
    }
}

void AssetManager::startFileWatcher(int pollIntervalMs) {
    if (watcherRunning)
        return;

    watcherRunning = true;
    watcherThread  = std::thread(&AssetManager::fileWatcherLoop, this, pollIntervalMs);
    LINP_CORE_INFO("File watcher started ({} ms poll interval)", pollIntervalMs);
}

void AssetManager::stopFileWatcher() {
    if (!watcherRunning.load())
        return;

    watcherRunning = false;
    if (watcherThread.joinable()) {
        watcherThread.join();
    }
    LINP_CORE_INFO("File watcher stopped");
}

void AssetManager::onAssetReloaded(std::function<void(const UUID&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(assetMutex);
    assetReloadedCallbacks.push_back(std::move(callback));
}

// ============================================================================
// Queries
// ============================================================================

AssetMetadata AssetManager::getMetadata(const UUID& id) const {
    std::lock_guard<std::mutex> lock(assetMutex);

    auto it = metadata.find(id);
    if (it != metadata.end()) {
        return it->second;
    }

    return {};
}

int AssetManager::getRefCount(const UUID& id) const {
    std::lock_guard<std::mutex> lock(assetMutex);

    auto it = assets.find(id);
    if (it != assets.end()) {
        return it->second.refCount;
    }

    return 0;
}

std::vector<std::pair<std::string, AssetType>> AssetManager::getCreatableAssetTypes() const {
    std::vector<std::pair<std::string, AssetType>> result;

    for (const auto& [typeIndex, loader] : loaders) {
        if (!loader->canCreate())
            continue;

        AssetType   type = loader->getType();
        std::string name;

        switch (type) {
            case AssetType::Scene:
                name = "Scene";
                break;
            case AssetType::Texture:
                name = "Texture";
                break;
            case AssetType::Audio:
                name = "Audio";
                break;
            case AssetType::Shader:
                name = "Shader";
                break;
            default:
                name = "Unknown";
                break;
        }

        result.emplace_back(name, type);
    }

    return result;
}

} // namespace Linp::Core
