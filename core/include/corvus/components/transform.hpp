#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include "cereal/cereal.hpp"
#include "component_registry.hpp"
#include "corvus/components/serializers.hpp"
#include "entity_info.hpp"
#include <array>
#include <cereal/archives/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>

namespace Corvus::Core::Components {
struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale    = glm::vec3(1.0f);

    glm::mat4 getMatrix() const {
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rot   = glm::toMat4(rotation);
        glm::mat4 scl   = glm::scale(glm::mat4(1.0f), scale);
        return trans * rot * scl;
    }

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("position", position));
        ar(cereal::make_nvp("rotation", rotation));
        ar(cereal::make_nvp("scale", scale));
    }

    static TransformComponent fromMatrix(const glm::mat4& matrix) {
        TransformComponent t;

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(matrix, t.scale, t.rotation, t.position, skew, perspective);

        // glm::decompose() returns conjugate quaternion, so correct orientation
        t.rotation = glm::conjugate(t.rotation);

        return t;
    }
};

REGISTER_COMPONENT(TransformComponent, "Transform");

}
