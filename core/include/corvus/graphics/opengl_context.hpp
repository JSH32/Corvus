#pragma once
#include "corvus/graphics/graphics.hpp"
#include "corvus/graphics/window.hpp"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <memory>
#include <unordered_map>

namespace Corvus::Graphics {

class OpenGLBackend final : public IGraphicsBackend {
public:
    // VBO
    VertexBuffer vbCreate(const void* data, uint32_t size) override;
    void         vbDestroy(uint32_t id) override;

    // IBO
    IndexBuffer ibCreate(const void* indices, uint32_t count, bool index16) override;
    void        ibDestroy(uint32_t id) override;

    // VAO
    VertexArray vaoCreate() override;
    void        vaoAddVB(uint32_t                     vaoId,
                         uint32_t                     vbId,
                         const std::vector<uint32_t>& comps,
                         const std::vector<bool>&     normalized,
                         uint32_t                     stride) override;
    void        vaoSetIB(uint32_t vaoId, uint32_t ibId) override;
    void        vaoDestroy(uint32_t id) override;

    // Shader
    Shader shaderCreate(const std::string& vs, const std::string& fs) override;
    void   shaderDestroy(uint32_t id) override;

    // Texture
    Texture2D tex2DCreate(uint32_t w, uint32_t h) override;
    Texture2D tex2DCreateDepth(uint32_t w, uint32_t h) override;
    void      tex2DSetData(uint32_t id, const void* data, uint32_t sizeBytes) override;
    void      tex2DDestroy(uint32_t id) override;

    TextureCube texCubeCreate(uint32_t resolution) override;
    void        texCubeSetFaceData(uint32_t    id,
                                   int         faceIndex,
                                   const void* data,
                                   uint32_t    resolution,
                                   uint32_t    sizeBytes) override;
    void        texCubeDestroy(uint32_t id) override;

    // Command buffer, records and executes commands
    CommandBuffer cmdCreate() override;
    void          cmdBegin(uint32_t id) override;
    void          cmdEnd(uint32_t id) override;
    void          cmdSubmit(uint32_t id) override;
    void cmdSetViewport(uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
    void cmdSetShader(uint32_t id, uint32_t shaderId) override;
    void cmdSetLineWidth(uint32_t cmdId, float width) override;
    void cmdSetVAO(uint32_t id, uint32_t vaoId) override;
    void cmdBindTexture(uint32_t id, uint32_t slot, uint32_t texId) override;
    void cmdBindTextureCube(uint32_t cmdID, uint32_t slot, uint32_t texID) override;
    void cmdDrawIndexed(uint32_t      id,
                        uint32_t      elemCount,
                        bool          index16,
                        uint32_t      indexOffset = 0,
                        PrimitiveType primitive   = PrimitiveType::Triangles) override;
    void cmdSetScissor(uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
    void cmdEnableScissor(uint32_t id, bool enable) override;
    void cmdSetBlendState(uint32_t id, bool enable) override;
    void cmdSetDepthTest(uint32_t id, bool enable) override;
    void cmdSetCullFace(uint32_t id, bool enable, Command::FaceCullingData::Order order) override;

    // Framebuffer
    Framebuffer fbCreate(uint32_t width, uint32_t height) override;
    void        fbAttachTexture2D(uint32_t fbID, uint32_t texID, uint32_t attachment) override;
    void        fbAttachDepthTexture(uint32_t fbID, uint32_t texID) override;
    void        fbAttachTextureCubeFace(uint32_t fbID, uint32_t texID, int faceIndex) override;
    void        fbDestroy(uint32_t fbID) override;
    void
    cmdBindFramebuffer(uint32_t cmdID, uint32_t fbID, uint32_t width, uint32_t height) override;
    void cmdUnbindFramebuffer(uint32_t cmdID) override;
    void cmdClearFramebuffer(uint32_t cmdID,
                             float    r,
                             float    g,
                             float    b,
                             float    a,
                             bool     clearDepth   = true,
                             bool     clearStencil = false) override;

    // User callbacks
    void cmdExecuteCallback(uint32_t id, std::function<void()> callback) override;

    // Command buffer queue
    void                         cmdExecute(uint32_t id);
    void                         queueCommandBuffer(uint32_t cmdId);
    const std::vector<uint32_t>& getPendingSubmissions() const;
    void                         clearPendingSubmissions();

    // Buffer updates (deferred)
    void
    cmdUpdateVertexBuffer(uint32_t cmdID, uint32_t vboID, const void* data, uint32_t size) override;
    void cmdUpdateIndexBuffer(
        uint32_t cmdID, uint32_t iboID, const void* data, uint32_t count, bool index16) override;

    // Shader uniforms (deferred)
    void cmdSetShaderUniformMat4(uint32_t     cmdID,
                                 uint32_t     shaderID,
                                 const char*  name,
                                 const float* m16) override;
    void
    cmdSetShaderUniformInt(uint32_t cmdID, uint32_t shaderID, const char* name, int value) override;
    void cmdSetShaderUniformFloat(uint32_t    cmdID,
                                  uint32_t    shaderID,
                                  const char* name,
                                  float       value) override;
    void cmdSetShaderUniformVec3(uint32_t     cmdID,
                                 uint32_t     shaderID,
                                 const char*  name,
                                 const float* vec3) override;
    void cmdSetShaderUniformVec4(uint32_t     cmdID,
                                 uint32_t     shaderID,
                                 const char*  name,
                                 const float* vec4) override;
    void cmdSetShaderUniformVec2(uint32_t     cmdID,
                                 uint32_t     shaderID,
                                 const char*  name,
                                 const float* vec2) override;
    void cmdSetDepthMask(uint32_t id, bool enable) override;

private:
    struct CommandBufferData {
        std::vector<Command> commands;
        bool                 recording = false;
    };

    std::unordered_map<uint32_t, CommandBufferData> commandBuffers_;
    uint32_t                                        nextCmdBufferId_ = 1;
    std::vector<uint32_t>                           pendingSubmissions_;

    void executeCommand(const Command& cmd);
};

class OpenGLContext final : public GraphicsContext {
public:
    explicit OpenGLContext(Window* window);
    ~OpenGLContext() override;

    bool initialize(Window& window) override;
    void shutdown() override;

    void beginFrame() override;
    void endFrame() override;

    // Value-returning factories
    VertexBuffer  createVertexBuffer(const void* data, uint32_t size) override;
    IndexBuffer   createIndexBuffer(const void* indices, uint32_t count, bool index16) override;
    VertexArray   createVertexArray() override;
    Shader        createShader(const std::string& vs, const std::string& fs) override;
    Texture2D     createTexture2D(uint32_t w, uint32_t h) override;
    TextureCube   createTextureCube(uint32_t resolution) override;
    CommandBuffer createCommandBuffer() override;
    Texture2D     createDepthTexture(uint32_t width, uint32_t height) override;
    Framebuffer   createFramebuffer(uint32_t width, uint32_t height) override;

    GraphicsAPI getAPI() const override { return GraphicsAPI::OpenGL; }

    void flush() override;

private:
    Window*                        window { nullptr };
    std::unique_ptr<OpenGLBackend> backend;
    void                           attachBackend(HandleBase& h);
};

}
