#pragma once

#include "asset_handle.hpp"
#include "asset_manager.hpp"
#include "linp/log.hpp"
#include "linp/scene.hpp"
#include <cereal/archives/json.hpp>
#include <physfs.h>
#include <sstream>

namespace Linp::Core {

class SceneLoader : public AssetLoader<Scene> {
public:
    Scene* loadTyped(const std::string& path) override {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            LINP_CORE_ERROR("Failed to open scene file: {}", path);
            return nullptr;
        }

        PHYSFS_sint64     fileSize = PHYSFS_fileLength(file);
        std::vector<char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);

        try {
            std::istringstream       iss(std::string(buffer.begin(), buffer.end()));
            cereal::JSONInputArchive ar(iss);

            Scene* scene = new Scene("Loading...", getAssetManager());
            ar(cereal::make_nvp("scene", *scene));

            LINP_CORE_INFO("Loaded scene: {}", scene->name);
            return scene;
        } catch (const std::exception& e) {
            LINP_CORE_ERROR("Failed to parse scene file {}: {}", path, e.what());
            return nullptr;
        }
    }

    bool saveTyped(const Scene* scene, const std::string& path) override {
        if (!scene) {
            LINP_CORE_ERROR("Cannot save null scene");
            return false;
        }

        try {
            std::ostringstream oss;
            {
                cereal::JSONOutputArchive ar(oss);
                ar(cereal::make_nvp("scene", *scene));
            }
            std::string data = oss.str();

            // path is already the correct PhysFS path (e.g., "project/scenes/Hello.scene")
            // For writes, we need to strip the mount alias prefix
            std::string writePath = path;
            size_t      slashPos  = writePath.find('/');
            if (slashPos != std::string::npos) {
                writePath = writePath.substr(slashPos + 1);
            }

            auto lastSlash = writePath.find_last_of('/');
            if (lastSlash != std::string::npos) {
                PHYSFS_mkdir(writePath.substr(0, lastSlash).c_str());
            }

            PHYSFS_File* file = PHYSFS_openWrite(writePath.c_str());
            if (!file) {
                LINP_CORE_ERROR("Failed to open scene for write: {}", writePath);
                return false;
            }

            PHYSFS_sint64 written = PHYSFS_writeBytes(file, data.c_str(), data.size());
            PHYSFS_close(file);

            if (written != static_cast<PHYSFS_sint64>(data.size())) {
                LINP_CORE_ERROR("Failed to write complete scene data: {}", path);
                return false;
            }

            LINP_CORE_INFO("Scene saved: {} ({} bytes)", path, data.size());
            return true;
        } catch (const std::exception& e) {
            LINP_CORE_ERROR("Failed to save scene {}: {}", path, e.what());
            return false;
        }
    }

    bool canCreate() const override { return true; }

    Scene* createTyped(const std::string& name) override {
        auto* scene = new Scene(name.empty() ? "New Scene" : name, getAssetManager());
        LINP_CORE_INFO("Created new scene asset: {}", scene->name);
        return scene;
    }

    void unloadTyped(Scene* scene) override {
        if (scene) {
            delete scene;
        }
    }

    AssetType getType() const override { return AssetType::Scene; }
};

}
