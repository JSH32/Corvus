#pragma once

#include "corvus/renderer/mesh.hpp"
#include "entt/entity/fwd.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include <limits>

namespace Corvus::Geometry {

struct Ray {
    glm::vec3 origin { 0.0f };
    glm::vec3 direction { 0.0f };
};

struct RaycastHit {
    bool      hit        = false;
    float     distance   = std::numeric_limits<float>::max();
    glm::vec3 position   = {};
    glm::vec3 normal     = {};
    int       meshIndex  = -1;
    int       triangleID = -1;
};

Ray buildRay(const glm::vec2& mouse,
             const glm::vec2& size,
             const glm::mat4& view,
             const glm::mat4& proj);

bool intersectTriangle(const Ray&       ray,
                       const glm::vec3& v0,
                       const glm::vec3& v1,
                       const glm::vec3& v2,
                       float&           outT,
                       glm::vec3&       outNormal);

template <typename VertexT>
bool intersectMesh(const Ray&                   rayLocal,
                   const std::vector<VertexT>&  vertices,
                   const std::vector<uint32_t>& indices,
                   RaycastHit&                  outHit);

template <typename ModelT>
bool intersectModel(const ModelT&    model,
                    const glm::mat4& modelMatrix,
                    const Ray&       rayWorld,
                    RaycastHit&      outHit);

bool intersectModel(const Renderer::Mesh& mesh,
                    const glm::mat4&      modelMatrix,
                    const Ray&            rayWorld,
                    RaycastHit&           outHit);

}
