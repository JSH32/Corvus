#pragma once
#include "linp/asset/asset_manager.hpp"
#include "linp/log.hpp"
#include "material.hpp"
#include <cereal/archives/json.hpp>
#include <physfs.h>
#include <sstream>

namespace Linp::Core {

class MaterialLoader : public AssetLoader<Material> {
public:
    Material* loadTyped(const std::string& path) override {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            LINP_CORE_ERROR("Failed to open material file: {}", path);
            return nullptr;
        }

        PHYSFS_sint64     fileSize = PHYSFS_fileLength(file);
        std::vector<char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);

        try {
            std::istringstream       iss(std::string(buffer.begin(), buffer.end()));
            cereal::JSONInputArchive ar(iss);

            Material* material = new Material();
            ar(cereal::make_nvp("material", *material));

            // Extract filename from path for logging
            std::string filename = path.substr(path.find_last_of('/') + 1);
            LINP_CORE_INFO("Loaded material: {}", filename);

            return material;
        } catch (const std::exception& e) {
            LINP_CORE_ERROR("Failed to parse material file {}: {}", path, e.what());
            return nullptr;
        }
    }

    bool saveTyped(const Material* material, const std::string& path) override {
        if (!material) {
            LINP_CORE_ERROR("Cannot save null material");
            return false;
        }

        try {
            std::ostringstream oss;
            {
                cereal::JSONOutputArchive ar(oss);
                ar(cereal::make_nvp("material", *material));
            }

            std::string data      = oss.str();
            std::string writePath = path;

            // Remove mount point prefix if present
            size_t slashPos = writePath.find('/');
            if (slashPos != std::string::npos) {
                writePath = writePath.substr(slashPos + 1);
            }

            // Create directory if needed
            auto lastSlash = writePath.find_last_of('/');
            if (lastSlash != std::string::npos) {
                PHYSFS_mkdir(writePath.substr(0, lastSlash).c_str());
            }

            PHYSFS_File* file = PHYSFS_openWrite(writePath.c_str());
            if (!file) {
                LINP_CORE_ERROR("Failed to open material for write: {}", writePath);
                return false;
            }

            PHYSFS_sint64 written = PHYSFS_writeBytes(file, data.c_str(), data.size());
            PHYSFS_close(file);

            if (written != static_cast<PHYSFS_sint64>(data.size())) {
                LINP_CORE_ERROR("Failed to write complete material data: {}", path);
                return false;
            }

            // Extract filename from path for logging
            std::string filename = path.substr(path.find_last_of('/') + 1);
            LINP_CORE_INFO("Material saved: {} ({} bytes)", filename, data.size());

            return true;
        } catch (const std::exception& e) {
            LINP_CORE_ERROR("Failed to save material {}: {}", path, e.what());
            return false;
        }
    }

    bool canCreate() const override { return true; }

    Material* createTyped(const std::string& name) override {
        auto* material = new Material();

        std::string filename = name.empty() ? "NewMaterial" : name;
        LINP_CORE_INFO("Created new material asset: {}", filename);

        return material;
    }

    void unloadTyped(Material* material) override {
        if (material) {
            delete material;
        }
    }

    AssetType getType() const override { return AssetType::Material; }
};

}
