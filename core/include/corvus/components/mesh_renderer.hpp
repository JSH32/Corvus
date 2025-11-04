#pragma once
#include "cereal/cereal.hpp"
#include "component_registry.hpp"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/asset/material/material.hpp"
#include "corvus/log.hpp"
#include "corvus/renderer/model.hpp"
#include "corvus/renderer/model_generator.hpp"
#include <memory>

namespace Corvus::Core::Components {

enum class PrimitiveType {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Model
};

struct MeshRendererComponent {
    PrimitiveType primitiveType = PrimitiveType::Cube;

    // Asset references
    AssetHandle<Renderer::Model> modelHandle;
    AssetHandle<MaterialAsset>   materialHandle;
    bool                         renderWireframe = false;

    // Cached generated primitive
    std::unique_ptr<Renderer::Model> generatedModel;
    bool                             hasGeneratedModel = false;

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
            primitiveType           = other.primitiveType;
            modelHandle             = std::move(other.modelHandle);
            materialHandle          = std::move(other.materialHandle);
            generatedModel          = std::move(other.generatedModel);
            hasGeneratedModel       = other.hasGeneratedModel;
            params                  = other.params;
            renderWireframe         = other.renderWireframe;
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
        ar(cereal::make_nvp("render_wireframe", renderWireframe));

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
    }

    void generateModel(Graphics::GraphicsContext& ctx) {
        generatedModel.reset();
        hasGeneratedModel = false;

        Renderer::Model model;
        switch (primitiveType) {
            case PrimitiveType::Cube:
                model = Renderer::ModelGenerator::createCube(ctx, params.cube.size);
                break;
            case PrimitiveType::Sphere:
                model = Renderer::ModelGenerator::createSphere(
                    ctx, params.sphere.radius, params.sphere.rings, params.sphere.slices);
                break;
            case PrimitiveType::Plane:
                model = Renderer::ModelGenerator::createPlane(
                    ctx, params.plane.width, params.plane.length);
                break;
            case PrimitiveType::Cylinder:
                model = Renderer::ModelGenerator::createCylinder(
                    ctx, params.cylinder.radius, params.cylinder.height, params.cylinder.slices);
                break;
            default:
                return;
        }

        generatedModel    = std::make_unique<Renderer::Model>(std::move(model));
        hasGeneratedModel = true;
    }

    Renderer::Model* getModel(AssetManager* mgr, Graphics::GraphicsContext* ctx) {
        static Renderer::Model* fallback = nullptr;

        if (!fallback && ctx) {
            auto cube = Renderer::ModelGenerator::createCube(*ctx, 1.0f);
            fallback  = new Renderer::Model(std::move(cube));
            CORVUS_CORE_INFO("Created fallback cube model");
        }

        if (primitiveType == PrimitiveType::Model) {
            if (mgr)
                modelHandle.setAssetManager(mgr);
            if (modelHandle.isValid()) {
                auto ptr = modelHandle.get();
                if (ptr)
                    return ptr.get();
            }
            return fallback;
        }

        if (!generatedModel && ctx)
            generateModel(*ctx);

        return generatedModel ? generatedModel.get() : fallback;
    }

    float getBoundingRadius() const {
        if (primitiveType == PrimitiveType::Model && modelHandle.isValid()) {
            auto model = modelHandle.get();
            if (model) {
                return model->getBoundingRadius();
            }
        }

        // Fallback to primitive parameters
        switch (primitiveType) {
            case PrimitiveType::Cube:
                return params.cube.size * 0.866f;
            case PrimitiveType::Sphere:
                return params.sphere.radius;
            case PrimitiveType::Plane:
                return std::max(params.plane.width, params.plane.length) * 0.5f;
            case PrimitiveType::Cylinder:
                return std::max(params.cylinder.radius, params.cylinder.height * 0.5f);
            default:
                return 1.0f;
        }
    }

    MaterialAsset* getMaterial(AssetManager* mgr) {
        static MaterialAsset* fallbackMat = nullptr;
        if (!fallbackMat) {
            fallbackMat = new MaterialAsset();
            fallbackMat->setVector4("_MainColor", glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
            fallbackMat->setFloat("_Metallic", 0.0f);
            fallbackMat->setFloat("_Smoothness", 0.5f);
            CORVUS_CORE_INFO("Created fallback material (magenta)");
        }

        if (mgr)
            materialHandle.setAssetManager(mgr);

        if (materialHandle.isValid() && materialHandle.isLoaded()) {
            auto matPtr = materialHandle.get();
            if (matPtr)
                return matPtr.get();
        }

        return fallbackMat;
    }
};

REGISTER_COMPONENT(MeshRendererComponent, "MeshRenderer");

}
