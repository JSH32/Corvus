#include "corvus/renderer/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Corvus::Renderer {

Camera::Camera()
    : position_(0.0f, 0.0f, 5.0f), target_(0.0f), up_(0.0f, 1.0f, 0.0f),
      rotation_(1.0f, 0.0f, 0.0f, 0.0f), projectionType_(ProjectionType::Perspective), fov_(45.0f),
      aspectRatio_(16.0f / 9.0f), orthoSize_(10.0f), nearPlane_(0.1f), farPlane_(1000.0f) { }

Camera::Camera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up)
    : position_(position), target_(target), up_(up), rotation_(1.0f, 0.0f, 0.0f, 0.0f),
      useLookAt_(true), projectionType_(ProjectionType::Perspective), fov_(45.0f),
      aspectRatio_(16.0f / 9.0f), orthoSize_(10.0f), nearPlane_(0.1f), farPlane_(1000.0f) { }

void Camera::setPosition(const glm::vec3& position) {
    position_     = position;
    viewDirty_    = true;
    frustumDirty_ = true;
}

void Camera::setRotation(const glm::vec3& euler) {
    rotation_     = glm::quat(glm::radians(euler));
    useLookAt_    = false;
    viewDirty_    = true;
    frustumDirty_ = true;
}

void Camera::setRotation(const glm::quat& rotation) {
    rotation_     = rotation;
    useLookAt_    = false;
    viewDirty_    = true;
    frustumDirty_ = true;
}

void Camera::lookAt(const glm::vec3& target, const glm::vec3& up) {
    target_       = target;
    up_           = up;
    useLookAt_    = true;
    viewDirty_    = true;
    frustumDirty_ = true;
}

glm::vec3 Camera::getForward() const {
    if (useLookAt_) {
        return glm::normalize(target_ - position_);
    } else {
        return glm::normalize(rotation_ * glm::vec3(0, 0, -1));
    }
}

glm::vec3 Camera::getRight() const {
    return glm::normalize(glm::cross(getForward(), glm::vec3(0, 1, 0)));
}

glm::vec3 Camera::getUpDirection() const {
    return glm::normalize(glm::cross(getRight(), getForward()));
}

void Camera::setPerspective(float fov, float aspectRatio, float nearPlane, float farPlane) {
    projectionType_  = ProjectionType::Perspective;
    fov_             = fov;
    aspectRatio_     = aspectRatio;
    nearPlane_       = nearPlane;
    farPlane_        = farPlane;
    projectionDirty_ = true;
    frustumDirty_    = true;
}

void Camera::setOrthographic(
    float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    projectionType_  = ProjectionType::Orthographic;
    orthoLeft_       = left;
    orthoRight_      = right;
    orthoBottom_     = bottom;
    orthoTop_        = top;
    nearPlane_       = nearPlane;
    farPlane_        = farPlane;
    orthoSize_       = (right - left) * 0.5f;
    projectionDirty_ = true;
    frustumDirty_    = true;
}

const glm::mat4& Camera::getViewMatrix() const {
    if (viewDirty_) {
        updateViewMatrix();
        viewDirty_ = false;
    }
    return viewMatrix_;
}

const glm::mat4& Camera::getProjectionMatrix() const {
    if (projectionDirty_) {
        updateProjectionMatrix();
        projectionDirty_ = false;
    }
    return projectionMatrix_;
}

glm::mat4 Camera::getViewProjectionMatrix() const {
    return getProjectionMatrix() * getViewMatrix();
}

const Camera::Frustum& Camera::getFrustum() const {
    if (frustumDirty_) {
        updateFrustum();
        frustumDirty_ = false;
    }
    return frustum_;
}

void Camera::updateViewMatrix() const {
    if (useLookAt_) {
        viewMatrix_ = glm::lookAt(position_, target_, up_);
    } else {
        glm::mat4 rotMat   = glm::toMat4(rotation_);
        glm::mat4 transMat = glm::translate(glm::mat4(1.0f), -position_);
        viewMatrix_        = rotMat * transMat;
    }
}

void Camera::updateProjectionMatrix() const {
    if (projectionType_ == ProjectionType::Perspective) {
        projectionMatrix_
            = glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
    } else {
        projectionMatrix_
            = glm::ortho(orthoLeft_, orthoRight_, orthoBottom_, orthoTop_, nearPlane_, farPlane_);
    }
}

void Camera::updateFrustum() const {
    glm::mat4 vp = getViewProjectionMatrix();

    // Extract frustum planes
    for (int i = 0; i < 6; ++i) {
        int   row  = i / 2;
        float sign = (i % 2 == 0) ? 1.0f : -1.0f;

        frustum_.planes[i] = glm::vec4(vp[0][3] + sign * vp[0][row],
                                       vp[1][3] + sign * vp[1][row],
                                       vp[2][3] + sign * vp[2][row],
                                       vp[3][3] + sign * vp[3][row]);

        float length = glm::length(glm::vec3(frustum_.planes[i]));
        frustum_.planes[i] /= length;
    }
}

void Camera::setTarget(const glm::vec3& target) {
    target_    = target;
    useLookAt_ = true;
    viewDirty_ = true;
}

void Camera::setUp(const glm::vec3& up) {
    up_        = up;
    viewDirty_ = true;
}

}
