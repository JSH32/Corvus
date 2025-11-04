#include "corvus/asset/asset_manager.hpp"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/asset/loaders.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/log.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <physfs.h>
#include <sstream>
#include <unordered_map>

namespace Corvus::Core {

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

AssetManager::AssetManager(Graphics::GraphicsContext* graphicsContext,
                           const std::string&         assetRoot,
                           const std::string&         alias)
    : projectPath(assetRoot), physfsAlias(alias), watcherRunning(false) {
    if (!PHYSFS_mount(assetRoot.c_str(), alias.c_str(), 1)) {
        throw std::runtime_error("Failed to mount asset root: " + assetRoot);
    }
    if (!PHYSFS_setWriteDir(assetRoot.c_str())) {
        throw std::runtime_error("Failed to set PhysFS write directory: " + assetRoot);
    }

    this->loaderContext.graphics = graphicsContext;

    // setupRaylibBridge();
    registerLoaders(*this);
    CORVUS_CORE_INFO("AssetManager mounted '{}' at '/{}'", assetRoot, alias);
}

AssetManager::~AssetManager() {
    CORVUS_CORE_INFO("AssetManager shutting down...");
    stopFileWatcher();
    shuttingDown.store(true, std::memory_order_relaxed);

    decltype(assets) localAssets;
    {
        std::lock_guard<std::mutex> lock(assetMutex);
        localAssets.swap(assets);
        pathToID.clear();
        metadata.clear();
        fileModificationTimes.clear();
    }

    localAssets.clear();
    PHYSFS_unmount(projectPath.c_str());
    CORVUS_CORE_INFO("AssetManager shutdown complete");
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
    auto extIt = extensionToType.find(ext);
    if (extIt == extensionToType.end()) {
        return AssetType::Unknown;
    }

    auto loaderIt = loaders.find(extIt->second);
    if (loaderIt == loaders.end()) {
        return AssetType::Unknown;
    }

    return loaderIt->second->getType();
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
        CORVUS_CORE_ERROR("Failed to parse meta file '{}': {}", metaPath, e.what());
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
            CORVUS_CORE_ERROR("Failed to open meta file for writing: {}", metaPath);
            return false;
        }

        PHYSFS_writeBytes(file, json.c_str(), json.size());
        PHYSFS_close(file);
        return true;
    } catch (const std::exception& e) {
        CORVUS_CORE_ERROR("Failed to save meta file for '{}': {}", assetInternalPath, e.what());
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
        CORVUS_CORE_ERROR("Failed to create directory: {}", userPath);
        return false;
    }

    CORVUS_CORE_INFO("Created directory: {}", userPath);
    return true;
}

bool AssetManager::deleteDirectory(const std::string& userPath) {
    std::lock_guard<std::mutex> lock(assetMutex);

    std::string internalPath = toInternal(userPath);

    CORVUS_CORE_INFO("Attempting to delete directory: {}", internalPath);

    bool result = deleteDirectoryRecursive(internalPath, true);

    if (result) {
        CORVUS_CORE_INFO("Successfully deleted directory: {}", internalPath);
    } else {
        CORVUS_CORE_ERROR("Failed to delete directory: {}", internalPath);
    }

    return result;
}

// Asset Operations
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
        CORVUS_CORE_ERROR("Failed to copy asset: {} -> {}", srcMeta.path, dstInternalPath);
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

    CORVUS_CORE_INFO("Copied asset: {} -> {}", srcMeta.path, dstInternalPath);
    return true;
}

bool AssetManager::deleteAsset(const UUID& id) {
    std::shared_ptr<void> dataToDestroy; // Will be destroyed after unlock

    {
        std::lock_guard<std::mutex> lock(assetMutex);

        auto it = metadata.find(id);
        if (it == metadata.end())
            return false;

        std::string internalPath = it->second.path;
        std::string physfsPath   = stripLeadingSlash(internalPath);

        // Delete asset and meta files
        PHYSFS_delete(physfsPath.c_str());
        PHYSFS_delete(stripLeadingSlash(getMetaFilePath(internalPath)).c_str());

        // Remove from tracking
        pathToID.erase(internalPath);

        // Extract the shared_ptr before erasing
        auto assetIt = assets.find(id);
        if (assetIt != assets.end()) {
            dataToDestroy = std::move(assetIt->second.data);
            assets.erase(assetIt);
        }

        metadata.erase(id);
        fileModificationTimes.erase(internalPath);

        CORVUS_CORE_INFO("Deleted asset: {}", internalPath);
    }

    // dataToDestroy destructor runs here, after mutex is unlocked
    return true;
}

bool AssetManager::moveAsset(const UUID& id, const std::string& newUserPath) {
    std::lock_guard<std::mutex> lock(assetMutex);

    auto it = metadata.find(id);
    if (it == metadata.end())
        return false;

    const std::string oldInternalPath = it->second.path;
    const std::string newInternalPath = toInternal(newUserPath);

    if (oldInternalPath == newInternalPath) {
        CORVUS_CORE_INFO("Move skipped: source and destination are identical ({})",
                         oldInternalPath);
        return true; // Not an error, just a no-op
    }

    // Copy to new location
    if (!physfsCopyFile(toPhysFS(oldInternalPath), stripLeadingSlash(newInternalPath))) {
        CORVUS_CORE_ERROR("Failed to move asset: {} -> {}", oldInternalPath, newInternalPath);
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
    pathToID[it->second.path] = id;
    fileModificationTimes.erase(oldInternalPath);
    fileModificationTimes[it->second.path] = it->second.lastModified;

    // Update loaded asset if present
    if (auto assetIt = assets.find(id); assetIt != assets.end()) {
        assetIt->second.path         = it->second.path;
        assetIt->second.lastModified = it->second.lastModified;
    }

    CORVUS_CORE_INFO("Moved asset: {} -> {}", oldInternalPath, newInternalPath);
    return true;
}

bool AssetManager::renameDirectory(const std::string& oldUserPath, const std::string& newUserPath) {
    std::lock_guard<std::mutex> lock(assetMutex);

    std::string oldInternal = toInternal(oldUserPath);
    std::string newInternal = toInternal(newUserPath);

    CORVUS_CORE_INFO("Renaming directory: {} -> {}", oldInternal, newInternal);

    // Collect all affected assets BEFORE moving
    std::vector<std::pair<UUID, std::string>> affectedAssets;
    for (const auto& [path, id] : pathToID) {
        if (path.rfind(oldInternal, 0) == 0) {
            affectedAssets.push_back({ id, path });
        }
    }

    // Copy directory to new location using PhysFS
    if (!copyDirectoryRecursive(oldInternal, newInternal)) {
        CORVUS_CORE_ERROR("Failed to copy directory during rename");
        return false;
    }

    // Update all metadata and internal mappings
    for (const auto& [id, oldPath] : affectedAssets) {
        // Calculate new path
        std::string newPath = newInternal + oldPath.substr(oldInternal.length());

        CORVUS_CORE_INFO("  Remapping: {} -> {}", oldPath, newPath);

        // Update metadata
        auto metaIt = metadata.find(id);
        if (metaIt != metadata.end()) {
            metaIt->second.path = newPath;
            // Save updated meta file (now in new location)
            saveMetaFile(newPath, metaIt->second);
        }

        // Update pathToID
        pathToID.erase(oldPath);
        pathToID[newPath] = id;

        // Update loaded assets
        auto assetIt = assets.find(id);
        if (assetIt != assets.end()) {
            assetIt->second.path = newPath;
        }

        // Update file modification times
        fileModificationTimes.erase(oldPath);
        fileModificationTimes[newPath] = getFileModTime(newPath);
    }

    // Delete old directory
    if (!deleteDirectoryRecursive(oldInternal, false)) {
        CORVUS_CORE_WARN("Failed to delete old directory after rename (files copied successfully)");
    }

    CORVUS_CORE_INFO("Directory renamed successfully");
    return true;
}

bool AssetManager::createAssetByType(AssetType          type,
                                     const std::string& relativePath,
                                     const std::string& name) {
    std::lock_guard<std::mutex> lock(assetMutex);

    // Find a loader that can create this type
    IAssetLoader*   loader        = nullptr;
    std::type_index targetTypeIdx = typeid(void);

    for (auto& [typeIdx, loaderPtr] : loaders) {
        if (loaderPtr->getType() == type && loaderPtr->canCreate()) {
            loader        = loaderPtr.get();
            targetTypeIdx = typeIdx;
            break;
        }
    }

    if (!loader) {
        CORVUS_CORE_ERROR("No loader found that can create assets of type {}",
                          static_cast<int>(type));
        return false;
    }

    // Ensure path has correct extension
    std::string finalPath = relativePath;
    std::string ext       = getFileExtension(relativePath);

    if (ext.empty()) {
        // Find the first registered extension for this type
        for (const auto& [extension, typeIdx] : extensionToType) {
            if (typeIdx == targetTypeIdx) {
                finalPath = relativePath + extension;
                break;
            }
        }
    }

    // Create the asset using the loader
    void* obj = loader->create(name);
    if (!obj) {
        CORVUS_CORE_ERROR("Loader failed to create asset");
        return false;
    }

    // Convert to internal path
    std::string internalPath = toInternal(finalPath);

    // Create metadata
    AssetMetadata meta;
    meta.id           = boost::uuids::random_generator()();
    meta.path         = internalPath;
    meta.type         = type;
    meta.lastModified = getFileModTime(internalPath);

    saveMetaFile(internalPath, meta);
    pathToID[internalPath] = meta.id;
    metadata[meta.id]      = meta;

    // Save the asset
    if (!loader->save(obj, toPhysFS(internalPath))) {
        CORVUS_CORE_ERROR("Failed to save newly created asset");
        loader->unload(obj);
        return false;
    }

    // Create asset entry
    auto deleter = [this, id = meta.id, loader](void* ptr) {
        loader->unload(ptr);
        decrementRef(id);
    };

    AssetEntry entry;
    entry.id           = meta.id;
    entry.path         = meta.path;
    entry.type         = meta.type;
    entry.typeIndex    = targetTypeIdx;
    entry.data         = std::shared_ptr<void>(obj, deleter);
    entry.loader       = loader;
    entry.refCount     = 1;
    entry.lastModified = meta.lastModified;

    assets.emplace(meta.id, std::move(entry));

    CORVUS_CORE_INFO("Created new asset: {}", internalPath);
    return true;
}

bool AssetManager::copyDirectoryRecursive(const std::string& srcInternal,
                                          const std::string& dstInternal) {
    // Convert to PhysFS paths for reading
    std::string srcPhysFS = toPhysFS(srcInternal);

    // Convert to write paths (strip mount prefix)
    std::string dstWrite = dstInternal;
    if (dstWrite[0] == '/') {
        dstWrite = dstWrite.substr(1);
    }

    // Create destination directory
    if (!PHYSFS_mkdir(dstWrite.c_str())) {
        CORVUS_CORE_ERROR("Failed to create directory: {}", dstWrite);
        return false;
    }

    // List all files in source directory
    char** files = PHYSFS_enumerateFiles(srcPhysFS.c_str());
    if (!files) {
        CORVUS_CORE_ERROR("Failed to enumerate directory: {}", srcPhysFS);
        return false;
    }

    for (char** file = files; *file != nullptr; file++) {
        std::string filename        = *file;
        std::string srcPath         = srcPhysFS + "/" + filename;
        std::string dstPath         = dstWrite + "/" + filename;
        std::string srcPathInternal = srcInternal + "/" + filename;
        std::string dstPathInternal = dstInternal + "/" + filename;

        PHYSFS_Stat stat;
        if (!PHYSFS_stat(srcPath.c_str(), &stat)) {
            continue;
        }

        if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
            // Recursively copy subdirectory
            if (!copyDirectoryRecursive(srcPathInternal, dstPathInternal)) {
                PHYSFS_freeList(files);
                return false;
            }
        } else {
            // Copy file
            PHYSFS_File* srcFile = PHYSFS_openRead(srcPath.c_str());
            if (!srcFile) {
                CORVUS_CORE_ERROR("Failed to open source file: {}", srcPath);
                continue;
            }

            PHYSFS_sint64     fileSize = PHYSFS_fileLength(srcFile);
            std::vector<char> buffer(fileSize);
            PHYSFS_sint64     bytesRead = PHYSFS_readBytes(srcFile, buffer.data(), fileSize);
            PHYSFS_close(srcFile);

            if (bytesRead != fileSize) {
                CORVUS_CORE_ERROR("Failed to read file: {}", srcPath);
                continue;
            }

            PHYSFS_File* dstFile = PHYSFS_openWrite(dstPath.c_str());
            if (!dstFile) {
                CORVUS_CORE_ERROR("Failed to create destination file: {}", dstPath);
                continue;
            }

            PHYSFS_sint64 bytesWritten = PHYSFS_writeBytes(dstFile, buffer.data(), fileSize);
            PHYSFS_close(dstFile);

            if (bytesWritten != fileSize) {
                CORVUS_CORE_ERROR("Failed to write file: {}", dstPath);
                continue;
            }
        }
    }

    PHYSFS_freeList(files);
    return true;
}

bool AssetManager::deleteDirectoryRecursive(const std::string& internalPath, bool untrackAssets) {
    // Convert to PhysFS path for reading
    std::string physFSPath = toPhysFS(internalPath);

    // Convert to write path (strip mount prefix)
    std::string writePath = internalPath;
    if (writePath[0] == '/') {
        writePath = writePath.substr(1);
    }

    // Only untrack assets if we're actually deleting (not renaming)
    if (untrackAssets) {
        std::vector<UUID> assetsToDelete;
        for (const auto& [path, id] : pathToID) {
            // Check if this asset is in the directory being deleted
            if (path.rfind(internalPath, 0) == 0) {
                assetsToDelete.push_back(id);
            }
        }

        // Untrack all assets in this directory
        for (const UUID& id : assetsToDelete) {
            CORVUS_CORE_INFO("  Untracking asset: {}", boost::uuids::to_string(id));

            // Remove from metadata
            auto metaIt = metadata.find(id);
            if (metaIt != metadata.end()) {
                // Remove from pathToID
                pathToID.erase(metaIt->second.path);
                metadata.erase(metaIt);
            }

            // Remove from loaded assets
            assets.erase(id);

            // Remove from file modification times
            auto timeIt = std::find_if(fileModificationTimes.begin(),
                                       fileModificationTimes.end(),
                                       [&internalPath](const auto& pair) {
                                           return pair.first.rfind(internalPath, 0) == 0;
                                       });
            if (timeIt != fileModificationTimes.end()) {
                fileModificationTimes.erase(timeIt);
            }
        }
    }

    // List all files in directory
    char** files = PHYSFS_enumerateFiles(physFSPath.c_str());
    if (!files) {
        CORVUS_CORE_ERROR("Failed to enumerate directory for deletion: {}", physFSPath);
        return false;
    }

    // Now delete all files and subdirectories
    for (char** file = files; *file != nullptr; file++) {
        std::string filename     = *file;
        std::string fullPhysFS   = physFSPath + "/" + filename;
        std::string fullWrite    = writePath + "/" + filename;
        std::string fullInternal = internalPath + "/" + filename;

        PHYSFS_Stat stat;
        if (!PHYSFS_stat(fullPhysFS.c_str(), &stat)) {
            continue;
        }

        if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
            // Recursively delete subdirectory (pass untrackAssets flag down)
            if (!deleteDirectoryRecursive(fullInternal, untrackAssets)) {
                CORVUS_CORE_WARN("Failed to delete subdirectory: {}", fullInternal);
            }
        } else {
            // Delete file (including .meta files)
            if (!PHYSFS_delete(fullWrite.c_str())) {
                CORVUS_CORE_WARN("Failed to delete file: {}", fullWrite);
            }
        }
    }

    PHYSFS_freeList(files);

    // Delete the directory itself
    if (!PHYSFS_delete(writePath.c_str())) {
        CORVUS_CORE_ERROR("Failed to delete directory: {}", writePath);
        return false;
    }

    return true;
}

// Asset Scanning
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
    CORVUS_CORE_INFO("Scanning assets in mount '/{}'", physfsAlias);
    scanDirectory(toInternal(subDirectory));
    CORVUS_CORE_INFO("Asset scan complete: {} assets indexed", metadata.size());
}

// Reference Counting
void AssetManager::incrementRef(const UUID& id) {
    auto it = assets.find(id);
    if (it != assets.end()) {
        it->second.refCount++;
    }
}

void AssetManager::decrementRef(const UUID& id) {
    if (shuttingDown.load(std::memory_order_relaxed))
        return;

    std::lock_guard<std::mutex> lock(assetMutex);
    if (assets.empty())
        return;

    auto it = assets.find(id);
    if (it != assets.end() && --it->second.refCount <= 0) {
        CORVUS_CORE_INFO("Asset ref count reached 0: {}", it->second.path);
    }
}

void AssetManager::unload(const UUID& id) {
    std::lock_guard<std::mutex> lock(assetMutex);
    assets.erase(id);
    CORVUS_CORE_INFO("Unloaded asset: {}", boost::uuids::to_string(id));
}

void AssetManager::unloadUnused() {
    std::lock_guard<std::mutex> lock(assetMutex);
    std::erase_if(assets, [](const auto& pair) { return pair.second.refCount <= 0; });
}

void AssetManager::unloadAll() {
    std::lock_guard<std::mutex> lock(assetMutex);
    assets.clear();
    CORVUS_CORE_INFO("Unloaded all assets");
}

// File Watcher
void AssetManager::checkFileChanges() {
    std::lock_guard<std::mutex> lock(assetMutex);

    std::unordered_set<std::string> seenFiles;

    std::function<void(const std::string&)> scanForChanges = [&](const std::string& dirPath) {
        std::string physfsPath = toPhysFS(dirPath);

        char** entries = PHYSFS_enumerateFiles(physfsPath.c_str());
        if (!entries) {
            return;
        }

        for (char** entry = entries; *entry; ++entry) {
            std::string entryInternalPath = toInternal(dirPath + "/" + *entry);

            PHYSFS_Stat stat;
            if (!PHYSFS_stat(toPhysFS(entryInternalPath).c_str(), &stat)) {
                continue;
            }

            if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                scanForChanges(entryInternalPath);
                continue;
            }

            if (entryInternalPath.ends_with(".meta")) {
                continue;
            }

            seenFiles.insert(entryInternalPath);

            // Check if this is a new file
            if (fileModificationTimes.find(entryInternalPath) == fileModificationTimes.end()) {
                CORVUS_CORE_INFO("Detected new file: {}", entryInternalPath);

                // Load or create metadata
                AssetMetadata meta;
                if (!loadMetaFile(entryInternalPath, meta)) {
                    meta.id   = boost::uuids::random_generator()();
                    meta.path = entryInternalPath;
                    meta.type = getAssetTypeFromExtension(getFileExtension(entryInternalPath));
                    meta.lastModified = stat.modtime;
                    saveMetaFile(meta.path, meta);
                }

                metadata[meta.id]                = meta;
                pathToID[meta.path]              = meta.id;
                fileModificationTimes[meta.path] = meta.lastModified;

                // Trigger callbacks for new asset
                for (auto& callback : assetReloadedCallbacks) {
                    try {
                        callback(meta.id, entryInternalPath);
                    } catch (...) { }
                }
            }
            // Check for modifications
            else {
                auto&    lastModTime    = fileModificationTimes[entryInternalPath];
                uint64_t currentModTime = stat.modtime;

                if (currentModTime > lastModTime) {
                    lastModTime = currentModTime;
                    CORVUS_CORE_INFO("Detected file modification: {}", entryInternalPath);

                    auto it = pathToID.find(entryInternalPath);
                    if (it != pathToID.end()) {
                        const UUID& assetId = it->second;

                        // Check if meta file exists, recreate if missing
                        AssetMetadata meta;
                        if (!loadMetaFile(entryInternalPath, meta)) {
                            CORVUS_CORE_WARN("Meta file missing for {}, recreating",
                                             entryInternalPath);
                            meta              = metadata[assetId];
                            meta.lastModified = currentModTime;
                            saveMetaFile(meta.path, meta);
                        }

                        for (auto& callback : assetReloadedCallbacks) {
                            try {
                                callback(assetId, entryInternalPath);
                            } catch (...) { }
                        }
                    }
                }
            }
        }

        PHYSFS_freeList(entries);
    };

    // Recursive scan
    scanForChanges("/");

    // Check for deleted meta files and recreate them
    for (const auto& [path, uuid] : pathToID) {
        if (seenFiles.find(path) != seenFiles.end()) {
            // File exists, check if meta file exists
            std::string metaPhysfsPath = toPhysFS(getMetaFilePath(path));
            PHYSFS_Stat metaStat;

            if (!PHYSFS_stat(metaPhysfsPath.c_str(), &metaStat)) {
                // Meta file is missing, recreate it
                CORVUS_CORE_WARN("Meta file missing for existing asset {}, recreating", path);

                auto metaIt = metadata.find(uuid);
                if (metaIt != metadata.end()) {
                    saveMetaFile(path, metaIt->second);
                }
            }
        }
    }

    // Check for deleted files
    std::vector<std::string> deletedFiles;
    for (const auto& [path, _] : fileModificationTimes) {
        if (seenFiles.find(path) == seenFiles.end()) {
            deletedFiles.push_back(path);
        }
    }

    for (const auto& deletedPath : deletedFiles) {
        CORVUS_CORE_INFO("Detected file deletion: {}", deletedPath);

        auto it = pathToID.find(deletedPath);
        if (it != pathToID.end()) {
            UUID deletedId = it->second;

            // Remove metadata
            metadata.erase(deletedId);
            pathToID.erase(it);
            fileModificationTimes.erase(deletedPath);

            // Delete associated .meta file
            std::string metaPath = stripLeadingSlash(getMetaFilePath(deletedPath));
            if (PHYSFS_delete(metaPath.c_str())) {
                CORVUS_CORE_INFO("Deleted orphaned meta file: {}", metaPath);
            } else {
                const char* errMsg = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
                CORVUS_CORE_WARN("Failed to delete meta file: {} - {}", metaPath, errMsg);
            }

            // Unload the asset if loaded
            assets.erase(deletedId);
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
    CORVUS_CORE_INFO("File watcher started ({} ms poll interval)", pollIntervalMs);
}

bool AssetManager::fileWatcherRunning() const { return watcherRunning; }

void AssetManager::stopFileWatcher() {
    if (!watcherRunning.load())
        return;

    watcherRunning = false;
    if (watcherThread.joinable()) {
        watcherThread.join();
    }
    CORVUS_CORE_INFO("File watcher stopped");
}

void AssetManager::onAssetReloaded(std::function<void(const UUID&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(assetMutex);
    assetReloadedCallbacks.push_back(std::move(callback));
}

// Queries
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

bool AssetManager::hasAsset(const UUID& id) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    return metadata.find(id) != metadata.end();
}

bool AssetManager::reloadAsset(const UUID& id) {
    std::lock_guard<std::mutex> lock(assetMutex);

    auto it = assets.find(id);
    if (it == assets.end())
        return false;

    auto& entry = it->second;
    if (!entry.loader) {
        CORVUS_CORE_WARN("Asset {} has no loader, cannot reload", entry.path);
        return false;
    }

    CORVUS_CORE_INFO("Reloading asset: {}", entry.path);

    // Load fresh data
    void* newData = entry.loader->load(toPhysFS(entry.path));
    if (!newData) {
        CORVUS_CORE_ERROR("Failed to reload asset {}", entry.path);
        return false;
    }

    if (entry.data) {
        entry.loader->reloadTyped(entry.data.get(), newData);
    } else {
        // No previous data,normal path
        entry.data.reset(newData, [this, id = entry.id, loader = entry.loader](void* ptr) {
            loader->unload(ptr);
            decrementRef(id);
        });
    }

    entry.lastModified = getFileModTime(entry.path);
    return true;
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
            case AssetType::Material:
                name = "Material";
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

}
