#pragma once
#include "corvus/graphics/graphics.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace Corvus::Renderer {

using Graphics::CommandBuffer;
using Graphics::GraphicsContext;
using Graphics::IndexBuffer;
using Graphics::PrimitiveType;
using Graphics::VertexArray;
using Graphics::VertexBuffer;
using Graphics::VertexBufferLayout;

// Standard vertex formats
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct VertexColor {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec4 color;
};

class Mesh {
public:
    Mesh(GraphicsContext&          ctx,
         const void*               vertices,
         uint32_t                  vertexSize,
         const void*               indices,
         uint32_t                  indexCount,
         bool                      index16,
         const VertexBufferLayout& layout,
         PrimitiveType             primitive = PrimitiveType::Triangles);

    // Factory helpers for standard vertex layouts
    static Mesh createFromVertices(GraphicsContext&             ctx,
                                   const std::vector<Vertex>&   vertices,
                                   const std::vector<uint32_t>& indices);

    static Mesh createFromVerticesColor(GraphicsContext&                ctx,
                                        const std::vector<VertexColor>& vertices,
                                        const std::vector<uint32_t>&    indices);

    // GPU updates
    void updateVertices(CommandBuffer& cmd, const void* data, uint32_t size);
    void updateIndices(CommandBuffer& cmd, const void* data, uint32_t count, bool index16);

    // Draw command
    void draw(CommandBuffer& cmd, bool wireframe = false) const;

    // Metadata
    bool               valid() const { return indexCount_ > 0 && vao_.valid(); }
    uint32_t           getIndexCount() const { return indexCount_; }
    uint32_t           getVertexCount() const { return static_cast<uint32_t>(vertices_.size()); }
    PrimitiveType      getPrimitiveType() const { return primitiveType_; }
    const VertexArray& getVAO() const { return vao_; }

    const std::vector<Vertex>&   getVertices() const { return vertices_; }
    const std::vector<uint32_t>& getIndices() const { return indices_; }

    // Bounding info
    float     getBoundingRadius() const;
    glm::vec3 getBoundingBoxMin() const;
    glm::vec3 getBoundingBoxMax() const;

    // Layout access
    const VertexBufferLayout& getLayout() const { return layout_; }

    // Attribute presence flags (for ImGui/info panels)
    bool hasNormals() const;
    bool hasTexcoords() const;
    bool hasColors() const;

    void release();

private:
    VertexBuffer       vbo_;
    IndexBuffer        ibo_;
    VertexArray        vao_;
    uint32_t           indexCount_    = 0;
    bool               index16_       = false;
    PrimitiveType      primitiveType_ = PrimitiveType::Triangles;
    VertexBufferLayout layout_;

    std::vector<Vertex>   vertices_;
    std::vector<uint32_t> indices_;
};

}
