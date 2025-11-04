#pragma once
#include "glm/ext/matrix_float4x4.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Corvus::Graphics {

// API selection
enum class GraphicsAPI {
    OpenGL,
    Vulkan,
    DirectX12,
    Metal
};

enum class PrimitiveType {
    Triangles,
    Lines,
    LineStrip,
    Points
};

// Forward declarations
class Window;
struct VertexBuffer;
struct IndexBuffer;
struct VertexArray;
struct Shader;
struct Texture2D;
struct TextureCube;
struct Framebuffer;
struct CommandBuffer;
class GraphicsContext;

// Command types for recording
struct Command {
    enum class Type {
        SetViewport,
        SetShader,
        SetVAO,
        BindTexture,
        BindTextureCube,
        DrawIndexed,
        BindFramebuffer,
        UnbindFramebuffer,
        ClearFramebuffer,
        SetBlendState,
        SetDepthTest,
        SetCullFace,
        SetScissor,
        EnableScissor,
        UserCallback,
        UpdateVertexBuffer,
        UpdateIndexBuffer,
        SetShaderUniformMat4,
        SetShaderUniformInt,
        SetShaderUniformFloat,
        SetShaderUniformVec3,
        SetShaderUniformVec4,
        SetShaderUniformVec2,
        SetDepthMask,
        SetLineWidth
    };

    Type type;

    struct DepthMaskData {
        bool enable;
    };

    struct ViewportData {
        uint32_t x, y, w, h;
    };

    struct ShaderData {
        uint32_t shaderId;
    };

    struct VAOData {
        uint32_t vaoId;
    };

    struct TextureData {
        uint32_t slot, texId;
    };

    struct DrawIndexedData {
        uint32_t      elemCount;
        bool          index16;
        uint32_t      offset;
        PrimitiveType mode;
    };

    struct FramebufferData {
        uint32_t fbId;
        uint32_t width;
        uint32_t height;
    };

    struct ClearData {
        float r, g, b, a;
        bool  depth, stencil;
    };

    struct StateData {
        bool enable;
    };

    struct FaceCullingData {
        enum class Order {
            Clockwise,
            CounterClockwise
        };

        bool  enable;
        Order order;
    };

    struct LineWidthData {
        float width;
    };

    struct ScissorData {
        uint32_t x, y, w, h;
    };

    struct UserCallbackData {
        std::function<void()> callback;
    };

    struct UpdateVertexBufferData {
        uint32_t             vboId;
        std::vector<uint8_t> data;
    };

    struct UpdateIndexBufferData {
        uint32_t             iboId;
        std::vector<uint8_t> data;
        uint32_t             count;
        bool                 index16;
    };

    struct SetShaderUniformMat4Data {
        uint32_t    shaderId;
        std::string name;
        float       matrix[16];
    };

    struct SetShaderUniformIntData {
        uint32_t    shaderId;
        std::string name;
        int         value;
    };

    struct SetShaderUniformFloatData {
        uint32_t    shaderId;
        std::string name;
        float       value;
    };

    struct SetShaderUniformVec3Data {
        uint32_t    shaderId;
        std::string name;
        float       vec[3];
    };

    struct SetShaderUniformVec4Data {
        uint32_t    shaderId;
        std::string name;
        float       vec[4];
    };

    struct SetShaderUniformVec2Data {
        uint32_t    shaderId;
        std::string name;
        float       vec[2];
    };

    std::variant<ViewportData,
                 ShaderData,
                 VAOData,
                 TextureData,
                 DrawIndexedData,
                 FramebufferData,
                 ClearData,
                 StateData,
                 ScissorData,
                 UserCallbackData,
                 UpdateVertexBufferData,
                 UpdateIndexBufferData,
                 SetShaderUniformMat4Data,
                 SetShaderUniformIntData,
                 SetShaderUniformFloatData,
                 SetShaderUniformVec3Data,
                 SetShaderUniformVec4Data,
                 SetShaderUniformVec2Data,
                 DepthMaskData,
                 LineWidthData,
                 FaceCullingData,
                 std::monostate>
        data;
};

// Backend interface
class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;

    // Buffer creation/destroy
    virtual VertexBuffer vbCreate(const void* data, uint32_t size) = 0;
    virtual void         vbDestroy(uint32_t id)                    = 0;

    virtual IndexBuffer ibCreate(const void* indices, uint32_t count, bool index16) = 0;
    virtual void        ibDestroy(uint32_t id)                                      = 0;

    virtual VertexArray vaoCreate() = 0;
    virtual void        vaoAddVB(uint32_t                     vaoId,
                                 uint32_t                     vbId,
                                 const std::vector<uint32_t>& comps,
                                 const std::vector<bool>&     normalized,
                                 uint32_t                     stride)
        = 0;
    virtual void vaoSetIB(uint32_t vaoId, uint32_t ibId) = 0;
    virtual void vaoDestroy(uint32_t id)                 = 0;

    // Shader
    virtual Shader shaderCreate(const std::string& vs, const std::string& fs) = 0;
    virtual void   shaderDestroy(uint32_t id)                                 = 0;

    // Texture
    virtual Texture2D tex2DCreate(uint32_t w, uint32_t h)                             = 0;
    virtual Texture2D tex2DCreateDepth(uint32_t w, uint32_t h)                        = 0;
    virtual void      tex2DSetData(uint32_t id, const void* data, uint32_t sizeBytes) = 0;
    virtual void      tex2DDestroy(uint32_t id)                                       = 0;

    virtual TextureCube texCubeCreate(uint32_t resolution) = 0;
    virtual void        texCubeSetFaceData(
               uint32_t id, int faceIndex, const void* data, uint32_t resolution, uint32_t sizeBytes)
        = 0;
    virtual void texCubeDestroy(uint32_t id) = 0;

    // Command buffer + draw
    virtual CommandBuffer cmdCreate()                                                        = 0;
    virtual void          cmdBegin(uint32_t id)                                              = 0;
    virtual void          cmdEnd(uint32_t id)                                                = 0;
    virtual void          cmdSubmit(uint32_t id)                                             = 0;
    virtual void cmdSetViewport(uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h) = 0;
    virtual void cmdSetShader(uint32_t id, uint32_t shaderId)                                = 0;
    virtual void cmdSetVAO(uint32_t id, uint32_t vaoId)                                      = 0;
    virtual void cmdBindTexture(uint32_t id, uint32_t slot, uint32_t texId)                  = 0;
    virtual void cmdBindTextureCube(uint32_t cmdID, uint32_t slot, uint32_t texID)           = 0;
    virtual void cmdDrawIndexed(uint32_t      id,
                                uint32_t      elemCount,
                                bool          index16,
                                uint32_t      indexOffset = 0,
                                PrimitiveType primitive   = PrimitiveType::Triangles)
        = 0;

    // Framebuffer
    virtual Framebuffer fbCreate(uint32_t width, uint32_t height)                             = 0;
    virtual void        fbAttachTexture2D(uint32_t fbID, uint32_t texID, uint32_t attachment) = 0;
    virtual void        fbAttachTextureCubeFace(uint32_t fbID, uint32_t texID, int faceIndex) = 0;
    virtual void        fbDestroy(uint32_t fbID)                                              = 0;
    virtual void        fbAttachDepthTexture(uint32_t fbID, uint32_t texID)                   = 0;

    virtual void cmdBindFramebuffer(uint32_t cmdID, uint32_t fbID, uint32_t width, uint32_t height)
        = 0;
    virtual void cmdUnbindFramebuffer(uint32_t cmdID) = 0;
    virtual void cmdClearFramebuffer(uint32_t cmdID,
                                     float    r,
                                     float    g,
                                     float    b,
                                     float    a,
                                     bool     clearDepth   = true,
                                     bool     clearStencil = false)
        = 0;

    virtual void cmdSetScissor(uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h) = 0;
    virtual void cmdEnableScissor(uint32_t id, bool enable)                                 = 0;
    virtual void cmdSetBlendState(uint32_t id, bool enable)                                 = 0;
    virtual void cmdSetDepthTest(uint32_t id, bool enable)                                  = 0;
    virtual void cmdSetCullFace(uint32_t id, bool enable, Command::FaceCullingData::Order winding)
        = 0;

    // User callbacks
    virtual void cmdExecuteCallback(uint32_t id, std::function<void()> callback) = 0;

    // Buffer updates (deferred)
    virtual void
    cmdUpdateVertexBuffer(uint32_t cmdID, uint32_t vboID, const void* data, uint32_t size)
        = 0;
    virtual void cmdUpdateIndexBuffer(
        uint32_t cmdID, uint32_t iboID, const void* data, uint32_t count, bool index16)
        = 0;

    // Shader uniforms (deferred)
    virtual void
    cmdSetShaderUniformMat4(uint32_t cmdID, uint32_t shaderID, const char* name, const float* m16)
        = 0;
    virtual void
    cmdSetShaderUniformInt(uint32_t cmdID, uint32_t shaderID, const char* name, int value)
        = 0;
    virtual void
    cmdSetShaderUniformFloat(uint32_t cmdID, uint32_t shaderID, const char* name, float value)
        = 0;
    virtual void
    cmdSetShaderUniformVec3(uint32_t cmdID, uint32_t shaderID, const char* name, const float* vec3)
        = 0;
    virtual void
    cmdSetShaderUniformVec4(uint32_t cmdID, uint32_t shaderID, const char* name, const float* vec4)
        = 0;
    virtual void
    cmdSetShaderUniformVec2(uint32_t cmdID, uint32_t shaderID, const char* name, const float* vec2)
        = 0;

    virtual void cmdSetDepthMask(uint32_t id, bool enable) = 0;

    virtual void cmdSetLineWidth(uint32_t cmdId, float width) = 0;
};

// Vertex layout helpers
enum class ShaderDataType {
    None = 0,
    Float,
    Float2,
    Float3,
    Float4,
    Mat3,
    Mat4,
    Int,
    Int2,
    Int3,
    Int4,
    Byte,
    Bool
};

struct VertexElement {
    ShaderDataType type;
    uint32_t       count;
    bool           normalized;
};

class VertexBufferLayout {
public:
    VertexBufferLayout() = default;

    template <typename T>
    void push(uint32_t count);

    const std::vector<VertexElement>& getElements() const { return elements; }
    uint32_t                          getStride() const { return stride; }

private:
    std::vector<VertexElement> elements;
    uint32_t                   stride = 0;
};

uint32_t getSizeOfType(ShaderDataType type);
uint32_t getComponentCount(ShaderDataType type);

// Handle types
struct HandleBase {
    uint32_t          id { 0 };
    IGraphicsBackend* be { nullptr };
    bool              valid() const { return id != 0 && be != nullptr; }
};

struct VertexBuffer : HandleBase {
    uint32_t sizeBytes { 0 };

    void setData(CommandBuffer& cmd, const void* data, uint32_t size);
    void release();
};

struct IndexBuffer : HandleBase {
    uint32_t count { 0 };
    bool     index16 { true };

    void setData(CommandBuffer& cmd, const void* indices, uint32_t newCount, bool is16 = true);
    void release();
};

struct VertexArray : HandleBase {
    void addVertexBuffer(const VertexBuffer& vb, const VertexBufferLayout& layout);
    void setIndexBuffer(const IndexBuffer& ib);
    void release();
};

struct Shader : HandleBase {
    void setUniform(CommandBuffer& cmd, const char* name, const float* m16);
    void setMat4(CommandBuffer& cmd, const char* name, const float* m16);
    void setMat4(CommandBuffer& cmd, const char* name, const glm::mat4& m);
    void setInt(CommandBuffer& cmd, const char* name, int value);
    void setFloat(CommandBuffer& cmd, const char* name, float value);
    void setVec3(CommandBuffer& cmd, const char* name, const glm::vec3& v);
    void setVec2(CommandBuffer& cmd, const char* name, const glm::vec2& v);
    void setVec4(CommandBuffer& cmd, const char* name, const glm::vec4& v);
    void release();
};

struct Texture2D : HandleBase {
    uint32_t width { 0 }, height { 0 };

    void     setData(const void* data, uint32_t sizeBytes);
    uint64_t getNativeHandle() const { return id; }
    void     release();
};

struct TextureCube : HandleBase {
    uint32_t resolution { 0 };

    void release();
};

struct Framebuffer : HandleBase {
    uint32_t width { 0 };
    uint32_t height { 0 };

    void attachTexture2D(const Texture2D& tex, uint32_t attachment = 0);
    void attachTextureCubeFace(const TextureCube& tex, int faceIndex);
    void attachDepthTexture(const Texture2D& tex);
    void bind(uint32_t cmdID) const;
    void release();
};

struct CommandBuffer : HandleBase {
    void begin();
    void end();
    void submit();

    void setViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void setShader(const Shader& s);
    void setVertexArray(const VertexArray& v);
    void bindTexture(uint32_t slot, const Texture2D& t);
    void bindTextureCube(uint32_t slot, const TextureCube& t);
    void drawIndexed(uint32_t      elemCount,
                     bool          index16,
                     uint32_t      indexOffset = 0,
                     PrimitiveType primitive   = PrimitiveType::Triangles);
    void bindFramebuffer(const Framebuffer& fb);
    void unbindFramebuffer();
    void
    clear(float r, float g, float b, float a, bool clearDepth = true, bool clearStencil = false);
    void setBlendState(bool enable);
    void setDepthTest(bool enable);
    void setCullFace(bool enable, bool clockwise);
    void setScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void enableScissor(bool enable);
    void setDepthMask(bool enable);
    void setLineWidth(float width);
    void release();

    // User callbacks
    template <typename Func>
    void executeCallback(Func&& callback) {
        if (valid())
            be->cmdExecuteCallback(id, std::forward<Func>(callback));
    }

    // Buffer updates (deferred)
    void updateVertexBuffer(const VertexBuffer& vb, const void* data, uint32_t size);
    void updateIndexBuffer(const IndexBuffer& ib, const void* data, uint32_t count, bool index16);

    // Shader uniforms (deferred)
    void setShaderUniformMat4(const Shader& shader, const char* name, const float* m16);
    void setShaderUniformInt(const Shader& shader, const char* name, int value);
    void setShaderUniformFloat(const Shader& shader, const char* name, float value);
    void setShaderUniformVec3(const Shader& shader, const char* name, const float* vec3);
    void setShaderUniformVec4(const Shader& shader, const char* name, const float* vec4);
    void setShaderUniformVec2(const Shader& shader, const char* name, const float* vec2);
};

// Graphics context
class GraphicsContext {
public:
    virtual ~GraphicsContext() = default;

    virtual bool initialize(const Window& window) = 0;
    virtual void shutdown()                       = 0;

    virtual void beginFrame() = 0;
    virtual void endFrame()   = 0;

    virtual void setWindowSize(uint32_t width, uint32_t height) {
        this->windowHeight = height;
        this->windowWidth  = width;
    }

    // Value-returning factories
    virtual VertexBuffer  createVertexBuffer(const void* data, uint32_t size)                  = 0;
    virtual IndexBuffer   createIndexBuffer(const void* indices, uint32_t count, bool index16) = 0;
    virtual VertexArray   createVertexArray()                                                  = 0;
    virtual Shader        createShader(const std::string& vs, const std::string& fs)           = 0;
    virtual Texture2D     createTexture2D(uint32_t w, uint32_t h)                              = 0;
    virtual Texture2D     createDepthTexture(uint32_t width, uint32_t height)                  = 0;
    virtual TextureCube   createTextureCube(uint32_t resolution)                               = 0;
    virtual CommandBuffer createCommandBuffer()                                                = 0;
    virtual Framebuffer   createFramebuffer(uint32_t width, uint32_t height)                   = 0;

    virtual GraphicsAPI getAPI() const = 0;

    static std::unique_ptr<GraphicsContext> create(GraphicsAPI api);

    virtual void flush() = 0;

protected:
    uint32_t windowWidth  = 0;
    uint32_t windowHeight = 0;

    // Queue of command buffers to execute this frame
    std::vector<uint32_t> pendingCommandBuffers_;
};

// Template specializations for VertexBufferLayout
template <>
inline void VertexBufferLayout::push<float>(uint32_t count) {
    elements.push_back({ ShaderDataType::Float, count, false });
    stride += count * getSizeOfType(ShaderDataType::Float);
}

template <>
inline void VertexBufferLayout::push<uint32_t>(uint32_t count) {
    elements.push_back({ ShaderDataType::Int, count, false });
    stride += count * getSizeOfType(ShaderDataType::Int);
}

template <>
inline void VertexBufferLayout::push<uint8_t>(uint32_t count) {
    elements.push_back({ ShaderDataType::Byte, count, true });
    stride += count * getSizeOfType(ShaderDataType::Byte);
}

}
