#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Corvus::Renderer {

enum class ProjectionType {
    Perspective,
    Orthographic
};

class Camera {
public:
    Camera();
    Camera(const glm::vec3& position,
           const glm::vec3& target,
           const glm::vec3& up = glm::vec3(0, 1, 0));

    // Transform
    void setPosition(const glm::vec3& position);
    void setRotation(const glm::vec3& euler); // Pitch, Yaw, Roll in degrees
    void setRotation(const glm::quat& rotation);
    void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0, 1, 0));

    void             setTarget(const glm::vec3& target);
    void             setUp(const glm::vec3& up);
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getTarget() const { return target; }
    const glm::vec3& getUp() const { return up; }

    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUpDirection() const;

    // Projection
    void setPerspective(float fov, float aspectRatio, float nearPlane, float farPlane);
    void setOrthographic(
        float left, float right, float bottom, float top, float nearPlane, float farPlane);

    ProjectionType getProjectionType() const { return projectionType; }
    float          getFOV() const { return fov; }
    float          getAspectRatio() const { return aspectRatio; }
    float          getNearPlane() const { return nearPlane; }
    float          getFarPlane() const { return farPlane; }
    float          getOrthoSize() const { return orthoSize; }

    // Matrices
    const glm::mat4& getViewMatrix() const;
    const glm::mat4& getProjectionMatrix() const;
    glm::mat4        getViewProjectionMatrix() const;

    // Frustum culling (future use)
    struct Frustum {
        glm::vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far
    };
    const Frustum& getFrustum() const;

private:
    void updateViewMatrix() const;
    void updateProjectionMatrix() const;
    void updateFrustum() const;

    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;
    glm::quat rotation;
    bool      useLookAt = true;

    ProjectionType projectionType;

    // Perspective params
    float fov;
    float aspectRatio;

    // Orthographic params
    float orthoLeft   = 0;
    float orthoRight  = 0;
    float orthoBottom = 0;
    float orthoTop    = 0;
    float orthoSize;

    // Common params
    float nearPlane;
    float farPlane;

    // Cached matrices
    mutable glm::mat4 viewMatrix {};
    mutable glm::mat4 projectionMatrix {};
    mutable Frustum   frustum {};

    mutable bool viewDirty       = true;
    mutable bool projectionDirty = true;
    mutable bool frustumDirty    = true;
};

}
