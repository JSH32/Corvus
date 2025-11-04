#include "corvus/renderer/material.hpp"

namespace Corvus::Renderer {

Material::Material(const Shader& shader) : shader_(shader) { }

void Material::setInt(const std::string& name, int value) { uniforms_[name] = value; }

void Material::setFloat(const std::string& name, float value) { uniforms_[name] = value; }

void Material::setVec2(const std::string& name, const glm::vec2& value) { uniforms_[name] = value; }

void Material::setVec3(const std::string& name, const glm::vec3& value) { uniforms_[name] = value; }

void Material::setVec4(const std::string& name, const glm::vec4& value) { uniforms_[name] = value; }

void Material::setMat4(const std::string& name, const glm::mat4& value) { uniforms_[name] = value; }

void Material::setTexture(uint32_t slot, const Texture2D& texture) { textures_[slot] = texture; }

void Material::setTextureCube(uint32_t slot, const TextureCube& texture) {
    textureCubes_[slot] = texture;
}

void Material::setRenderState(const RenderState& state) { renderState_ = state; }

void Material::bind(CommandBuffer& cmd) {
    // Bind shader
    cmd.setShader(shader_);

    // Set render state
    cmd.setDepthTest(renderState_.depthTest);
    cmd.setDepthMask(renderState_.depthWrite);
    cmd.setBlendState(renderState_.blend);
    cmd.setCullFace(renderState_.cullFace, false);

    // Bind textures
    for (const auto& [slot, texture] : textures_) {
        cmd.bindTexture(slot, texture);
    }

    for (const auto& [slot, texture] : textureCubes_) {
        cmd.bindTextureCube(slot, texture);
    }

    // Set uniforms
    for (const auto& [name, value] : uniforms_) {
        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int>) {
                    shader_.setInt(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, float>) {
                    shader_.setFloat(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, glm::vec2>) {
                    shader_.setVec2(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    shader_.setVec3(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, glm::vec4>) {
                    shader_.setVec4(cmd, name.c_str(), arg);
                } else if constexpr (std::is_same_v<T, glm::mat4>) {
                    shader_.setMat4(cmd, name.c_str(), arg);
                }
            },
            value);
    }
}

}
