#pragma once

#include "cereal/cereal.hpp"
#include "component_registry.hpp"
#include "entity_info.hpp"
#include "linp/components/serializers.hpp"
#include <array>
#include <cereal/archives/json.hpp>
#include <raylib-cpp.hpp>
#include <string>

namespace Linp::Core::Components {
struct TransformComponent : public Linp::Core::Components::SerializableComponent<TransformComponent> {
    raylib::Vector3    position = raylib::Vector3(0.0f, 0.0f, 0.0f);
    raylib::Quaternion rotation = raylib::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    raylib::Vector3    scale    = raylib::Vector3(1.0f, 1.0f, 1.0f);

    raylib::Matrix getMatrix() const {
        auto translation = MatrixTranslate(position.x, position.y, position.z);
        auto rotationMat = QuaternionToMatrix(rotation);
        auto scaling     = MatrixScale(scale.x, scale.y, scale.z);
        return MatrixMultiply(MatrixMultiply(scaling, rotationMat), translation);
    }

    TransformComponent() : SerializableComponent("Transform") { }

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("position", position));
        ar(cereal::make_nvp("rotation", rotation));
        ar(cereal::make_nvp("scale", scale));
    }

    static TransformComponent fromMatrix(const raylib::Matrix& matrix) {
        TransformComponent transform;

        // Extract translation (position) from the last column
        transform.position = raylib::Vector3(matrix.m12, matrix.m13, matrix.m14);

        // Extract scale by calculating the length of each basis vector
        raylib::Vector3 scaleX = raylib::Vector3(matrix.m0, matrix.m1, matrix.m2);
        raylib::Vector3 scaleY = raylib::Vector3(matrix.m4, matrix.m5, matrix.m6);
        raylib::Vector3 scaleZ = raylib::Vector3(matrix.m8, matrix.m9, matrix.m10);

        transform.scale.x = Vector3Length(scaleX);
        transform.scale.y = Vector3Length(scaleY);
        transform.scale.z = Vector3Length(scaleZ);

        // Handle negative scale (check determinant)
        float det = matrix.m0 * (matrix.m5 * matrix.m10 - matrix.m6 * matrix.m9) - matrix.m1 * (matrix.m4 * matrix.m10 - matrix.m6 * matrix.m8) + matrix.m2 * (matrix.m4 * matrix.m9 - matrix.m5 * matrix.m8);

        if (det < 0.0f) {
            transform.scale.x = -transform.scale.x;
        }

        // Remove scale from the matrix to get pure rotation
        raylib::Matrix rotationMatrix = matrix;

        // Normalize the basis vectors to remove scale
        if (transform.scale.x != 0.0f) {
            rotationMatrix.m0 /= transform.scale.x;
            rotationMatrix.m1 /= transform.scale.x;
            rotationMatrix.m2 /= transform.scale.x;
        }

        if (transform.scale.y != 0.0f) {
            rotationMatrix.m4 /= transform.scale.y;
            rotationMatrix.m5 /= transform.scale.y;
            rotationMatrix.m6 /= transform.scale.y;
        }

        if (transform.scale.z != 0.0f) {
            rotationMatrix.m8 /= transform.scale.z;
            rotationMatrix.m9 /= transform.scale.z;
            rotationMatrix.m10 /= transform.scale.z;
        }

        // Clear translation from rotation matrix
        rotationMatrix.m12 = 0.0f;
        rotationMatrix.m13 = 0.0f;
        rotationMatrix.m14 = 0.0f;
        rotationMatrix.m15 = 1.0f;

        // Convert rotation matrix to quaternion
        transform.rotation = QuaternionFromMatrix(rotationMatrix);

        return transform;
    }
};

}
