#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cereal/cereal.hpp>
#include <memory>
#include <string>

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
template <typename T>
class AssetHandle {
private:
    UUID                       assetID {};
    std::string                path;
    mutable std::shared_ptr<T> cachedPtr;
    mutable AssetManager*      assetManager = nullptr;

    void updateCache() const;

public:
    AssetHandle() = default;
    AssetHandle(const UUID& id, const std::string& p = {}, AssetManager* mgr = nullptr)
        : assetID(id), path(p), assetManager(mgr) { }
    AssetHandle(const UUID& id, const char* p, AssetManager* mgr = nullptr)
        : assetID(id), path(p ? p : ""), assetManager(mgr) { }

    bool               isValid() const;
    bool               isLoaded() const;
    std::shared_ptr<T> get() const;
    bool               save() const;

    explicit operator bool() const { return isValid() && isLoaded(); }
    T*       operator->() const { return get().get(); }
    T&       operator*() const { return *get(); }

    UUID               getID() const { return assetID; }
    const std::string& getPath() const { return path; }
    void               setAssetManager(AssetManager* mgr) { assetManager = mgr; }

    bool reload();

    template <class Archive>
    void save(Archive& ar) const {
        std::string uuidStr;
        if (!assetID.is_nil())
            uuidStr = boost::uuids::to_string(assetID);

        ar(cereal::make_nvp("id", uuidStr));
    }

    template <class Archive>
    void load(Archive& ar) {
        std::string uuidStr;
        ar(cereal::make_nvp("id", uuidStr));

        if (!uuidStr.empty())
            assetID = boost::uuids::string_generator()(uuidStr);
        else
            assetID = UUID {};

        path.clear();
        cachedPtr.reset();
    }
};

}
