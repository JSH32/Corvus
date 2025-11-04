#pragma once
#include "corvus/renderer/material.hpp"
#include "corvus/renderer/mesh.hpp"
#include "corvus/renderer/model.hpp"
#include "corvus/renderer/transform.hpp"
#include <memory>

namespace Corvus::Renderer {

struct Renderable {
    Model*    model     = nullptr;
    Material* material  = nullptr;
    glm::mat4 transform = glm::mat4(1.0f);
    bool      wireframe = false;
    bool      enabled   = true;

    // Optional: for culling/lighting
    glm::vec3 position       = glm::vec3(0.0f);
    float     boundingRadius = 1.0f;
};

}
