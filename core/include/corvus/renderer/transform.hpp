#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Corvus::Renderer {

class Transform {
public:
    Transform();

    // Position
    void             setPosition(const glm::vec3& position);
    void             translate(const glm::vec3& delta);
    const glm::vec3& getPosition() const { return position_; }

    // Rotation
    void             setRotation(const glm::quat& rotation);
    void             setRotation(const glm::vec3& euler); // Degrees
    void             rotate(const glm::quat& delta);
    void             rotate(float angle, const glm::vec3& axis);
    const glm::quat& getRotation() const { return rotation_; }
    glm::vec3        getEulerAngles() const;

    // Scale
    void             setScale(const glm::vec3& scale);
    void             setScale(float uniformScale);
    const glm::vec3& getScale() const { return scale_; }

    // Matrix
    const glm::mat4& getMatrix() const;
    glm::mat4        getInverseMatrix() const;

    // Directions
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;

private:
    void markDirty() const { dirty_ = true; }
    void updateMatrix() const;

    glm::vec3 position_;
    glm::quat rotation_;
    glm::vec3 scale_;

    mutable glm::mat4 matrix_;
    mutable bool      dirty_;
};

}
