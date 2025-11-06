#include "corvus/renderer/transform.hpp"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Corvus::Renderer {

Transform::Transform()
    : position_(0.0f), rotation_(glm::quat(1, 0, 0, 0)), scale_(1.0f), matrix_(1.0f), dirty_(true) {
}

void Transform::setPosition(const glm::vec3& position) {
    position_ = position;
    markDirty();
}

void Transform::translate(const glm::vec3& delta) {
    position_ += delta;
    markDirty();
}

void Transform::setRotation(const glm::quat& rotation) {
    rotation_ = rotation;
    markDirty();
}

void Transform::setRotation(const glm::vec3& euler) {
    rotation_ = glm::quat(glm::radians(euler));
    markDirty();
}

void Transform::rotate(const glm::quat& delta) {
    rotation_ = delta * rotation_;
    markDirty();
}

void Transform::rotate(float angle, const glm::vec3& axis) {
    rotation_ = glm::angleAxis(glm::radians(angle), glm::normalize(axis)) * rotation_;
    markDirty();
}

glm::vec3 Transform::getEulerAngles() const { return glm::degrees(glm::eulerAngles(rotation_)); }

void Transform::setScale(const glm::vec3& scale) {
    scale_ = scale;
    markDirty();
}

void Transform::setScale(float uniformScale) {
    scale_ = glm::vec3(uniformScale);
    markDirty();
}

const glm::mat4& Transform::getMatrix() const {
    if (dirty_) {
        updateMatrix();
        dirty_ = false;
    }
    return matrix_;
}

glm::mat4 Transform::getInverseMatrix() const { return glm::inverse(getMatrix()); }

glm::vec3 Transform::getForward() const {
    return glm::normalize(glm::vec3(getMatrix() * glm::vec4(0, 0, -1, 0)));
}

glm::vec3 Transform::getRight() const {
    return glm::normalize(glm::vec3(getMatrix() * glm::vec4(1, 0, 0, 0)));
}

glm::vec3 Transform::getUp() const {
    return glm::normalize(glm::vec3(getMatrix() * glm::vec4(0, 1, 0, 0)));
}

void Transform::updateMatrix() const {
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), position_);
    const glm::mat4 rotation    = glm::toMat4(rotation_);
    const glm::mat4 scale       = glm::scale(glm::mat4(1.0f), scale_);

    matrix_ = translation * rotation * scale;
}

}
