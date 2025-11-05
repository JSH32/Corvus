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
    glm::vec3       e1  = v1 - v0;
    glm::vec3       e2  = v2 - v0;
    glm::vec3       p   = glm::cross(ray.direction, e2);
    float           det = glm::dot(e1, p);
    if (fabs(det) < EPS)
        return false;

    float     invDet = 1.0f / det;
    glm::vec3 tvec   = ray.origin - v0;
    float     u      = glm::dot(tvec, p) * invDet;
    if (u < 0.0f || u > 1.0f)
        return false;

    glm::vec3 q = glm::cross(tvec, e1);
    float     v = glm::dot(ray.direction, q) * invDet;
    if (v < 0.0f || u + v > 1.0f)
        return false;

    float t = glm::dot(e2, q) * invDet;
    if (t < EPS)
        return false;

    outT      = t;
    outNormal = glm::normalize(glm::cross(e1, e2));
    return true;
}

template <typename VertexT>
bool intersectMesh(const Ray&                   rayLocal,
                   const std::vector<VertexT>&  vertices,
                   const std::vector<uint32_t>& indices,
                   RaycastHit&                  outHit) {
    bool  hitAny  = false;
    float closest = outHit.distance;

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        float     t;
        glm::vec3 n;
        if (intersectTriangle(rayLocal,
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
        auto& verts = mesh.getVertices();
        auto& inds  = mesh.getIndices();

        RaycastHit localHit;
        if (intersectMesh(rayLocal, verts, inds, localHit) && localHit.distance < closest) {
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

    const auto& verts = mesh.getVertices();
    const auto& inds  = mesh.getIndices();

    RaycastHit localHit;
    if (intersectMesh(rayLocal, verts, inds, localHit) && localHit.distance < closest) {
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
