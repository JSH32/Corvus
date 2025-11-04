#include "corvus/graphics/graphics.hpp"
#include "corvus/graphics/opengl_context.hpp"
#include <optional>

namespace Corvus::Graphics {

std::unique_ptr<GraphicsContext> GraphicsContext::create(GraphicsAPI api) {
    switch (api) {
        case GraphicsAPI::OpenGL:
            return std::make_unique<OpenGLContext>(nullptr);
        case GraphicsAPI::Vulkan:
        case GraphicsAPI::DirectX12:
        case GraphicsAPI::Metal:
        default:
            return nullptr;
    }
}

uint32_t getSizeOfType(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float:
            return 4;
        case ShaderDataType::Float2:
            return 4 * 2;
        case ShaderDataType::Float3:
            return 4 * 3;
        case ShaderDataType::Float4:
            return 4 * 4;
        case ShaderDataType::Mat3:
            return 4 * 3 * 3;
        case ShaderDataType::Mat4:
            return 4 * 4 * 4;
        case ShaderDataType::Int:
            return 4;
        case ShaderDataType::Int2:
            return 4 * 2;
        case ShaderDataType::Int3:
            return 4 * 3;
        case ShaderDataType::Int4:
            return 4 * 4;
        case ShaderDataType::Byte:
            return 1;
        case ShaderDataType::Bool:
            return 1;
        default:
            return 0;
    }
}

uint32_t getComponentCount(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float:
            return 1;
        case ShaderDataType::Float2:
            return 2;
        case ShaderDataType::Float3:
            return 3;
        case ShaderDataType::Float4:
            return 4;
        case ShaderDataType::Mat3:
            return 3 * 3;
        case ShaderDataType::Mat4:
            return 4 * 4;
        case ShaderDataType::Int:
            return 1;
        case ShaderDataType::Int2:
            return 2;
        case ShaderDataType::Int3:
            return 3;
        case ShaderDataType::Int4:
            return 4;
        case ShaderDataType::Byte:
            return 1;
        case ShaderDataType::Bool:
            return 1;
        default:
            return 0;
    }
}

// VertexBuffer implementation
void VertexBuffer::setData(CommandBuffer& cmd, const void* data, uint32_t size) {
    if (valid()) {
        cmd.updateVertexBuffer(*this, data, size);
        sizeBytes = size;
    }
}

void VertexBuffer::release() {
    if (valid()) {
        be->vbDestroy(id);
        id        = 0;
        be        = nullptr;
        sizeBytes = 0;
    }
}

// IndexBuffer implementation
void IndexBuffer::setData(CommandBuffer& cmd, const void* indices, uint32_t newCount, bool is16) {
    if (valid()) {
        cmd.updateIndexBuffer(*this, indices, newCount, is16);
        count   = newCount;
        index16 = is16;
    }
}

void IndexBuffer::release() {
    if (valid()) {
        be->ibDestroy(id);
        id    = 0;
        be    = nullptr;
        count = 0;
    }
}

// VertexArray implementation
void VertexArray::addVertexBuffer(const VertexBuffer& vb, const VertexBufferLayout& layout) {
    if (!valid() || !vb.valid())
        return;

    std::vector<uint32_t> comps;
    comps.reserve(layout.getElements().size());
    std::vector<bool> norms;
    norms.reserve(layout.getElements().size());

    for (auto& e : layout.getElements()) {
        comps.push_back(e.count);
        norms.push_back(e.normalized);
    }

    be->vaoAddVB(id, vb.id, comps, norms, layout.getStride());
}

void VertexArray::setIndexBuffer(const IndexBuffer& ib) {
    if (valid() && ib.valid())
        be->vaoSetIB(id, ib.id);
}

void VertexArray::release() {
    if (!valid() || !be)
        return;
    be->vaoDestroy(id);
    id = 0;
    be = nullptr;
}

// Shader implementation
void Shader::setUniform(CommandBuffer& cmd, const char* name, const float* m16) {
    if (valid())
        cmd.setShaderUniformMat4(*this, name, m16);
}

void Shader::setMat4(CommandBuffer& cmd, const char* name, const float* m16) {
    if (valid())
        cmd.setShaderUniformMat4(*this, name, m16);
}

void Shader::setMat4(CommandBuffer& cmd, const char* name, const glm::mat4& m) {
    if (valid())
        cmd.setShaderUniformMat4(*this, name, &m[0][0]);
}

void Shader::setInt(CommandBuffer& cmd, const char* name, int value) {
    if (valid())
        cmd.setShaderUniformInt(*this, name, value);
}

void Shader::setFloat(CommandBuffer& cmd, const char* name, float value) {
    if (valid())
        cmd.setShaderUniformFloat(*this, name, value);
}

void Shader::setVec3(CommandBuffer& cmd, const char* name, const glm::vec3& v) {
    if (valid())
        cmd.setShaderUniformVec3(*this, name, &v.x);
}

void Shader::setVec4(CommandBuffer& cmd, const char* name, const glm::vec4& v) {
    if (valid())
        cmd.setShaderUniformVec4(*this, name, &v.x);
}

void Shader::setVec2(CommandBuffer& cmd, const char* name, const glm::vec2& v) {
    if (valid())
        cmd.setShaderUniformVec2(*this, name, &v.x);
}

void Shader::release() {
    if (valid()) {
        be->shaderDestroy(id);
        id = 0;
        be = nullptr;
    }
}

// Texture2D implementation
void Texture2D::setData(const void* data, uint32_t sizeBytes) {
    if (valid())
        be->tex2DSetData(id, data, sizeBytes);
}

void Texture2D::release() {
    if (valid()) {
        be->tex2DDestroy(id);
        id    = 0;
        be    = nullptr;
        width = height = 0;
    }
}

// TextureCube implementation
void TextureCube::release() {
    if (valid()) {
        be->texCubeDestroy(id);
        id         = 0;
        be         = nullptr;
        resolution = 0;
    }
}

// Framebuffer implementation
void Framebuffer::attachTexture2D(const Texture2D& tex, uint32_t attachment) {
    if (valid() && tex.valid())
        be->fbAttachTexture2D(id, tex.id, attachment);
}

void Framebuffer::attachTextureCubeFace(const TextureCube& tex, int faceIndex) {
    if (valid() && tex.valid())
        be->fbAttachTextureCubeFace(id, tex.id, faceIndex);
}

void Framebuffer::attachDepthTexture(const Texture2D& tex) {
    if (valid() && tex.valid())
        be->fbAttachDepthTexture(id, tex.id);
}

void Framebuffer::release() {
    if (valid()) {
        be->fbDestroy(id);
        id     = 0;
        be     = nullptr;
        width  = 0;
        height = 0;
    }
}

// CommandBuffer implementation
void CommandBuffer::begin() {
    if (valid())
        be->cmdBegin(id);
}

void CommandBuffer::end() {
    if (valid())
        be->cmdEnd(id);
}

void CommandBuffer::submit() {
    if (valid())
        be->cmdSubmit(id);
}

void CommandBuffer::setViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (valid())
        be->cmdSetViewport(id, x, y, w, h);
}

void CommandBuffer::setShader(const Shader& s) {
    if (valid() && s.valid())
        be->cmdSetShader(id, s.id);
}

void CommandBuffer::setVertexArray(const VertexArray& v) {
    if (valid() && v.valid())
        be->cmdSetVAO(id, v.id);
}

void CommandBuffer::setLineWidth(float width) {
    if (valid())
        be->cmdSetLineWidth(id, width);
}

void CommandBuffer::bindTexture(uint32_t                   slot,
                                const Texture2D&           t,
                                std::optional<std::string> uniformName) {
    if (valid() && t.valid())
        be->cmdBindTexture(id, slot, t.id, uniformName);
}

void CommandBuffer::bindTextureCube(uint32_t                   slot,
                                    const TextureCube&         t,
                                    std::optional<std::string> uniformName) {
    if (valid() && t.valid())
        be->cmdBindTextureCube(id, slot, t.id, uniformName);
}

void CommandBuffer::drawIndexed(uint32_t      elemCount,
                                bool          index16,
                                uint32_t      indexOffset,
                                PrimitiveType primitive) {
    if (valid())
        be->cmdDrawIndexed(id, elemCount, index16, indexOffset, primitive);
}

void CommandBuffer::bindFramebuffer(const Framebuffer& fb) {
    if (valid() && fb.valid())
        be->cmdBindFramebuffer(id, fb.id, fb.width, fb.height);
}

void CommandBuffer::unbindFramebuffer() {
    if (valid())
        be->cmdUnbindFramebuffer(id);
}

void CommandBuffer::clear(float r, float g, float b, float a, bool clearDepth, bool clearStencil) {
    if (valid())
        be->cmdClearFramebuffer(id, r, g, b, a, clearDepth, clearStencil);
}

void CommandBuffer::setBlendState(bool enable) {
    if (valid())
        be->cmdSetBlendState(id, enable);
}

void CommandBuffer::setDepthTest(bool enable) {
    if (valid())
        be->cmdSetDepthTest(id, enable);
}

void CommandBuffer::setCullFace(bool enable, bool clockwise) {
    if (valid())
        be->cmdSetCullFace(id,
                           enable,
                           clockwise ? Command::FaceCullingData::Order::Clockwise
                                     : Command::FaceCullingData::Order::CounterClockwise);
}

void CommandBuffer::setScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (valid())
        be->cmdSetScissor(id, x, y, w, h);
}

void CommandBuffer::enableScissor(bool enable) {
    if (valid())
        be->cmdEnableScissor(id, enable);
}

void CommandBuffer::release() {
    id = 0;
    be = nullptr;
}

void CommandBuffer::updateVertexBuffer(const VertexBuffer& vb, const void* data, uint32_t size) {
    if (valid() && vb.valid())
        be->cmdUpdateVertexBuffer(id, vb.id, data, size);
}

void CommandBuffer::updateIndexBuffer(const IndexBuffer& ib,
                                      const void*        data,
                                      uint32_t           count,
                                      bool               index16) {
    if (valid() && ib.valid())
        be->cmdUpdateIndexBuffer(id, ib.id, data, count, index16);
}

void CommandBuffer::setShaderUniformMat4(const Shader& shader, const char* name, const float* m16) {
    if (valid() && shader.valid())
        be->cmdSetShaderUniformMat4(id, shader.id, name, m16);
}

void CommandBuffer::setShaderUniformInt(const Shader& shader, const char* name, int value) {
    if (valid() && shader.valid())
        be->cmdSetShaderUniformInt(id, shader.id, name, value);
}

void CommandBuffer::setShaderUniformFloat(const Shader& shader, const char* name, float value) {
    if (valid() && shader.valid())
        be->cmdSetShaderUniformFloat(id, shader.id, name, value);
}

void CommandBuffer::setShaderUniformVec3(const Shader& shader,
                                         const char*   name,
                                         const float*  vec3) {
    if (valid() && shader.valid())
        be->cmdSetShaderUniformVec3(id, shader.id, name, vec3);
}

void CommandBuffer::setShaderUniformVec4(const Shader& shader,
                                         const char*   name,
                                         const float*  vec4) {
    if (valid() && shader.valid())
        be->cmdSetShaderUniformVec4(id, shader.id, name, vec4);
}

void CommandBuffer::setShaderUniformVec2(const Shader& shader,
                                         const char*   name,
                                         const float*  vec4) {
    if (valid() && shader.valid())
        be->cmdSetShaderUniformVec2(id, shader.id, name, vec4);
}

void Framebuffer::bind(uint32_t cmdID) const {
    if (valid())
        be->cmdBindFramebuffer(cmdID, id, width, height);
}

void CommandBuffer::setDepthMask(bool enable) {
    if (valid())
        be->cmdSetDepthMask(id, enable);
}

}
