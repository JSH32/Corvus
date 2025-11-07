#pragma once
#include "corvus/asset/asset_manager.hpp"
#include "corvus/log.hpp"
#include "material.hpp"
#include <cereal/archives/json.hpp>
#include <physfs.h>
#include <sstream>

namespace Corvus::Core {

class MaterialLoader final : public AssetLoader<MaterialAsset> {
public:
    MaterialAsset* loadTyped(const std::string& path) override {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            CORVUS_CORE_ERROR("Failed to open material file: {}", path);
            return nullptr;
        }

        PHYSFS_sint64     fileSize = PHYSFS_fileLength(file);
        std::vector<char> buffer(fileSize);
        if (PHYSFS_readBytes(file, buffer.data(), fileSize) != fileSize) {
            CORVUS_CORE_ERROR("Failed to read material file: {}", path);
            PHYSFS_close(file);
            return nullptr;
        }
        PHYSFS_close(file);

        try {
            std::istringstream       iss(std::string(buffer.begin(), buffer.end()));
            cereal::JSONInputArchive ar(iss);

            auto* material = new MaterialAsset();
            ar(cereal::make_nvp("material", *material));

            std::string filename = path.substr(path.find_last_of('/') + 1);
            CORVUS_CORE_INFO("Loaded material: {}", filename);

            return material;
        } catch (const std::exception& e) {
            CORVUS_CORE_ERROR("Failed to parse material file {}: {}", path, e.what());
            return nullptr;
        }
    }

    bool saveTyped(const MaterialAsset* material, const std::string& path) override {
        if (!material) {
            CORVUS_CORE_ERROR("Cannot save null material");
            return false;
        }

        try {
            std::ostringstream oss;
            {
                cereal::JSONOutputArchive ar(oss);
                ar(cereal::make_nvp("material", *material));
            }

            std::string data = oss.str();

            // Remove mount prefix for PHYSFS
            std::string writePath = path;
            size_t      slashPos  = writePath.find('/');
            if (slashPos != std::string::npos)
                writePath = writePath.substr(slashPos + 1);

            // Create dir if needed
            auto lastSlash = writePath.find_last_of('/');
            if (lastSlash != std::string::npos)
                PHYSFS_mkdir(writePath.substr(0, lastSlash).c_str());

            PHYSFS_File* file = PHYSFS_openWrite(writePath.c_str());
            if (!file) {
                CORVUS_CORE_ERROR("Failed to open material for write: {}", writePath);
                return false;
            }

            PHYSFS_sint64 written = PHYSFS_writeBytes(file, data.c_str(), data.size());
            PHYSFS_close(file);

            if (written != static_cast<PHYSFS_sint64>(data.size())) {
                CORVUS_CORE_ERROR("Failed to write complete material data: {}", path);
                return false;
            }

            std::string filename = path.substr(path.find_last_of('/') + 1);
            CORVUS_CORE_INFO("Material saved: {} ({} bytes)", filename, data.size());
            return true;
        } catch (const std::exception& e) {
            CORVUS_CORE_ERROR("Failed to save material {}: {}", path, e.what());
            return false;
        }
    }

    bool canCreate() const override { return true; }

    MaterialAsset* createTyped(const std::string& name) override {
        auto*       mat      = new MaterialAsset();
        std::string filename = name.empty() ? "NewMaterial" : name;
        CORVUS_CORE_INFO("Created new material asset: {}", filename);
        return mat;
    }

    void unloadTyped(MaterialAsset* mat) override {
        if (!mat)
            return;

        delete mat;
    }

    void reloadTyped(MaterialAsset* existing, MaterialAsset* fresh) override {
        if (!existing || !fresh)
            return;

        *existing = std::move(*fresh);

        CORVUS_CORE_INFO("Reloaded material asset (shader {}, {} properties)",
                         !existing->getShaderAsset().is_nil()
                             ? boost::uuids::to_string(existing->getShaderAsset())
                             : "none",
                         existing->getPropertyCount());
    }

    AssetType getType() const override { return AssetType::Material; }
};

}
