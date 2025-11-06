#include "corvus/renderer/mesh.hpp"
#include <cmath>
#include <cstring>
#include <utility>

namespace Corvus::Renderer {

Mesh::Mesh(GraphicsContext&          ctx,
           const void*               vertices,
           const uint32_t            vertexSize,
           const void*               indices,
           const uint32_t            indexCount,
           const bool                index16,
           const VertexBufferLayout& layout,
           const PrimitiveType       primitive)
    : indexCount(indexCount), index16(index16), primitiveType(primitive), layout(layout) {
    vbo = ctx.createVertexBuffer(vertices, vertexSize);
    ibo = ctx.createIndexBuffer(indices, indexCount, index16);
    vao = ctx.createVertexArray();

    vao.addVertexBuffer(vbo, layout);
    vao.setIndexBuffer(ibo);
}

Mesh Mesh::createFromVertices(GraphicsContext&             ctx,
                              const std::vector<Vertex>&   vertices,
                              const std::vector<uint32_t>& indices) {
    VertexBufferLayout layout;
    layout.push<float>(3);
    layout.push<float>(3);
    layout.push<float>(2);

    Mesh mesh(ctx,
              vertices.data(),
              vertices.size() * sizeof(Vertex),
              indices.data(),
              static_cast<uint32_t>(indices.size()),
              false,
              layout);

    mesh.vertices = vertices;
    mesh.indices  = indices;
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

    mesh.vertices.reserve(vertices.size());
    for (const auto& v : vertices)
        mesh.vertices.push_back({ v.position, v.normal, v.texCoord });
    mesh.indices = indices;

    return mesh;
}

void Mesh::updateVertices(CommandBuffer& cmd, const void* data, uint32_t size) {
    vbo.setData(cmd, data, size);
    if (!vertices.empty() && size == vertices.size() * sizeof(Vertex))
        std::memcpy(vertices.data(), data, size);
}

void Mesh::updateIndices(CommandBuffer& cmd, const void* data, uint32_t count, bool index16) {
    ibo.setData(cmd, data, count, index16);
    indexCount = count;
    index16    = index16;

    if (!indices.empty() && count == indices.size())
        std::memcpy(indices.data(), data, count * sizeof(uint32_t));
}

void Mesh::draw(CommandBuffer& cmd, bool wireframe) const {
    cmd.setVertexArray(vao);
    PrimitiveType prim = wireframe ? PrimitiveType::Lines : primitiveType;
    cmd.drawIndexed(indexCount, index16, 0, prim);
}

float Mesh::getBoundingRadius() const {
    if (vertices.empty())
        return 0.0f;
    float maxDist2 = 0.0f;
    for (const auto& v : vertices)
        maxDist2 = std::max(maxDist2, glm::dot(v.position, v.position));
    return std::sqrt(maxDist2);
}

glm::vec3 Mesh::getBoundingBoxMin() const {
    if (vertices.empty())
        return glm::vec3(0.0f);
    glm::vec3 min = vertices[0].position;
    for (const auto& v : vertices)
        min = glm::min(min, v.position);
    return min;
}

glm::vec3 Mesh::getBoundingBoxMax() const {
    if (vertices.empty())
        return glm::vec3(0.0f);
    glm::vec3 max = vertices[0].position;
    for (const auto& v : vertices)
        max = glm::max(max, v.position);
    return max;
}

bool Mesh::hasNormals() const {
    // Standard layouts always have normals as second element
    return layout.getElements().size() >= 2;
}

bool Mesh::hasTextureCoords() const {
    // Texcoords as 3rd element
    return layout.getElements().size() >= 3;
}

bool Mesh::hasColors() const {
    // Color 4th
    return layout.getElements().size() >= 4;
}

void Mesh::release() {
    vbo.release();
    ibo.release();
    vao.release();
    vertices.clear();
    indices.clear();
}

}
