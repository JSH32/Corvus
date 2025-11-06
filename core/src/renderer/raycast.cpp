#include "corvus/renderer/raycast.hpp"
#include "corvus/renderer/mesh.hpp"
#include "corvus/renderer/model.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

namespace Corvus::Geometry {

bool intersectTriangle(const Ray&       ray,
                       const glm::vec3& v0,
                       const glm::vec3& v1,
                       const glm::vec3& v2,
                       float&           outT,
                       glm::vec3&       outNormal) {
    constexpr float EPS = 1e-6f;
    const glm::vec3 e1  = v1 - v0;
    const glm::vec3 e2  = v2 - v0;
    const glm::vec3 p   = glm::cross(ray.direction, e2);
    const float     det = glm::dot(e1, p);
    if (fabs(det) < EPS)
        return false;

    const float     invDet = 1.0f / det;
    const glm::vec3 tvec   = ray.origin - v0;
    const float     u      = glm::dot(tvec, p) * invDet;
    if (u < 0.0f || u > 1.0f)
        return false;

    const glm::vec3 q = glm::cross(tvec, e1);
    if (const float v = glm::dot(ray.direction, q) * invDet; v < 0.0f || u + v > 1.0f)
        return false;

    const float t = glm::dot(e2, q) * invDet;
    if (t < EPS)
        return false;

    outT      = t;
    outNormal = glm::normalize(glm::cross(e1, e2));
    return true;
}

Ray buildRay(const glm::vec2& mouse,
             const glm::vec2& size,
             const glm::mat4& view,
             const glm::mat4& proj) {
    const glm::vec2 ndc((2.f * mouse.x) / size.x - 1.f, 1.f - (2.f * mouse.y) / size.y);
    const glm::mat4 invVP = glm::inverse(proj * view);
    glm::vec4       nearP = invVP * glm::vec4(ndc, 0.f, 1.f);
    glm::vec4       farP  = invVP * glm::vec4(ndc, 1.f, 1.f);
    nearP /= nearP.w;
    farP /= farP.w;
    Ray ray;
    ray.origin    = glm::vec3(nearP);
    ray.direction = glm::normalize(glm::vec3(farP - nearP));
    return ray;
}

template <typename VertexT>
bool intersectMesh(const Ray&                   rayLocal,
                   const std::vector<VertexT>&  vertices,
                   const std::vector<uint32_t>& indices,
                   RaycastHit&                  outHit) {
    bool  hitAny  = false;
    float closest = outHit.distance;

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        glm::vec3 n;
        if (float t = 0; intersectTriangle(rayLocal,
                                           vertices[indices[i]].position,
                                           vertices[indices[i + 1]].position,
                                           vertices[indices[i + 2]].position,
                                           t,
                                           n)) {
            if (t < closest) {
                closest           = t;
                outHit.hit        = true;
                outHit.distance   = t;
                outHit.normal     = n;
                outHit.triangleID = static_cast<int>(i / 3);
            }
            hitAny = true;
        }
    }
    return hitAny;
}

template <typename ModelT>
bool intersectModel(const ModelT&    model,
                    const glm::mat4& modelMatrix,
                    const Ray&       rayWorld,
                    RaycastHit&      outHit) {
    if (!model.valid())
        return false;

    glm::mat4 inv         = glm::inverse(modelMatrix);
    glm::vec3 localOrigin = glm::vec3(inv * glm::vec4(rayWorld.origin, 1.f));
    glm::vec3 localDir    = glm::normalize(glm::mat3(inv) * rayWorld.direction);

    Ray rayLocal { localOrigin, localDir };

    bool  hitAny  = false;
    float closest = outHit.distance;

    const auto& meshes = model.getMeshes();
    for (size_t m = 0; m < meshes.size(); ++m) {
        const auto& mesh = *meshes[m];
        if (!mesh.valid())
            continue;
        auto& vertices = mesh.getVertices();
        auto& indices  = mesh.getIndices();

        if (RaycastHit localHit;
            intersectMesh(rayLocal, vertices, indices, localHit) && localHit.distance < closest) {
            closest = localHit.distance;
            hitAny  = true;

            glm::vec3 localPos = rayLocal.origin + rayLocal.direction * localHit.distance;
            outHit.position    = glm::vec3(modelMatrix * glm::vec4(localPos, 1.0f));
            outHit.normal      = glm::normalize(glm::mat3(glm::transpose(inv)) * localHit.normal);
            outHit.distance    = glm::length(outHit.position - rayWorld.origin);
            outHit.hit         = true;
            outHit.meshIndex   = static_cast<int>(m);
            outHit.triangleID  = localHit.triangleID;
        }
    }
    return hitAny;
}

bool intersectModel(const Renderer::Mesh& mesh,
                    const glm::mat4&      modelMatrix,
                    const Ray&            rayWorld,
                    RaycastHit&           outHit) {
    if (!mesh.valid())
        return false;

    glm::mat4 inv         = glm::inverse(modelMatrix);
    glm::vec3 localOrigin = glm::vec3(inv * glm::vec4(rayWorld.origin, 1.f));
    glm::vec3 localDir    = glm::normalize(glm::mat3(inv) * rayWorld.direction);
    Ray       rayLocal { localOrigin, localDir };

    bool  hitAny  = false;
    float closest = outHit.distance;

    const auto& vertices = mesh.getVertices();
    const auto& vector   = mesh.getIndices();

    if (RaycastHit localHit;
        intersectMesh(rayLocal, vertices, vector, localHit) && localHit.distance < closest) {
        closest = localHit.distance;
        hitAny  = true;

        glm::vec3 localPos = rayLocal.origin + rayLocal.direction * localHit.distance;
        outHit.position    = glm::vec3(modelMatrix * glm::vec4(localPos, 1.0f));
        outHit.normal      = glm::normalize(glm::mat3(glm::transpose(inv)) * localHit.normal);
        outHit.distance    = glm::length(outHit.position - rayWorld.origin);
        outHit.hit         = true;
        outHit.triangleID  = localHit.triangleID;
    }

    return hitAny;
}

// explicit template instantiations for your standard vertex
template bool intersectMesh(const Ray&,
                            const std::vector<Renderer::Vertex>&,
                            const std::vector<uint32_t>&,
                            RaycastHit&);
template bool intersectModel(const Renderer::Model&, const glm::mat4&, const Ray&, RaycastHit&);

}
