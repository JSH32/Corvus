#pragma once
#include "raylib-cpp.hpp"
#include "raylib.h"
#include <memory>

namespace Linp::Core::Components {

enum class PrimitiveType {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Custom
};

struct MeshRendererComponent {
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

    MeshRendererComponent() {
        params.cube.size = 1.0f;
        material         = std::make_shared<raylib::Material>(LoadMaterialDefault());
        generateMesh();
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
