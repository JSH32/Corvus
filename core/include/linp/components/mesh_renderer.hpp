#pragma once
#include "cereal/cereal.hpp"
#include "component_registry.hpp"
#include "raylib-cpp.hpp"
#include "raylib.h"
#include <cereal/archives/json.hpp>
#include <memory>

namespace Linp::Core::Components {

enum class PrimitiveType {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Custom
};

struct MeshRendererComponent : public Linp::Core::Components::SerializableComponent<MeshRendererComponent> {
    PrimitiveType                     primitiveType = PrimitiveType::Cube;
    std::shared_ptr<raylib::Mesh>     mesh;
    std::shared_ptr<raylib::Material> material;

    // Primitive parameters (only used for built-in types)
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

    MeshRendererComponent() : SerializableComponent("MeshRenderer") {
        params.cube.size = 1.0f;
        material         = std::make_shared<raylib::Material>(LoadMaterialDefault());
        generateMesh();
    }

    template <class Archive>
    void serialize(Archive& ar) {
        int primitiveTypeInt = static_cast<int>(primitiveType);
        ar(CEREAL_NVP(primitiveTypeInt));

        // Serialize all union data
        ar(cereal::make_nvp("cube_size", params.cube.size));
        ar(cereal::make_nvp("sphere_radius", params.sphere.radius));
        ar(cereal::make_nvp("sphere_rings", params.sphere.rings));
        ar(cereal::make_nvp("sphere_slices", params.sphere.slices));
        ar(cereal::make_nvp("plane_width", params.plane.width));
        ar(cereal::make_nvp("plane_length", params.plane.length));
        ar(cereal::make_nvp("cylinder_radius", params.cylinder.radius));
        ar(cereal::make_nvp("cylinder_height", params.cylinder.height));
        ar(cereal::make_nvp("cylinder_slices", params.cylinder.slices));

        // After deserialization, regenerate GPU resources
        if constexpr (Archive::is_loading::value) {
            primitiveType = static_cast<PrimitiveType>(primitiveTypeInt);
            material      = std::make_shared<raylib::Material>(LoadMaterialDefault());
            generateMesh();
        }
    }

    void generateMesh() {
        switch (primitiveType) {
            case PrimitiveType::Cube:
                mesh = std::make_shared<raylib::Mesh>(GenMeshCube(params.cube.size, params.cube.size, params.cube.size));
                break;
            case PrimitiveType::Sphere:
                mesh = std::make_shared<raylib::Mesh>(GenMeshSphere(params.sphere.radius, params.sphere.rings, params.sphere.slices));
                break;
            case PrimitiveType::Plane:
                mesh = std::make_shared<raylib::Mesh>(GenMeshPlane(params.plane.width, params.plane.length, 1, 1));
                break;
            case PrimitiveType::Cylinder:
                mesh = std::make_shared<raylib::Mesh>(GenMeshCylinder(params.cylinder.radius, params.cylinder.height, params.cylinder.slices));
                break;
            case PrimitiveType::Custom:
                // Don't regenerate custom meshes
                break;
        }
    }
};

}
