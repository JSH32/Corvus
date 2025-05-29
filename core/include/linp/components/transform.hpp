#pragma once

#include <raylib-cpp.hpp>
#include <string>

namespace Linp::Core::Components {
struct TransformComponent {
    raylib::Vector3    position = raylib::Vector3(0.0f, 0.0f, 0.0f);
    raylib::Quaternion rotation = raylib::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    raylib::Vector3    scale    = raylib::Vector3(1.0f, 1.0f, 1.0f);

    raylib::Matrix getMatrix() const {
        auto translation = MatrixTranslate(position.x, position.y, position.z);
        auto rotationMat = QuaternionToMatrix(rotation);
        auto scaling     = MatrixScale(scale.x, scale.y, scale.z);
        return MatrixMultiply(MatrixMultiply(scaling, rotationMat), translation);
    }
};
}
