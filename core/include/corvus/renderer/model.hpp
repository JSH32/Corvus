#pragma once
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/mesh.hpp"
#include <memory>
#include <vector>

namespace Corvus::Renderer {

/**
 * A model is a collection of meshes that make up a 3D object.
 * Models can be loaded from files or procedurally generated.
 */
class Model {
public:
    Model()                            = default;
    Model(const Model&)                = delete;
    Model& operator=(const Model&)     = delete;
    Model(Model&&) noexcept            = default;
    Model& operator=(Model&&) noexcept = default;
    ~Model()                           = default;

    // Add a mesh to this model
    void addMesh(Mesh&& mesh) { meshes_.push_back(std::make_shared<Mesh>(std::move(mesh))); }

    // Get all meshes
    const std::vector<std::shared_ptr<Mesh>>& getMeshes() const { return meshes_; }

    std::vector<std::shared_ptr<Mesh>>& getMeshes() { return meshes_; }

    // Check if model has any meshes
    bool valid() const { return !meshes_.empty(); }

    // Draw all meshes
    void draw(Graphics::CommandBuffer& cmd, bool wireframe = false) const {
        for (const auto& mesh : meshes_) {
            if (mesh && mesh->valid()) {
                mesh->draw(cmd, wireframe);
            }
        }
    }

    // Calculate bounding radius for all meshes
    float getBoundingRadius() const {
        float maxRadius = 0.0f;
        for (const auto& mesh : meshes_) {
            if (mesh) {
                maxRadius = std::max(maxRadius, mesh->getBoundingRadius());
            }
        }
        return maxRadius > 0.0f ? maxRadius : 1.0f;
    }

    void release() {
        for (auto& mesh : meshes_) {
            if (mesh) {
                mesh->release();
            }
        }
        meshes_.clear();
    }

private:
    std::vector<std::shared_ptr<Mesh>> meshes_;
};

}
