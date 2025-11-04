#pragma once
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/material.hpp"
#include "corvus/renderer/mesh.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace Corvus::Renderer {

using Graphics::GraphicsContext;
using Graphics::Shader;
using Graphics::Texture2D;

class ResourceManager {
public:
    explicit ResourceManager(GraphicsContext& context);

    // Shader management
    std::shared_ptr<Shader> loadShader(const std::string& name,
                                       const std::string& vertexSource,
                                       const std::string& fragmentSource);
    std::shared_ptr<Shader> getShader(const std::string& name);

    // Texture management
    std::shared_ptr<Texture2D>
    loadTexture(const std::string& name, const void* data, uint32_t width, uint32_t height);
    std::shared_ptr<Texture2D> getTexture(const std::string& name);

    // Material management
    MaterialRef createMaterial(const std::string& name, const std::string& shaderName);
    MaterialRef getMaterial(const std::string& name);

    // Mesh management (for procedural/cached meshes)
    std::shared_ptr<Mesh> createMesh(const std::string& name, const Mesh& mesh);
    std::shared_ptr<Mesh> getMesh(const std::string& name);

    // Clear resources
    void clear();

private:
    GraphicsContext&                                            context_;
    std::unordered_map<std::string, std::shared_ptr<Shader>>    shaders_;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> textures_;
    std::unordered_map<std::string, MaterialRef>                materials_;
    std::unordered_map<std::string, std::shared_ptr<Mesh>>      meshes_;
};

}
