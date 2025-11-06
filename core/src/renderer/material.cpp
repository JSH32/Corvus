#include "corvus/renderer/material.hpp"

namespace Corvus::Renderer {

Material::Material(const Shader& shader) : shader(shader) { }

void Material::setInt(const std::string& name, int value) { uniforms[name] = value; }

void Material::setFloat(const std::string& name, float value) { uniforms[name] = value; }

void Material::setVec2(const std::string& name, const glm::vec2& value) { uniforms[name] = value; }

void Material::setVec3(const std::string& name, const glm::vec3& value) { uniforms[name] = value; }

void Material::setVec4(const std::string& name, const glm::vec4& value) { uniforms[name] = value; }

void Material::setMat4(const std::string& name, const glm::mat4& value) { uniforms[name] = value; }

void Material::setTexture(const uint32_t slot, const Texture2D& texture) {
    textures[slot] = texture;
}

void Material::setTextureCube(const uint32_t slot, const TextureCube& texture) {
    textureCubes[slot] = texture;
}

void Material::setRenderState(const RenderState& state) { renderState = state; }

void Material::bind(CommandBuffer& cmd) {
    // Bind shader
    cmd.setShader(shader);

    // Set render state
    cmd.setDepthTest(renderState.depthTest);
    cmd.setDepthMask(renderState.depthWrite);
    cmd.setBlendState(renderState.blend);
    cmd.setCullFace(renderState.cullFace, false);

    // Set uniforms
    for (const auto& [name, value] : uniforms) {
        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int>) {
                    shader.setInt(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, float>) {
                    shader.setFloat(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, glm::vec2>) {
                    shader.setVec2(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    shader.setVec3(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, glm::vec4>) {
                    shader.setVec4(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, glm::mat4>) {
                    shader.setMat4(cmd, name.c_str(), arg);
                }
            },
            value);
    }

    // Bind textures
    for (const auto& [slot, texture] : textures) {
        cmd.bindTexture(slot, texture);
    }

    // Bind cube maps
    for (const auto& [slot, cubemap] : textureCubes) {
        cmd.bindTextureCube(slot, cubemap);
    }
}

}
