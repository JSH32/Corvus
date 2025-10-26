#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cereal/cereal.hpp>
#include <string>
#include <memory>

namespace Linp::Core {

using UUID = boost::uuids::uuid;

enum class AssetType {
    Unknown,
    Texture,
    Model,
    Material,
    Shader,
    Audio,
    Script,
    Prefab,
    Scene,
    Font
};

class AssetManager;

/**
 * @brief Smart handle to an asset that automatically manages loading/unloading
 * and reference counting through the AssetManager.
 */
template<typename T>
class AssetHandle {
private:
    UUID assetID{};
    std::string path;
    mutable std::shared_ptr<T> cachedPtr;
    mutable AssetManager* assetManager = nullptr;

    void updateCache() const;

public:
    AssetHandle() = default;
    AssetHandle(const UUID& id, std::string p, AssetManager* mgr = nullptr)
        : assetID(id), path(std::move(p)), assetManager(mgr) {}

    bool isValid() const { return !assetID.is_nil(); }
    bool isLoaded() const;
    std::shared_ptr<T> get() const;
    bool save() const;

    explicit operator bool() const { return isValid() && isLoaded(); }
    T* operator->() const { return get().get(); }
    T& operator*() const { return *get(); }

    UUID getID() const { return assetID; }
    const std::string& getPath() const { return path; }
    void setAssetManager(AssetManager* mgr) { assetManager = mgr; }
};

}
