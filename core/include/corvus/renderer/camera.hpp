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
    const glm::vec3& getPosition() const { return position_; }
    const glm::vec3& getTarget() const { return target_; }
    const glm::vec3& getUp() const { return up_; }

    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUpDirection() const;

    // Projection
    void setPerspective(float fov, float aspectRatio, float nearPlane, float farPlane);
    void setOrthographic(
        float left, float right, float bottom, float top, float nearPlane, float farPlane);

    ProjectionType getProjectionType() const { return projectionType_; }
    float          getFOV() const { return fov_; }
    float          getAspectRatio() const { return aspectRatio_; }
    float          getNearPlane() const { return nearPlane_; }
    float          getFarPlane() const { return farPlane_; }
    float          getOrthoSize() const { return orthoSize_; }

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

    glm::vec3 position_;
    glm::vec3 target_;
    glm::vec3 up_;
    glm::quat rotation_;
    bool      useLookAt_ = true;

    ProjectionType projectionType_;

    // Perspective params
    float fov_;
    float aspectRatio_;

    // Orthographic params
    float orthoLeft_, orthoRight_;
    float orthoBottom_, orthoTop_;
    float orthoSize_;

    // Common params
    float nearPlane_;
    float farPlane_;

    // Cached matrices
    mutable glm::mat4 viewMatrix_;
    mutable glm::mat4 projectionMatrix_;
    mutable Frustum   frustum_;

    mutable bool viewDirty_       = true;
    mutable bool projectionDirty_ = true;
    mutable bool frustumDirty_    = true;
};

}
