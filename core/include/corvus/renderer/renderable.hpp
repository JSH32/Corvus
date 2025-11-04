#pragma once
#include "corvus/renderer/material.hpp"
#include "corvus/renderer/mesh.hpp"
#include "corvus/renderer/transform.hpp"
#include <memory>

namespace Corvus::Renderer {

class Renderable {
public:
    Renderable(const Mesh& mesh, MaterialRef material);

    // Transform
    Transform&       getTransform() { return transform_; }
    const Transform& getTransform() const { return transform_; }

    // Material
    void        setMaterial(MaterialRef material) { material_ = material; }
    MaterialRef getMaterial() const { return material_; }

    // Mesh
    const Mesh& getMesh() const { return mesh_; }

    // Visibility
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

    // Layer/mask for filtering
    void     setLayer(uint32_t layer) { layer_ = layer; }
    uint32_t getLayer() const { return layer_; }

private:
    Mesh        mesh_;
    MaterialRef material_;
    Transform   transform_;
    bool        visible_ = true;
    uint32_t    layer_   = 0; // Default layer
};

}
