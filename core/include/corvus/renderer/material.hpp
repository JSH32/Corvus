#pragma once
#include "corvus/graphics/graphics.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

namespace Corvus::Renderer {

using Graphics::CommandBuffer;
using Graphics::Shader;
using Graphics::Texture2D;
using Graphics::TextureCube;

// Uniform value types
using UniformValue = std::variant<int, float, glm::vec2, glm::vec3, glm::vec4, glm::mat4>;

// Render state configuration
struct RenderState {
    bool depthTest  = true;
    bool depthWrite = true;
    bool blend      = false;
    bool cullFace   = true;

    bool operator==(const RenderState& other) const {
        return depthTest == other.depthTest && depthWrite == other.depthWrite
            && blend == other.blend && cullFace == other.cullFace;
    }
};

class Material {
public:
    Material(const Shader& shader);

    // Uniform setters
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setMat4(const std::string& name, const glm::mat4& value);

    // Texture binding
    void setTexture(uint32_t slot, const Texture2D& texture);
    void setTextureCube(uint32_t slot, const TextureCube& texture);

    // Render state
    void               setRenderState(const RenderState& state);
    const RenderState& getRenderState() const { return renderState_; }

    // Bind all state and uniforms to command buffer
    void bind(CommandBuffer& cmd);

    // Shader access
    Shader& getShader() { return shader_; }

    // For sorting/batching
    uint32_t getShaderId() const { return shader_.id; }

private:
    Shader                                        shader_;
    std::unordered_map<std::string, UniformValue> uniforms_;
    std::unordered_map<uint32_t, Texture2D>       textures_;
    std::unordered_map<uint32_t, TextureCube>     textureCubes_;
    RenderState                                   renderState_;
};

using MaterialRef = std::shared_ptr<Material>;

} // namespace Corvus::Renderer
