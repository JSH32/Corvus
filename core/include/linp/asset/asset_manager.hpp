#pragma once
#include "asset_handle.hpp"
#include "linp/log.hpp"
#include <atomic>
#include <boost/functional/hash.hpp>
#include <cereal/archives/json.hpp>
#include <fstream>
#include <functional>
#include <mutex>
#include <physfs.h>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Linp::Core {

class IAssetLoader {
private:
    AssetManager* assetManager = nullptr;

public:
    virtual ~IAssetLoader()                         = default;
    virtual void*     load(const std::string& path) = 0;
    virtual void      unload(void* asset)           = 0;
    virtual AssetType getType() const               = 0;
    virtual bool      save(void* asset, const std::string& path) { return false; }

    virtual bool  canCreate() const { return false; }
    virtual void* create(const std::string& name) { return nullptr; }

    virtual void  setAssetManager(AssetManager* mgr) { assetManager = mgr; }
    AssetManager* getAssetManager() const { return assetManager; }
};

template <typename T>
class AssetLoader : public IAssetLoader {
public:
    void* load(const std::string& path) override { return loadTyped(path); }
    void  unload(void* asset) override { unloadTyped(static_cast<T*>(asset)); }
    bool  save(void* asset, const std::string& path) override {
        return saveTyped(static_cast<T*>(asset), path);
    }

    virtual T*   createTyped(const std::string& name) { return nullptr; }
    virtual T*   loadTyped(const std::string& path) = 0;
    virtual void unloadTyped(T* asset)              = 0;
    virtual bool saveTyped(const T* asset, const std::string& path) { return false; }

    virtual bool canCreate() const override { return false; }
    void* create(const std::string& name) final { return static_cast<void*>(createTyped(name)); }
};

struct AssetMetadata {
    UUID        id;
    std::string path; // Internal format (with leading slash)
    AssetType   type;
    uint64_t    lastModified;

    template <class Archive>
    void serialize(Archive& ar) {
        std::string uuidStr;
        if constexpr (Archive::is_saving::value) {
            uuidStr = boost::uuids::to_string(id);
        }
        ar(cereal::make_nvp("id", uuidStr));
        if constexpr (Archive::is_loading::value) {
            id = boost::uuids::string_generator()(uuidStr);
        }

        int typeInt = static_cast<int>(type);
        ar(cereal::make_nvp("path", path));
        ar(cereal::make_nvp("type", typeInt));
        ar(cereal::make_nvp("lastModified", lastModified));

        if constexpr (Archive::is_loading::value) {
            type = static_cast<AssetType>(typeInt);
        }
    }
};

struct AssetEntry {
    UUID                  id;
    std::string           path; // Internal format (with leading slash)
    AssetType             type;
    std::type_index       typeIndex;
    std::shared_ptr<void> data;
    IAssetLoader*         loader;
    int                   refCount;
    uint64_t              lastModified;

    AssetEntry()
        : id(), path(), type(AssetType::Unknown), typeIndex(typeid(void)), data(nullptr),
          loader(nullptr), refCount(0), lastModified(0) { }

    AssetEntry(const AssetEntry&)            = default;
    AssetEntry(AssetEntry&&)                 = default;
    AssetEntry& operator=(const AssetEntry&) = default;
    AssetEntry& operator=(AssetEntry&&)      = default;
};

class AssetManager {
private:
    std::string projectPath;
    std::string physfsAlias;

    std::unordered_map<UUID, AssetEntry, boost::hash<UUID>>    assets;
    std::unordered_map<std::string, UUID>                      pathToID; // Keys are internal format
    std::unordered_map<UUID, AssetMetadata, boost::hash<UUID>> metadata;

    std::unordered_map<std::type_index, std::unique_ptr<IAssetLoader>> loaders;
    std::unordered_map<std::string, std::type_index>                   extensionToType;

    std::atomic<bool>                         watcherRunning;
    std::atomic<bool>                         shuttingDown;
    std::thread                               watcherThread;
    mutable std::mutex                        assetMutex;
    std::unordered_map<std::string, uint64_t> fileModificationTimes; // Keys are internal format

    std::vector<std::function<void(const UUID&, const std::string&)>> assetReloadedCallbacks;

    void     fileWatcherLoop(int pollIntervalMs);
    void     checkFileChanges();
    uint64_t getFileModTime(const std::string& internalPath);

    std::string getMetaFilePath(const std::string& assetPath);
    bool        loadMetaFile(const std::string& internalPath, AssetMetadata& outMeta);
    bool        saveMetaFile(const std::string& internalPath, const AssetMetadata& meta);

    void        scanDirectory(const std::string& internalPath);
    std::string getFileExtension(const std::string& path);

    void incrementRef(const UUID& id);
    void decrementRef(const UUID& id);

    // Path conversion helpers
    std::string toPhysFS(const std::string& userPath) const;   // user -> PhysFS with prefix
    std::string toInternal(const std::string& userPath) const; // user -> internal (leading slash)
    std::string getFullPath(const std::string& relativePath) const; // alias for toInternal
    void        setupRaylibBridge();

    bool copyDirectoryRecursive(const std::string& srcInternal, const std::string& dstInternal);
    bool deleteDirectoryRecursive(const std::string& internalPath, bool untrackAssets = true);

public:
    AssetManager(const std::string& projectPath, const std::string& alias = "project");
    ~AssetManager();

    AssetManager(const AssetManager&)            = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    std::vector<std::string>   getDirectories(const std::string& userPath = "") const;
    std::vector<AssetMetadata> getAssetsInDirectory(const std::string& userPath = "") const;
    bool                       createDirectory(const std::string& userPath);
    bool                       deleteDirectory(const std::string& userPath);

    std::vector<std::pair<std::string, AssetType>> getCreatableAssetTypes() const;

    bool
    createAssetByType(AssetType type, const std::string& relativePath, const std::string& name);

    template <typename T>
    AssetHandle<T> createAsset(const std::string& relativePath, const std::string& name) {
        std::type_index idx = typeid(T);
        auto            it  = loaders.find(idx);
        if (it == loaders.end() || !it->second->canCreate()) {
            LINP_CORE_ERROR("Loader for {} cannot create new assets", typeid(T).name());
            return {};
        }

        // Ensure path has correct extension
        std::string finalPath  = relativePath;
        std::string currentExt = getFileExtension(relativePath);

        std::string expectedExt;
        for (const auto& [extension, typeIdx] : extensionToType) {
            if (typeIdx == idx) {
                expectedExt = extension;
                break;
            }
        }

        // Only append if no extension exists OR extension exists but doesn't match expected
        // extension
        if (currentExt.empty()) {
            finalPath = relativePath + expectedExt;
        } else if (currentExt != expectedExt) {
            // Wrong extension, replace it
            size_t lastDot = finalPath.find_last_of('.');
            if (lastDot != std::string::npos) {
                finalPath = finalPath.substr(0, lastDot) + expectedExt;
            }
        }

        T* obj = static_cast<T*>(it->second->create(name));
        if (!obj)
            return {};
        auto handle = addAsset(finalPath, std::move(*obj));
        handle.save();
        return handle;
    }

    template <typename T, typename LoaderT>
    void registerLoader(const std::vector<std::string>& extensions) {
        static_assert(std::is_base_of_v<AssetLoader<T>, LoaderT>,
                      "Loader must inherit from AssetLoader<T>");

        auto loader = std::make_unique<LoaderT>();
        loader->setAssetManager(this);
        std::type_index typeIdx = typeid(T);

        for (const auto& ext : extensions) {
            extensionToType.insert_or_assign(ext, typeIdx);
        }

        loaders.emplace(typeIdx, std::move(loader));
    }

    AssetType getAssetTypeFromExtension(const std::string& ext);
    void      scanAssets(const std::string& subDirectory = "", bool recursive = true);

    bool deleteAsset(const UUID& id);
    bool moveAsset(const UUID& id, const std::string& newUserPath);
    bool copyAsset(const UUID& id, const std::string& newUserPath, bool includeMeta = true);
    bool renameDirectory(const std::string& oldUserPath, const std::string& newUserPath);
    bool hasAsset(const UUID& id) const;

    template <typename T>
    AssetHandle<T> load(const std::string& userPath) {
        std::lock_guard<std::mutex> lock(assetMutex);

        std::string internalPath = toInternal(userPath);

        auto pathIt = pathToID.find(internalPath);
        if (pathIt == pathToID.end()) {
            AssetMetadata meta;
            if (loadMetaFile(internalPath, meta)) {
                pathToID[internalPath] = meta.id;
                metadata[meta.id]      = meta;
            } else {
                meta.id           = boost::uuids::random_generator()();
                meta.path         = internalPath;
                meta.type         = getAssetTypeFromExtension(getFileExtension(internalPath));
                meta.lastModified = getFileModTime(internalPath);

                saveMetaFile(internalPath, meta);
                pathToID[internalPath] = meta.id;
                metadata[meta.id]      = meta;
            }

            pathIt = pathToID.find(internalPath);
        }

        UUID id = pathIt->second;
        return AssetHandle<T>(id, internalPath, this);
    }

    template <typename T>
    bool saveAsset(const AssetHandle<T>& handle) {
        std::lock_guard<std::mutex> lock(assetMutex);

        auto it = assets.find(handle.getID());
        if (it == assets.end())
            return false;

        auto metaIt = metadata.find(handle.getID());
        if (metaIt == metadata.end())
            return false;

        auto loaderIt = loaders.find(it->second.typeIndex);
        if (loaderIt == loaders.end())
            return false;

        if (!it->second.data)
            return false;

        // Loader receives PhysFS path (with prefix)
        return loaderIt->second->save(it->second.data.get(), toPhysFS(metaIt->second.path));
    }

    template <typename T>
    AssetHandle<T> addAsset(const std::string& relativePath, T&& asset) {
        std::lock_guard<std::mutex> lock(assetMutex);

        std::string internalPath = toInternal(relativePath);

        // Create metadata
        AssetMetadata meta;
        meta.id           = boost::uuids::random_generator()();
        meta.path         = internalPath;
        meta.type         = getAssetTypeFromExtension(getFileExtension(internalPath));
        meta.lastModified = getFileModTime(internalPath);

        saveMetaFile(internalPath, meta);
        pathToID[internalPath] = meta.id;
        metadata[meta.id]      = meta;

        // Get loader
        std::type_index typeIdx  = typeid(T);
        auto            loaderIt = loaders.find(typeIdx);
        if (loaderIt == loaders.end()) {
            LINP_CORE_ERROR("No loader registered for type {}", typeIdx.name());
            return {};
        }
        IAssetLoader* loader = loaderIt->second.get();

        // Wrap in shared_ptr for managed lifecycle
        auto deleter = [this, id = meta.id, loader](T* ptr) {
            loader->unload(ptr);
            decrementRef(id);
        };
        auto assetPtr = std::shared_ptr<T>(new T(std::forward<T>(asset)), deleter);

        // Insert into cache
        AssetEntry entry;
        entry.id           = meta.id;
        entry.path         = meta.path;
        entry.type         = meta.type;
        entry.typeIndex    = typeIdx;
        entry.data         = std::static_pointer_cast<void>(assetPtr);
        entry.loader       = loader;
        entry.refCount     = 1;
        entry.lastModified = meta.lastModified;
        assets.emplace(meta.id, std::move(entry));

        LINP_CORE_INFO("Registered new asset: {}", internalPath);
        return AssetHandle<T>(meta.id, internalPath, this);
    }

    template <typename T>
    AssetHandle<T> loadByID(const UUID& id) {
        std::lock_guard<std::mutex> lock(assetMutex);

        auto metaIt = metadata.find(id);
        if (metaIt == metadata.end()) {
            return AssetHandle<T>();
        }

        return AssetHandle<T>(id, metaIt->second.path, this);
    }

    template <typename T>
    std::vector<AssetHandle<T>> getAllOfType() {
        std::lock_guard<std::mutex> lock(assetMutex);
        std::vector<AssetHandle<T>> result;

        std::type_index typeIdx = typeid(T);

        for (const auto& [id, meta] : metadata) {
            auto it = extensionToType.find(getFileExtension(meta.path));
            if (it != extensionToType.end() && it->second == typeIdx) {
                result.emplace_back(id, meta.path, this);
            }
        }

        return result;
    }

    template <typename T>
    std::shared_ptr<T> loadAssetData(const UUID& id) {
        std::lock_guard<std::mutex> lock(assetMutex);

        std::type_index typeIdx = typeid(T);

        auto it = assets.find(id);
        if (it != assets.end() && it->second.typeIndex == typeIdx) {
            incrementRef(id);
            return std::static_pointer_cast<T>(it->second.data);
        }

        auto metaIt = metadata.find(id);
        if (metaIt == metadata.end()) {
            return nullptr;
        }

        const AssetMetadata& meta = metaIt->second;

        auto loaderIt = loaders.find(typeIdx);
        if (loaderIt == loaders.end()) {
            return nullptr;
        }

        // Loader receives PhysFS path (with prefix)
        T* rawAsset = static_cast<T*>(loaderIt->second->load(toPhysFS(meta.path)));
        if (!rawAsset) {
            return nullptr;
        }

        auto deleter = [this, id, loader = loaderIt->second.get()](T* ptr) {
            loader->unload(static_cast<void*>(ptr));
            decrementRef(id);
        };

        std::shared_ptr<T> assetPtr(rawAsset, deleter);

        AssetEntry entry;
        entry.id           = id;
        entry.path         = meta.path;
        entry.type         = meta.type;
        entry.typeIndex    = typeIdx;
        entry.data         = std::static_pointer_cast<void>(assetPtr);
        entry.loader       = loaderIt->second.get();
        entry.refCount     = 1;
        entry.lastModified = meta.lastModified;

        assets.insert_or_assign(id, std::move(entry));

        return assetPtr;
    }

    void unload(const UUID& id);
    void unloadUnused();
    void unloadAll();

    void startFileWatcher(int pollIntervalMs = 1000);
    void stopFileWatcher();

    void onAssetReloaded(std::function<void(const UUID&, const std::string&)> callback);

    AssetMetadata getMetadata(const UUID& id) const;
    int           getRefCount(const UUID& id) const;

    std::string getProjectPath() const { return projectPath; }
    std::string getPhysfsAlias() const { return physfsAlias; }

    template <typename T>
    friend class AssetHandle;
};

template <typename T>
void AssetHandle<T>::updateCache() const {
    if (!cachedPtr && isValid() && assetManager)
        cachedPtr = assetManager->template loadAssetData<T>(assetID);
}

template <typename T>
bool AssetHandle<T>::isLoaded() const {
    updateCache();
    return cachedPtr != nullptr;
}

template <typename T>
std::shared_ptr<T> AssetHandle<T>::get() const {
    updateCache();
    return cachedPtr;
}

template <typename T>
bool AssetHandle<T>::reload() {
    if (!assetManager || assetID.is_nil())
        return false;

    assetManager->unload(assetID);
    cachedPtr = assetManager->loadByID<T>(assetID).get();
    return cachedPtr != nullptr;
}

template <typename T>
bool AssetHandle<T>::isValid() const {
    if (!assetManager || assetID.is_nil())
        return false;
    return assetManager->hasAsset(assetID); // simple lookup in assetManager
}

template <typename T>
bool AssetHandle<T>::save() const {
    return assetManager && isValid() && assetManager->template saveAsset<T>(*this);
}
}
