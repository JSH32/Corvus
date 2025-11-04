#include "corvus/renderer/model_generator.hpp"
#include "corvus/renderer/mesh.hpp"
#include <cmath>
#include <glm/glm.hpp>

namespace Corvus::Renderer::ModelGenerator {

static inline glm::vec3 faceNormal(glm::vec3 a, glm::vec3 b, glm::vec3 c) {
    return glm::normalize(glm::cross(b - a, c - a));
}

Model createCube(Graphics::GraphicsContext& ctx, float size) {
    Model model;

    const float h = size * 0.5f;

    // Define cube vertices (positions + normals + texcoords)
    std::vector<Vertex> vertices = {
        // Front (+Z)
        { { -h, -h, h }, { 0, 0, 1 }, { 0, 0 } },
        { { h, -h, h }, { 0, 0, 1 }, { 1, 0 } },
        { { h, h, h }, { 0, 0, 1 }, { 1, 1 } },
        { { -h, h, h }, { 0, 0, 1 }, { 0, 1 } },
        // Back (-Z)
        { { h, -h, -h }, { 0, 0, -1 }, { 0, 0 } },
        { { -h, -h, -h }, { 0, 0, -1 }, { 1, 0 } },
        { { -h, h, -h }, { 0, 0, -1 }, { 1, 1 } },
        { { h, h, -h }, { 0, 0, -1 }, { 0, 1 } },
        // Left (-X)
        { { -h, -h, -h }, { -1, 0, 0 }, { 0, 0 } },
        { { -h, -h, h }, { -1, 0, 0 }, { 1, 0 } },
        { { -h, h, h }, { -1, 0, 0 }, { 1, 1 } },
        { { -h, h, -h }, { -1, 0, 0 }, { 0, 1 } },
        // Right (+X)
        { { h, -h, h }, { 1, 0, 0 }, { 0, 0 } },
        { { h, -h, -h }, { 1, 0, 0 }, { 1, 0 } },
        { { h, h, -h }, { 1, 0, 0 }, { 1, 1 } },
        { { h, h, h }, { 1, 0, 0 }, { 0, 1 } },
        // Bottom (-Y)
        { { -h, -h, -h }, { 0, -1, 0 }, { 0, 0 } },
        { { h, -h, -h }, { 0, -1, 0 }, { 1, 0 } },
        { { h, -h, h }, { 0, -1, 0 }, { 1, 1 } },
        { { -h, -h, h }, { 0, -1, 0 }, { 0, 1 } },
        // Top (+Y)
        { { -h, h, h }, { 0, 1, 0 }, { 0, 0 } },
        { { h, h, h }, { 0, 1, 0 }, { 1, 0 } },
        { { h, h, -h }, { 0, 1, 0 }, { 1, 1 } },
        { { -h, h, -h }, { 0, 1, 0 }, { 0, 1 } },
    };

    // Reversed winding order (was 0,2,1 now 0,1,2)
    std::vector<uint32_t> indices = {
        0,  1,  2,  0,  2,  3,  // front
        4,  5,  6,  4,  6,  7,  // back
        8,  9,  10, 8,  10, 11, // left
        12, 13, 14, 12, 14, 15, // right
        16, 17, 18, 16, 18, 19, // bottom
        20, 21, 22, 20, 22, 23  // top
    };

    auto mesh = Mesh::createFromVertices(ctx, vertices, indices);
    model.addMesh(std::move(mesh));
    return model;
}

Model createPlane(Graphics::GraphicsContext& ctx, float width, float length) {
    Model       model;
    const float hw = width * 0.5f;
    const float hl = length * 0.5f;

    std::vector<Vertex> vertices = {
        { { -hw, 0, -hl }, { 0, 1, 0 }, { 0, 0 } },
        { { hw, 0, -hl }, { 0, 1, 0 }, { 1, 0 } },
        { { hw, 0, hl }, { 0, 1, 0 }, { 1, 1 } },
        { { -hw, 0, hl }, { 0, 1, 0 }, { 0, 1 } },
    };

    // Reversed winding order (was 0,1,2 now 0,2,1)
    std::vector<uint32_t> indices = { 0, 2, 1, 0, 3, 2 };

    auto mesh = Mesh::createFromVertices(ctx, vertices, indices);
    model.addMesh(std::move(mesh));
    return model;
}

Model createSphere(Graphics::GraphicsContext& ctx, float radius, uint32_t rings, uint32_t slices) {
    Model model;

    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    const float PI = 3.14159265359f;

    for (uint32_t r = 0; r <= rings; ++r) {
        float v   = float(r) / float(rings);
        float phi = v * PI;

        for (uint32_t s = 0; s <= slices; ++s) {
            float u     = float(s) / float(slices);
            float theta = u * (PI * 2.0f);

            glm::vec3 pos(radius * std::sin(phi) * std::cos(theta),
                          radius * std::cos(phi),
                          radius * std::sin(phi) * std::sin(theta));

            glm::vec3 n = glm::normalize(pos);
            vertices.push_back({ pos, n, { u, v } });
        }
    }

    // Reversed winding order
    for (uint32_t r = 0; r < rings; ++r) {
        for (uint32_t s = 0; s < slices; ++s) {
            uint32_t i0 = r * (slices + 1) + s;
            uint32_t i1 = i0 + slices + 1;

            // Was: i0, i1, i0+1, i1, i1+1, i0+1
            // Now reversed each triangle
            indices.insert(indices.end(), { i0, i0 + 1, i1, i1, i0 + 1, i1 + 1 });
        }
    }

    auto mesh = Mesh::createFromVertices(ctx, vertices, indices);
    model.addMesh(std::move(mesh));
    return model;
}

Model createCylinder(Graphics::GraphicsContext& ctx, float radius, float height, uint32_t slices) {
    Model                 model;
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    const float halfH = height * 0.5f;
    const float step  = (2.0f * M_PI) / slices;

    // Side vertices
    for (uint32_t i = 0; i <= slices; ++i) {
        float theta = step * i;
        float x     = std::cos(theta) * radius;
        float z     = std::sin(theta) * radius;
        float u     = float(i) / slices;

        glm::vec3 normal(std::cos(theta), 0, std::sin(theta));

        vertices.push_back({ { x, -halfH, z }, normal, { u, 0.0f } });
        vertices.push_back({ { x, halfH, z }, normal, { u, 1.0f } });
    }

    // Side indices (reversed)
    for (uint32_t i = 0; i < slices * 2; i += 2)
        indices.insert(indices.end(), { i, i + 3, i + 2, i, i + 1, i + 3 });

    // Top cap (reversed)
    uint32_t topCenterIndex = vertices.size();
    vertices.push_back({ { 0, halfH, 0 }, { 0, 1, 0 }, { 0.5f, 0.5f } });
    for (uint32_t i = 0; i <= slices; ++i) {
        float theta = step * i;
        float x     = std::cos(theta) * radius;
        float z     = std::sin(theta) * radius;
        float u     = (std::cos(theta) + 1.f) * 0.5f;
        float v     = (std::sin(theta) + 1.f) * 0.5f;
        vertices.push_back({ { x, halfH, z }, { 0, 1, 0 }, { u, v } });
    }
    for (uint32_t i = 0; i < slices; ++i)
        indices.insert(indices.end(),
                       { topCenterIndex, topCenterIndex + i + 2, topCenterIndex + i + 1 });

    // Bottom cap (reversed)
    uint32_t bottomCenterIndex = vertices.size();
    vertices.push_back({ { 0, -halfH, 0 }, { 0, -1, 0 }, { 0.5f, 0.5f } });
    for (uint32_t i = 0; i <= slices; ++i) {
        float theta = step * i;
        float x     = std::cos(theta) * radius;
        float z     = std::sin(theta) * radius;
        float u     = (std::cos(theta) + 1.f) * 0.5f;
        float v     = (std::sin(theta) + 1.f) * 0.5f;
        vertices.push_back({ { x, -halfH, z }, { 0, -1, 0 }, { u, v } });
    }
    for (uint32_t i = 0; i < slices; ++i)
        indices.insert(indices.end(),
                       { bottomCenterIndex, bottomCenterIndex + i + 1, bottomCenterIndex + i + 2 });

    auto mesh = Mesh::createFromVertices(ctx, vertices, indices);
    model.addMesh(std::move(mesh));
    return model;
}

}
