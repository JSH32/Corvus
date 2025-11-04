#include "corvus/graphics/opengl_context.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/log.hpp"
#include "spdlog/fmt/bundled/format.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>

namespace Corvus::Graphics {

// Helpers
static uint32_t compileGL(uint32_t type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char    log[2048];
        GLsizei n = 0;
        glGetShaderInfoLog(sh, 2048, &n, log);
        std::cerr << "GL shader compile error:\n" << log << "\n";
        CORVUS_CORE_ERROR("SHADER COMPILE FAILED:\n{}", log);
    } else {
        char    log[2048];
        GLsizei n = 0;
        glGetShaderInfoLog(sh, 2048, &n, log);
        if (n > 0) {
            CORVUS_CORE_WARN("Shader compile warnings:\n{}", log);
        }
    }
    return sh;
}

static uint32_t linkProgram(uint32_t vs, uint32_t fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char    log[2048];
        GLsizei n = 0;
        glGetProgramInfoLog(p, 2048, &n, log);
        std::cerr << "GL link error:\n" << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

// VBO, Creation and destruction only (updates via command buffer)
VertexBuffer OpenGLBackend::vbCreate(const void* data, uint32_t size) {
    GLuint id = 0;
    glGenBuffers(1, &id);
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW);
    VertexBuffer h;
    h.id        = id;
    h.be        = this;
    h.sizeBytes = size;
    return h;
}

void OpenGLBackend::vbDestroy(uint32_t id) {
    if (id)
        glDeleteBuffers(1, &id);
}

// IBO - Creation and destruction only (updates via command buffer)
IndexBuffer OpenGLBackend::ibCreate(const void* indices, uint32_t count, bool index16) {
    GLuint id = 0;
    glGenBuffers(1, &id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * (index16 ? 2u : 4u), indices, GL_DYNAMIC_DRAW);
    IndexBuffer h;
    h.id      = id;
    h.be      = this;
    h.count   = count;
    h.index16 = index16;
    return h;
}

void OpenGLBackend::ibDestroy(uint32_t id) {
    if (id)
        glDeleteBuffers(1, &id);
}

// VAO
VertexArray OpenGLBackend::vaoCreate() {
    GLuint id = 0;
    glGenVertexArrays(1, &id);
    VertexArray h;
    h.id = id;
    h.be = this;
    return h;
}

void OpenGLBackend::vaoAddVB(uint32_t                     vaoId,
                             uint32_t                     vbId,
                             const std::vector<uint32_t>& comps,
                             const std::vector<bool>&     normalized,
                             uint32_t                     stride) {
    glBindVertexArray(vaoId);
    glBindBuffer(GL_ARRAY_BUFFER, vbId);
    GLsizei   glStride = static_cast<GLsizei>(stride);
    uintptr_t offset   = 0;

    for (GLuint attrib = 0; attrib < comps.size(); ++attrib) {
        glEnableVertexAttribArray(attrib);

        // Special case, 4-component normalized attribute = packed RGBA bytes (for ImGui)
        if (comps[attrib] == 4 && normalized[attrib]) {
            glVertexAttribPointer(
                attrib, 4, GL_UNSIGNED_BYTE, GL_TRUE, glStride, (const void*)offset);
            offset += 4; // 4 bytes
        } else {
            // Standard float attributes
            glVertexAttribPointer(attrib,
                                  comps[attrib],
                                  GL_FLOAT,
                                  normalized[attrib] ? GL_TRUE : GL_FALSE,
                                  glStride,
                                  (const void*)offset);
            offset += comps[attrib] * sizeof(float);
        }
    }

    // Disable unused attributes
    for (GLuint attrib = comps.size(); attrib < 16; ++attrib) {
        glDisableVertexAttribArray(attrib);
    }

    glBindVertexArray(0);

    // Check for OpenGL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        CORVUS_CORE_ERROR("OpenGL error after vaoAddVB: 0x{:x}", err);
    }
}

void OpenGLBackend::vaoSetIB(uint32_t vaoId, uint32_t ibId) {
    glBindVertexArray(vaoId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibId);
    glBindVertexArray(0);
}

void OpenGLBackend::vaoDestroy(uint32_t id) {
    if (id)
        glDeleteVertexArrays(1, &id);
}

// Shader, Creation and destruction only (uniforms via command buffer)
Shader OpenGLBackend::shaderCreate(const std::string& vs, const std::string& fs) {
    uint32_t v = compileGL(GL_VERTEX_SHADER, vs.c_str());
    uint32_t f = compileGL(GL_FRAGMENT_SHADER, fs.c_str());
    uint32_t p = linkProgram(v, f);
    Shader   h;
    h.id = p;
    h.be = this;
    return h;
}

void OpenGLBackend::shaderDestroy(uint32_t id) {
    if (id)
        glDeleteProgram(id);
}

// Texture2D
Texture2D OpenGLBackend::tex2DCreate(uint32_t w, uint32_t h) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Texture2D t;
    t.id     = id;
    t.be     = this;
    t.width  = w;
    t.height = h;
    return t;
}

void OpenGLBackend::tex2DSetData(uint32_t id, const void* data, uint32_t /*sizeBytes*/) {
    if (!id || !data)
        return;

    glBindTexture(GL_TEXTURE_2D, id);

    // Query current texture width/height to upload full image.
    GLint width = 0, height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    if (width > 0 && height > 0) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
}

void OpenGLBackend::tex2DDestroy(uint32_t id) {
    if (id)
        glDeleteTextures(1, &id);
}

Texture2D OpenGLBackend::tex2DCreateDepth(uint32_t w, uint32_t h) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    Texture2D t;
    t.id     = id;
    t.be     = this;
    t.width  = w;
    t.height = h;
    return t;
}

// TextureCube
TextureCube OpenGLBackend::texCubeCreate(uint32_t res) {
    TextureCube t;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, t.id);

    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                     0,
                     GL_DEPTH_COMPONENT,
                     res,
                     res,
                     0,
                     GL_DEPTH_COMPONENT,
                     GL_FLOAT,
                     nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    t.resolution = res;
    t.be         = this;
    return t;
}

void OpenGLBackend::texCubeSetFaceData(
    uint32_t id, int faceIndex, const void* data, uint32_t resolution, uint32_t sizeBytes) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
                    0,
                    0,
                    0,
                    resolution,
                    resolution,
                    GL_DEPTH_COMPONENT,
                    GL_FLOAT,
                    data);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void OpenGLBackend::texCubeDestroy(uint32_t id) {
    GLuint tex = id;
    glDeleteTextures(1, &tex);
}

// Command Buffer, Records and executes commands in order
CommandBuffer OpenGLBackend::cmdCreate() {
    uint32_t id         = nextCmdBufferId_++;
    commandBuffers_[id] = CommandBufferData();

    CommandBuffer cb;
    cb.id = id;
    cb.be = this;
    return cb;
}

void OpenGLBackend::cmdBegin(uint32_t id) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end())
        return;

    it->second.commands.clear();
    it->second.recording = true;
}

void OpenGLBackend::cmdEnd(uint32_t id) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end())
        return;

    it->second.recording = false;
}

void OpenGLBackend::cmdSetViewport(uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetViewport;
    cmd.data = Command::ViewportData { x, y, w, h };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdSetLineWidth(uint32_t cmdId, float width) {
    auto it = commandBuffers_.find(cmdId);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetLineWidth;
    cmd.data = Command::LineWidthData { width };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdSetShader(uint32_t id, uint32_t shaderId) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetShader;
    cmd.data = Command::ShaderData { shaderId };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdSetVAO(uint32_t id, uint32_t vaoId) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetVAO;
    cmd.data = Command::VAOData { vaoId };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdBindTexture(uint32_t                   id,
                                   uint32_t                   slot,
                                   uint32_t                   texId,
                                   std::optional<std::string> uniformName) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::BindTexture;
    cmd.data = Command::TextureData { slot, texId, uniformName };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdBindTextureCube(uint32_t                   id,
                                       uint32_t                   slot,
                                       uint32_t                   texID,
                                       std::optional<std::string> uniformName) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::BindTextureCube;
    cmd.data = Command::TextureData { slot, texID, uniformName };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdDrawIndexed(
    uint32_t id, uint32_t elemCount, bool index16, uint32_t indexOffset, PrimitiveType primitive) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::DrawIndexed;
    cmd.data = Command::DrawIndexedData { elemCount, index16, indexOffset, primitive };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdBindFramebuffer(uint32_t cmdID,
                                       uint32_t fbID,
                                       uint32_t width,
                                       uint32_t height) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::BindFramebuffer;
    cmd.data = Command::FramebufferData { fbID, width, height };
    it->second.commands.push_back(std::move(cmd));
}

void OpenGLBackend::cmdUnbindFramebuffer(uint32_t id) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::UnbindFramebuffer;
    cmd.data = std::monostate {};
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdClearFramebuffer(
    uint32_t id, float r, float g, float b, float a, bool clearDepth, bool clearStencil) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::ClearFramebuffer;
    cmd.data = Command::ClearData { r, g, b, a, clearDepth, clearStencil };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdSetBlendState(uint32_t id, bool enable) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetBlendState;
    cmd.data = Command::StateData { enable };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdSetDepthTest(uint32_t id, bool enable) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetDepthTest;
    cmd.data = Command::StateData { enable };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdSetCullFace(uint32_t                        id,
                                   bool                            enable,
                                   Command::FaceCullingData::Order winding) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetCullFace;
    cmd.data = Command::FaceCullingData { enable, winding };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdSetScissor(uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetScissor;
    cmd.data = Command::ScissorData { x, y, w, h };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdEnableScissor(uint32_t id, bool enable) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::EnableScissor;
    cmd.data = Command::StateData { enable };
    it->second.commands.push_back(cmd);
}

void OpenGLBackend::cmdExecuteCallback(uint32_t id, std::function<void()> callback) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::UserCallback;
    cmd.data = Command::UserCallbackData { std::move(callback) };
    it->second.commands.push_back(cmd);
}

// Buffer updates (deferred)
void OpenGLBackend::cmdUpdateVertexBuffer(uint32_t    cmdID,
                                          uint32_t    vboID,
                                          const void* data,
                                          uint32_t    size) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::UpdateVertexBuffer;

    Command::UpdateVertexBufferData bufData;
    bufData.vboId = vboID;
    bufData.data.resize(size);
    std::memcpy(bufData.data.data(), data, size);

    cmd.data = std::move(bufData);
    it->second.commands.push_back(std::move(cmd));
}

void OpenGLBackend::cmdUpdateIndexBuffer(
    uint32_t cmdID, uint32_t iboID, const void* data, uint32_t count, bool index16) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::UpdateIndexBuffer;

    Command::UpdateIndexBufferData bufData;
    bufData.iboId   = iboID;
    bufData.count   = count;
    bufData.index16 = index16;

    uint32_t size = count * (index16 ? 2 : 4);
    bufData.data.resize(size);
    std::memcpy(bufData.data.data(), data, size);

    cmd.data = std::move(bufData);
    it->second.commands.push_back(std::move(cmd));
}

// Shader uniforms (deferred)
void OpenGLBackend::cmdSetShaderUniformMat4(uint32_t     cmdID,
                                            uint32_t     shaderID,
                                            const char*  name,
                                            const float* m16) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetShaderUniformMat4;

    Command::SetShaderUniformMat4Data uniformData;
    uniformData.shaderId = shaderID;
    uniformData.name     = name;
    std::memcpy(uniformData.matrix, m16, sizeof(float) * 16);

    cmd.data = std::move(uniformData);
    it->second.commands.push_back(std::move(cmd));
}

void OpenGLBackend::cmdSetShaderUniformInt(uint32_t    cmdID,
                                           uint32_t    shaderID,
                                           const char* name,
                                           int         value) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetShaderUniformInt;
    cmd.data = Command::SetShaderUniformIntData { shaderID, name, value };
    it->second.commands.push_back(std::move(cmd));
}

void OpenGLBackend::cmdSetShaderUniformFloat(uint32_t    cmdID,
                                             uint32_t    shaderID,
                                             const char* name,
                                             float       value) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetShaderUniformFloat;
    cmd.data = Command::SetShaderUniformFloatData { shaderID, name, value };
    it->second.commands.push_back(std::move(cmd));
}

void OpenGLBackend::cmdSetShaderUniformVec3(uint32_t     cmdID,
                                            uint32_t     shaderID,
                                            const char*  name,
                                            const float* vec3) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetShaderUniformVec3;

    Command::SetShaderUniformVec3Data uniformData;
    uniformData.shaderId = shaderID;
    uniformData.name     = name;
    std::memcpy(uniformData.vec, vec3, sizeof(float) * 3);

    cmd.data = std::move(uniformData);
    it->second.commands.push_back(std::move(cmd));
}

void OpenGLBackend::cmdSetShaderUniformVec4(uint32_t     cmdID,
                                            uint32_t     shaderID,
                                            const char*  name,
                                            const float* vec4) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetShaderUniformVec4;

    Command::SetShaderUniformVec4Data uniformData;
    uniformData.shaderId = shaderID;
    uniformData.name     = name;
    std::memcpy(uniformData.vec, vec4, sizeof(float) * 4);

    cmd.data = std::move(uniformData);
    it->second.commands.push_back(std::move(cmd));
}

void OpenGLBackend::cmdSetShaderUniformVec2(uint32_t     cmdID,
                                            uint32_t     shaderID,
                                            const char*  name,
                                            const float* vec2) {
    auto it = commandBuffers_.find(cmdID);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetShaderUniformVec2;

    Command::SetShaderUniformVec2Data uniformData;
    uniformData.shaderId = shaderID;
    uniformData.name     = name;
    std::memcpy(uniformData.vec, vec2, sizeof(float) * 2);

    cmd.data = std::move(uniformData);
    it->second.commands.push_back(std::move(cmd));
}

// Execute all recorded commands
void OpenGLBackend::executeCommand(const Command& cmd) {
    switch (cmd.type) {
        case Command::Type::SetViewport: {
            auto& vp = std::get<Command::ViewportData>(cmd.data);
            glViewport(vp.x, vp.y, vp.w, vp.h);
            break;
        }

        case Command::Type::SetLineWidth: {
            auto& d = std::get<Command::LineWidthData>(cmd.data);
            glLineWidth(d.width);
            break;
        }

        case Command::Type::SetShader: {
            auto& shader = std::get<Command::ShaderData>(cmd.data);
            glUseProgram(shader.shaderId);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                CORVUS_CORE_ERROR(
                    "OpenGL error after SetShader (id={}): 0x{:x}", shader.shaderId, err);
            }
            break;
        }

        case Command::Type::SetVAO: {
            auto& vao = std::get<Command::VAOData>(cmd.data);
            glBindVertexArray(vao.vaoId);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                CORVUS_CORE_ERROR("OpenGL error after SetVAO (id={}): 0x{:x}", vao.vaoId, err);
            }

            // Also check if VAO is actually valid
            GLint isVAO = 0;
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &isVAO);
            break;
        }

        case Command::Type::BindTexture: {
            auto& tex = std::get<Command::TextureData>(cmd.data);
            glActiveTexture(GL_TEXTURE0 + tex.slot);
            glBindTexture(GL_TEXTURE_2D, tex.texId);

            if (tex.uniformName && !tex.uniformName->empty()) {
                GLint currentProgram = 0;
                glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
                if (currentProgram != 0) {
                    GLint loc = glGetUniformLocation(currentProgram, tex.uniformName->c_str());
                    if (loc >= 0)
                        glUniform1i(loc, tex.slot);
                }
            }

            break;
        }

        case Command::Type::BindTextureCube: {
            auto& tex = std::get<Command::TextureData>(cmd.data);
            glActiveTexture(GL_TEXTURE0 + tex.slot);
            glBindTexture(GL_TEXTURE_CUBE_MAP, tex.texId);

            if (tex.uniformName && !tex.uniformName->empty()) {
                GLint currentProgram = 0;
                glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
                if (currentProgram != 0) {
                    GLint loc = glGetUniformLocation(currentProgram, tex.uniformName->c_str());
                    if (loc >= 0)
                        glUniform1i(loc, tex.slot);
                }
            }
            break;
        }

        case Command::Type::DrawIndexed: {
            auto&  draw = std::get<Command::DrawIndexedData>(cmd.data);
            GLenum glPrimitive;
            switch (draw.mode) {
                case PrimitiveType::Triangles:
                    glPrimitive = GL_TRIANGLES;
                    break;
                case PrimitiveType::Lines:
                    glPrimitive = GL_LINES;
                    break;
                case PrimitiveType::LineStrip:
                    glPrimitive = GL_LINE_STRIP;
                    break;
                case PrimitiveType::Points:
                    glPrimitive = GL_POINTS;
                    break;
                default:
                    glPrimitive = GL_TRIANGLES;
                    break;
            }

            const void* offset
                = (void*)(draw.offset * (draw.index16 ? sizeof(GLushort) : sizeof(GLuint)));
            glDrawElements(glPrimitive,
                           draw.elemCount,
                           draw.index16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                           offset);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                CORVUS_CORE_ERROR("OpenGL error after DrawIndexed: 0x{:x} ({})", err, err);
            }

            break;
        }

        case Command::Type::BindFramebuffer: {
            auto& fb = std::get<Command::FramebufferData>(cmd.data);
            glBindFramebuffer(GL_FRAMEBUFFER, fb.fbId);

            GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &drawBuffer);
            break;
        }

        case Command::Type::UnbindFramebuffer: {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            break;
        }

        case Command::Type::ClearFramebuffer: {
            auto&      clear = std::get<Command::ClearData>(cmd.data);
            GLbitfield mask  = GL_COLOR_BUFFER_BIT;
            if (clear.depth)
                mask |= GL_DEPTH_BUFFER_BIT;
            if (clear.stencil)
                mask |= GL_STENCIL_BUFFER_BIT;
            glClearColor(clear.r, clear.g, clear.b, clear.a);
            glClear(mask);
            break;
        }

        case Command::Type::SetBlendState: {
            auto& state = std::get<Command::StateData>(cmd.data);
            if (state.enable) {
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFuncSeparate(
                    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            } else {
                glDisable(GL_BLEND);
            }
            break;
        }

        case Command::Type::SetDepthTest: {
            auto& state = std::get<Command::StateData>(cmd.data);
            if (state.enable)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            break;
        }

        case Command::Type::SetCullFace: {
            auto& state = std::get<Command::FaceCullingData>(cmd.data);
            if (state.enable) {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                if (state.order == Command::FaceCullingData::Order::Clockwise)
                    glFrontFace(GL_CW);
                else
                    glFrontFace(GL_CCW);
            } else {
                glDisable(GL_CULL_FACE);
            }
            break;
        }

        case Command::Type::SetScissor: {
            auto& scissor = std::get<Command::ScissorData>(cmd.data);
            glScissor(scissor.x, scissor.y, scissor.w, scissor.h);
            break;
        }

        case Command::Type::EnableScissor: {
            auto& state = std::get<Command::StateData>(cmd.data);
            if (state.enable)
                glEnable(GL_SCISSOR_TEST);
            else
                glDisable(GL_SCISSOR_TEST);
            break;
        }

        case Command::Type::UserCallback: {
            auto& cb = std::get<Command::UserCallbackData>(cmd.data);
            if (cb.callback) {
                cb.callback();
            }
            break;
        }

        case Command::Type::UpdateVertexBuffer: {
            auto& buf = std::get<Command::UpdateVertexBufferData>(cmd.data);
            glBindBuffer(GL_ARRAY_BUFFER, buf.vboId);
            glBufferData(GL_ARRAY_BUFFER, buf.data.size(), buf.data.data(), GL_DYNAMIC_DRAW);
            break;
        }

        case Command::Type::UpdateIndexBuffer: {
            auto& buf = std::get<Command::UpdateIndexBufferData>(cmd.data);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf.iboId);
            glBufferData(
                GL_ELEMENT_ARRAY_BUFFER, buf.data.size(), buf.data.data(), GL_DYNAMIC_DRAW);
            break;
        }

        case Command::Type::SetShaderUniformMat4: {
            auto& uniform = std::get<Command::SetShaderUniformMat4Data>(cmd.data);
            glUseProgram(uniform.shaderId);
            GLint loc = glGetUniformLocation(uniform.shaderId, uniform.name.c_str());
            if (loc >= 0)
                glUniformMatrix4fv(loc, 1, GL_FALSE, uniform.matrix);
            break;
        }

        case Command::Type::SetShaderUniformInt: {
            auto& uniform = std::get<Command::SetShaderUniformIntData>(cmd.data);
            glUseProgram(uniform.shaderId);
            GLint loc = glGetUniformLocation(uniform.shaderId, uniform.name.c_str());
            if (loc >= 0)
                glUniform1i(loc, uniform.value);
            break;
        }

        case Command::Type::SetShaderUniformFloat: {
            auto& uniform = std::get<Command::SetShaderUniformFloatData>(cmd.data);
            glUseProgram(uniform.shaderId);
            GLint loc = glGetUniformLocation(uniform.shaderId, uniform.name.c_str());
            if (loc >= 0)
                glUniform1f(loc, uniform.value);
            break;
        }

        case Command::Type::SetShaderUniformVec3: {
            auto& uniform = std::get<Command::SetShaderUniformVec3Data>(cmd.data);
            glUseProgram(uniform.shaderId);
            GLint loc = glGetUniformLocation(uniform.shaderId, uniform.name.c_str());
            if (loc >= 0)
                glUniform3fv(loc, 1, uniform.vec);
            break;
        }
        case Command::Type::SetShaderUniformVec4: {
            auto& uniform = std::get<Command::SetShaderUniformVec4Data>(cmd.data);
            glUseProgram(uniform.shaderId);
            GLint loc = glGetUniformLocation(uniform.shaderId, uniform.name.c_str());
            if (loc >= 0)
                glUniform4fv(loc, 1, uniform.vec);
            break;
        }
        case Command::Type::SetShaderUniformVec2: {
            auto& uniform = std::get<Command::SetShaderUniformVec2Data>(cmd.data);
            glUseProgram(uniform.shaderId);
            GLint loc = glGetUniformLocation(uniform.shaderId, uniform.name.c_str());
            if (loc >= 0)
                glUniform2fv(loc, 1, uniform.vec);
            break;
        }
        case Command::Type::SetDepthMask: {
            auto& state = std::get<Command::StateData>(cmd.data);
            glDepthMask(state.enable ? GL_TRUE : GL_FALSE);
            break;
        }
    }
}

void OpenGLBackend::queueCommandBuffer(uint32_t cmdId) { pendingSubmissions_.push_back(cmdId); }

const std::vector<uint32_t>& OpenGLBackend::getPendingSubmissions() const {
    return pendingSubmissions_;
}

void OpenGLBackend::cmdExecute(uint32_t id) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end())
        return;

    // Execute all recorded commands in order
    for (size_t i = 0; i < it->second.commands.size(); ++i) {
        const auto& cmd = it->second.commands[i];
        executeCommand(cmd);
    }
}

void OpenGLBackend::clearPendingSubmissions() { pendingSubmissions_.clear(); }

void OpenGLBackend::cmdSubmit(uint32_t id) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end())
        return;

    pendingSubmissions_.push_back(id);
}

// Framebuffer
Framebuffer OpenGLBackend::fbCreate(uint32_t width, uint32_t height) {
    GLuint fb = 0;
    glGenFramebuffers(1, &fb);
    Framebuffer f;
    f.id     = fb;
    f.be     = this;
    f.width  = width;
    f.height = height;
    return f;
}

void OpenGLBackend::fbAttachTexture2D(uint32_t fbID, uint32_t texID, uint32_t attachment) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbID);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment, GL_TEXTURE_2D, texID, 0);

    const GLenum buf = GL_COLOR_ATTACHMENT0 + attachment;
    glDrawBuffers(1, &buf);
}

void OpenGLBackend::fbAttachDepthTexture(uint32_t fbID, uint32_t texID) {
    if (!fbID || !texID)
        return;

    GLint prevFb = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFb);

    glBindFramebuffer(GL_FRAMEBUFFER, fbID);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texID, 0);

    // Ensure we're drawing to color 0 if there's exactly one color attachment
    static const GLenum buf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buf);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[OpenGLBackend] FBO incomplete after depth attach: 0x" << std::hex << status
                  << std::dec << "\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, prevFb);
}

void OpenGLBackend::fbAttachTextureCubeFace(uint32_t fbID, uint32_t texID, int faceIndex) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbID);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, texID, 0);
}

void OpenGLBackend::fbDestroy(uint32_t fbID) {
    if (fbID)
        glDeleteFramebuffers(1, &fbID);
}

// OpenGL Context
OpenGLContext::OpenGLContext(Window* w) : window(w) { }
OpenGLContext::~OpenGLContext() { shutdown(); }

bool OpenGLContext::initialize(Window& window) {
    window.makeContextCurrent();
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }
    backend = std::make_unique<OpenGLBackend>();
    CORVUS_CORE_INFO("OpenGL: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    return true;
}

void OpenGLContext::flush() {
    endFrame();
    glFinish();
    beginFrame();
}

void OpenGLContext::shutdown() {
    backend.reset();
    window = nullptr;
}

void OpenGLBackend::clearCommandBuffers() {
    commandBuffers_.clear();
    commandBuffers_.rehash(0);
    nextCmdBufferId_ = 1;
}

void OpenGLContext::beginFrame() {
    backend->clearPendingSubmissions();
    backend->clearCommandBuffers();
}

void OpenGLContext::endFrame() {
    const auto& submissions = backend->getPendingSubmissions();

    // CORVUS_CORE_INFO("Submission order:");
    // for (size_t i = 0; i < submissions.size(); ++i) {
    // CORVUS_CORE_INFO("  [{}] Command buffer ID: {}", i, submissions[i]);
    // }

    // CORVUS_CORE_INFO("========================================");
    // CORVUS_CORE_INFO("BEGIN FRAME EXECUTION ({} command buffers queued)", submissions.size());
    // CORVUS_CORE_INFO("========================================");

    // Execute all queued command buffers in order
    for (size_t i = 0; i < submissions.size(); ++i) {
        uint32_t cmdId = submissions[i];

        // CORVUS_CORE_INFO("Command Buffer #{} (ID: {})", i, cmdId);

        backend->cmdExecute(cmdId);

        // Reset state between command buffers
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);

        if (windowWidth > 0 && windowHeight > 0) {
            glViewport(0, 0, windowWidth, windowHeight);
            // CORVUS_CORE_TRACE("Reset viewport to {}x{}", windowWidth, windowHeight);
        }
    }

    // CORVUS_CORE_INFO("========================================");
    // CORVUS_CORE_INFO("END FRAME EXECUTION");
    // CORVUS_CORE_INFO("========================================\n");
}

void OpenGLContext::attachBackend(HandleBase& h) { h.be = backend.get(); }

// Factories
VertexBuffer OpenGLContext::createVertexBuffer(const void* data, uint32_t size) {
    auto h = backend->vbCreate(data, size);
    attachBackend(h);
    return h;
}

IndexBuffer OpenGLContext::createIndexBuffer(const void* indices, uint32_t count, bool index16) {
    auto h = backend->ibCreate(indices, count, index16);
    attachBackend(h);
    return h;
}

VertexArray OpenGLContext::createVertexArray() {
    auto h = backend->vaoCreate();
    attachBackend(h);
    return h;
}

Shader OpenGLContext::createShader(const std::string& vs, const std::string& fs) {
    auto h = backend->shaderCreate(vs, fs);
    attachBackend(h);
    return h;
}

Texture2D OpenGLContext::createTexture2D(uint32_t w, uint32_t h) {
    auto t2d = backend->tex2DCreate(w, h);
    attachBackend(t2d);
    return t2d;
}

Texture2D OpenGLContext::createDepthTexture(uint32_t w, uint32_t h) {
    auto tex = backend->tex2DCreateDepth(w, h);
    attachBackend(tex);
    return tex;
}

TextureCube OpenGLContext::createTextureCube(uint32_t resolution) {
    TextureCube tex = backend->texCubeCreate(resolution);
    tex.be          = backend.get();
    return tex;
}

CommandBuffer OpenGLContext::createCommandBuffer() {
    auto h = backend->cmdCreate();
    attachBackend(h);
    return h;
}

Framebuffer OpenGLContext::createFramebuffer(uint32_t width, uint32_t height) {
    auto h = backend->fbCreate(width, height);
    attachBackend(h);
    return h;
}

void OpenGLBackend::cmdSetDepthMask(uint32_t id, bool enable) {
    auto it = commandBuffers_.find(id);
    if (it == commandBuffers_.end() || !it->second.recording)
        return;

    Command cmd;
    cmd.type = Command::Type::SetDepthMask;
    cmd.data = Command::StateData { enable };
    it->second.commands.push_back(cmd);
}
}
