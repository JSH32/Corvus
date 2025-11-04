#include "corvus/renderer/resource_manager.hpp"

namespace Corvus::Renderer {

ResourceManager::ResourceManager(GraphicsContext& context) : context_(context) { }

std::shared_ptr<Shader> ResourceManager::loadShader(const std::string& name,
                                                    const std::string& vertexSource,
                                                    const std::string& fragmentSource) {

    auto it = shaders_.find(name);
    if (it != shaders_.end()) {
        return it->second;
    }

    auto shader = std::make_shared<Shader>(context_.createShader(vertexSource, fragmentSource));

    shaders_[name] = shader;
    return shader;
}

std::shared_ptr<Shader> ResourceManager::getShader(const std::string& name) {
    auto it = shaders_.find(name);
    if (it != shaders_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Texture2D> ResourceManager::loadTexture(const std::string& name,
                                                        const void*        data,
                                                        uint32_t           width,
                                                        uint32_t           height) {

    auto it = textures_.find(name);
    if (it != textures_.end()) {
        return it->second;
    }

    auto texture = std::make_shared<Texture2D>(context_.createTexture2D(width, height));

    texture->setData(data, width * height * 4);

    textures_[name] = texture;
    return texture;
}

std::shared_ptr<Texture2D> ResourceManager::getTexture(const std::string& name) {
    auto it = textures_.find(name);
    if (it != textures_.end()) {
        return it->second;
    }
    return nullptr;
}

MaterialRef ResourceManager::createMaterial(const std::string& name,
                                            const std::string& shaderName) {
    auto it = materials_.find(name);
    if (it != materials_.end()) {
        return it->second;
    }

    auto shader = getShader(shaderName);
    if (!shader) {
        return nullptr;
    }

    auto material    = std::make_shared<Material>(*shader);
    materials_[name] = material;
    return material;
}

MaterialRef ResourceManager::getMaterial(const std::string& name) {
    auto it = materials_.find(name);
    if (it != materials_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Mesh> ResourceManager::createMesh(const std::string& name, const Mesh& mesh) {
    auto it = meshes_.find(name);
    if (it != meshes_.end()) {
        return it->second;
    }

    auto meshPtr  = std::make_shared<Mesh>(mesh);
    meshes_[name] = meshPtr;
    return meshPtr;
}

std::shared_ptr<Mesh> ResourceManager::getMesh(const std::string& name) {
    auto it = meshes_.find(name);
    if (it != meshes_.end()) {
        return it->second;
    }
    return nullptr;
}

void ResourceManager::clear() {
    materials_.clear();
    shaders_.clear();
    textures_.clear();
    meshes_.clear();
}

}
