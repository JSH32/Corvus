#include "corvus/renderer/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Corvus::Renderer {

Camera::Camera()
    : position(0.0f, 0.0f, 5.0f), target(0.0f), up(0.0f, 1.0f, 0.0f),
      rotation(1.0f, 0.0f, 0.0f, 0.0f), projectionType(ProjectionType::Perspective), fov(45.0f),
      aspectRatio(16.0f / 9.0f), orthoSize(10.0f), nearPlane(0.1f), farPlane(1000.0f) { }

Camera::Camera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up)
    : position(position), target(target), up(up), rotation(1.0f, 0.0f, 0.0f, 0.0f),
      projectionType(ProjectionType::Perspective), fov(45.0f), aspectRatio(16.0f / 9.0f),
      orthoSize(10.0f), nearPlane(0.1f), farPlane(1000.0f) { }

void Camera::setPosition(const glm::vec3& position) {
    this->position = position;
    viewDirty      = true;
    frustumDirty   = true;
}

void Camera::setRotation(const glm::vec3& euler) {
    rotation     = glm::quat(glm::radians(euler));
    useLookAt    = false;
    viewDirty    = true;
    frustumDirty = true;
}

void Camera::setRotation(const glm::quat& rotation) {
    this->rotation = rotation;
    useLookAt      = false;
    viewDirty      = true;
    frustumDirty   = true;
}

void Camera::lookAt(const glm::vec3& target, const glm::vec3& up) {
    this->target = target;
    this->up     = up;
    useLookAt    = true;
    viewDirty    = true;
    frustumDirty = true;
}

glm::vec3 Camera::getForward() const {
    if (useLookAt) {
        return glm::normalize(target - position);
    }

    return glm::normalize(rotation * glm::vec3(0, 0, -1));
}

glm::vec3 Camera::getRight() const {
    return glm::normalize(glm::cross(getForward(), glm::vec3(0, 1, 0)));
}

glm::vec3 Camera::getUpDirection() const {
    return glm::normalize(glm::cross(getRight(), getForward()));
}

void Camera::setPerspective(float fov, float aspectRatio, float nearPlane, float farPlane) {
    this->projectionType  = ProjectionType::Perspective;
    this->fov             = fov;
    this->aspectRatio     = aspectRatio;
    this->nearPlane       = nearPlane;
    this->farPlane        = farPlane;
    this->projectionDirty = true;
    this->frustumDirty    = true;
}

void Camera::setOrthographic(const float left,
                             const float right,
                             const float bottom,
                             const float top,
                             float       nearPlane,
                             float       farPlane) {
    projectionType  = ProjectionType::Orthographic;
    orthoLeft       = left;
    orthoRight      = right;
    orthoBottom     = bottom;
    orthoTop        = top;
    this->nearPlane = nearPlane;
    this->farPlane  = farPlane;
    orthoSize       = (right - left) * 0.5f;
    projectionDirty = true;
    frustumDirty    = true;
}

const glm::mat4& Camera::getViewMatrix() const {
    if (viewDirty) {
        updateViewMatrix();
        viewDirty = false;
    }
    return viewMatrix;
}

const glm::mat4& Camera::getProjectionMatrix() const {
    if (projectionDirty) {
        updateProjectionMatrix();
        projectionDirty = false;
    }
    return projectionMatrix;
}

glm::mat4 Camera::getViewProjectionMatrix() const {
    return getProjectionMatrix() * getViewMatrix();
}

const Camera::Frustum& Camera::getFrustum() const {
    if (frustumDirty) {
        updateFrustum();
        frustumDirty = false;
    }
    return frustum;
}

void Camera::updateViewMatrix() const {
    if (useLookAt) {
        this->viewMatrix = glm::lookAt(position, target, up);
    } else {
        const glm::mat4 rotMat   = glm::toMat4(rotation);
        const glm::mat4 transMat = glm::translate(glm::mat4(1.0f), -position);
        this->viewMatrix         = rotMat * transMat;
    }
}

void Camera::updateProjectionMatrix() const {
    if (projectionType == ProjectionType::Perspective) {
        projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    } else {
        projectionMatrix
            = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
    }
}

void Camera::updateFrustum() const {
    glm::mat4 vp = getViewProjectionMatrix();

    // Extract frustum planes
    for (int i = 0; i < 6; ++i) {
        const int   row  = i / 2;
        const float sign = (i % 2 == 0) ? 1.0f : -1.0f;

        frustum.planes[i] = glm::vec4(vp[0][3] + sign * vp[0][row],
                                      vp[1][3] + sign * vp[1][row],
                                      vp[2][3] + sign * vp[2][row],
                                      vp[3][3] + sign * vp[3][row]);

        const float length = glm::length(glm::vec3(frustum.planes[i]));
        frustum.planes[i] /= length;
    }
}

void Camera::setTarget(const glm::vec3& target) {
    this->target    = target;
    this->useLookAt = true;
    this->viewDirty = true;
}

void Camera::setUp(const glm::vec3& up) {
    this->up        = up;
    this->viewDirty = true;
}

}
