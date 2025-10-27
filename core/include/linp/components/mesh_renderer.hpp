#pragma once
#include "cereal/cereal.hpp"
#include "cereal/types/string.hpp"
#include "component_registry.hpp"
#include "linp/asset/asset_handle.hpp"
#include "linp/asset/asset_manager.hpp"
#include "linp/asset/material/material.hpp"
#include "raylib-cpp.hpp"
#include "raylib.h"
#include <memory>

namespace Linp::Core::Components {

enum class PrimitiveType {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Model,
};

struct MeshRendererComponent {
    PrimitiveType primitiveType = PrimitiveType::Cube;

    // Asset references
    AssetHandle<raylib::Model> modelHandle;
    AssetHandle<Material>      materialHandle;

    // Cached generated model for primitives
    std::unique_ptr<raylib::Model> generatedModel;
    bool                           hasGeneratedModel = false;

    // Primitive parameters
    union {
        struct {
            float size;
        } cube;
        struct {
            float radius;
            int   rings;
            int   slices;
        } sphere;
        struct {
            float width;
            float length;
        } plane;
        struct {
            float radius;
            float height;
            int   slices;
        } cylinder;
    } params;

    MeshRendererComponent() {
        memset(&params, 0, sizeof(params));
        params.cube.size = 1.0f;
        generateModel();
    }

    ~MeshRendererComponent() {
        generatedModel.reset();
        hasGeneratedModel = false;
    }

    MeshRendererComponent(const MeshRendererComponent&)            = delete;
    MeshRendererComponent& operator=(const MeshRendererComponent&) = delete;

    MeshRendererComponent(MeshRendererComponent&& other) noexcept { *this = std::move(other); }

    MeshRendererComponent& operator=(MeshRendererComponent&& other) noexcept {
        if (this != &other) {
            generatedModel          = std::move(other.generatedModel);
            hasGeneratedModel       = other.hasGeneratedModel;
            primitiveType           = other.primitiveType;
            modelHandle             = std::move(other.modelHandle);
            materialHandle          = std::move(other.materialHandle);
            params                  = other.params;
            other.hasGeneratedModel = false;
        }
        return *this;
    }

    template <class Archive>
    void serialize(Archive& ar) {
        int primitiveTypeInt = static_cast<int>(primitiveType);
        ar(CEREAL_NVP(primitiveTypeInt));
        if constexpr (Archive::is_loading::value)
            primitiveType = static_cast<PrimitiveType>(primitiveTypeInt);

        ar(cereal::make_nvp("model_handle", modelHandle));
        ar(cereal::make_nvp("material_handle", materialHandle));

        switch (primitiveType) {
            case PrimitiveType::Cube:
                ar(CEREAL_NVP(params.cube.size));
                break;
            case PrimitiveType::Sphere:
                ar(CEREAL_NVP(params.sphere.radius),
                   CEREAL_NVP(params.sphere.rings),
                   CEREAL_NVP(params.sphere.slices));
                break;
            case PrimitiveType::Plane:
                ar(CEREAL_NVP(params.plane.width), CEREAL_NVP(params.plane.length));
                break;
            case PrimitiveType::Cylinder:
                ar(CEREAL_NVP(params.cylinder.radius),
                   CEREAL_NVP(params.cylinder.height),
                   CEREAL_NVP(params.cylinder.slices));
                break;
            default:
                break;
        }

        if constexpr (Archive::is_loading::value)
            generateModel();
    }

    void generateModel() {
        generatedModel.reset();
        hasGeneratedModel = false;

        Mesh mesh = { 0 };

        switch (primitiveType) {
            case PrimitiveType::Cube:
                mesh = GenMeshCube(params.cube.size, params.cube.size, params.cube.size);
                break;
            case PrimitiveType::Sphere:
                mesh = GenMeshSphere(
                    params.sphere.radius, params.sphere.rings, params.sphere.slices);
                break;
            case PrimitiveType::Plane:
                mesh = GenMeshPlane(params.plane.width, params.plane.length, 1, 1);
                break;
            case PrimitiveType::Cylinder:
                mesh = GenMeshCylinder(
                    params.cylinder.radius, params.cylinder.height, params.cylinder.slices);
                break;
            default:
                return;
        }

        generatedModel    = std::make_unique<raylib::Model>(LoadModelFromMesh(mesh));
        hasGeneratedModel = true;
    }

    raylib::Model* getModel(AssetManager* mgr) {
        static raylib::Model missingModel = [] {
            Mesh          m                                  = GenMeshCube(1.0f, 1.0f, 1.0f);
            raylib::Model mdl                                = LoadModelFromMesh(m);
            mdl.materials[0].maps[MATERIAL_MAP_ALBEDO].color = Color { 255, 0, 255, 255 };
            return mdl;
        }();

        if (primitiveType == PrimitiveType::Model) {
            if (mgr)
                modelHandle.setAssetManager(mgr);
            if (modelHandle.isValid()) {
                auto ptr = modelHandle.get();
                if (ptr)
                    return ptr.get();
            }
            return &missingModel;
        }

        return generatedModel ? generatedModel.get() : &missingModel;
    }

    Material* getMaterial(AssetManager* mgr) {
        static std::unique_ptr<Material> fallbackMat;
        if (!fallbackMat) {
            fallbackMat = std::make_unique<Material>();
            fallbackMat->setColor("_MainColor", Color { 255, 0, 255, 255 }); // magenta
            fallbackMat->loadDefaultShader();
        }

        if (mgr)
            materialHandle.setAssetManager(mgr);

        if (materialHandle.isValid() && materialHandle.isLoaded()) {
            return materialHandle.get().get();
        }

        return fallbackMat.get();
    }
};

REGISTER_COMPONENT(MeshRendererComponent, "MeshRenderer");

}
