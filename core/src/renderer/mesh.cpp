#include "corvus/renderer/mesh.hpp"
#include <cmath>
#include <cstring>

namespace Corvus::Renderer {

Mesh::Mesh(GraphicsContext&          ctx,
           const void*               vertices,
           uint32_t                  vertexSize,
           const void*               indices,
           uint32_t                  indexCount,
           bool                      index16,
           const VertexBufferLayout& layout,
           PrimitiveType             primitive)
    : indexCount_(indexCount), index16_(index16), primitiveType_(primitive),
      layout_(layout) // ✅ store a copy for inspection
{
    vbo_ = ctx.createVertexBuffer(vertices, vertexSize);
    ibo_ = ctx.createIndexBuffer(indices, indexCount, index16);
    vao_ = ctx.createVertexArray();

    vao_.addVertexBuffer(vbo_, layout_);
    vao_.setIndexBuffer(ibo_);
}

Mesh Mesh::createFromVertices(GraphicsContext&             ctx,
                              const std::vector<Vertex>&   vertices,
                              const std::vector<uint32_t>& indices) {
    VertexBufferLayout layout;
    layout.push<float>(3); // position
    layout.push<float>(3); // normal
    layout.push<float>(2); // texCoord

    Mesh mesh(ctx,
              vertices.data(),
              vertices.size() * sizeof(Vertex),
              indices.data(),
              static_cast<uint32_t>(indices.size()),
              false,
              layout);

    mesh.vertices_ = vertices;
    mesh.indices_  = indices;
    return mesh;
}

Mesh Mesh::createFromVerticesColor(GraphicsContext&                ctx,
                                   const std::vector<VertexColor>& vertices,
                                   const std::vector<uint32_t>&    indices) {
    VertexBufferLayout layout;
    layout.push<float>(3); // position
    layout.push<float>(3); // normal
    layout.push<float>(2); // texCoord
    layout.push<float>(4); // color

    Mesh mesh(ctx,
              vertices.data(),
              vertices.size() * sizeof(VertexColor),
              indices.data(),
              static_cast<uint32_t>(indices.size()),
              false,
              layout);

    mesh.vertices_.reserve(vertices.size());
    for (const auto& v : vertices)
        mesh.vertices_.push_back({ v.position, v.normal, v.texCoord });
    mesh.indices_ = indices;

    return mesh;
}

void Mesh::updateVertices(CommandBuffer& cmd, const void* data, uint32_t size) {
    vbo_.setData(cmd, data, size);
    if (!vertices_.empty() && size == vertices_.size() * sizeof(Vertex))
        std::memcpy(vertices_.data(), data, size);
}

void Mesh::updateIndices(CommandBuffer& cmd, const void* data, uint32_t count, bool index16) {
    ibo_.setData(cmd, data, count, index16);
    indexCount_ = count;
    index16_    = index16;

    if (!indices_.empty() && count == indices_.size())
        std::memcpy(indices_.data(), data, count * sizeof(uint32_t));
}

void Mesh::draw(CommandBuffer& cmd, bool wireframe) const {
    cmd.setVertexArray(vao_);
    PrimitiveType prim = wireframe ? PrimitiveType::Lines : primitiveType_;
    cmd.drawIndexed(indexCount_, index16_, 0, prim);
}

float Mesh::getBoundingRadius() const {
    if (vertices_.empty())
        return 0.0f;
    float maxDist2 = 0.0f;
    for (const auto& v : vertices_)
        maxDist2 = std::max(maxDist2, glm::dot(v.position, v.position));
    return std::sqrt(maxDist2);
}

glm::vec3 Mesh::getBoundingBoxMin() const {
    if (vertices_.empty())
        return glm::vec3(0.0f);
    glm::vec3 min = vertices_[0].position;
    for (const auto& v : vertices_)
        min = glm::min(min, v.position);
    return min;
}

glm::vec3 Mesh::getBoundingBoxMax() const {
    if (vertices_.empty())
        return glm::vec3(0.0f);
    glm::vec3 max = vertices_[0].position;
    for (const auto& v : vertices_)
        max = glm::max(max, v.position);
    return max;
}

bool Mesh::hasNormals() const {
    // Standard layouts always have normals as second element
    return layout_.getElements().size() >= 2;
}

bool Mesh::hasTexcoords() const {
    // Texcoords as 3rd element
    return layout_.getElements().size() >= 3;
}

bool Mesh::hasColors() const {
    // Color 4th
    return layout_.getElements().size() >= 4;
}

void Mesh::release() {
    vbo_.release();
    ibo_.release();
    vao_.release();
    vertices_.clear();
    indices_.clear();
}

}
