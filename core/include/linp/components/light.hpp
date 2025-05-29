#pragma once
#include <raylib-cpp.hpp>

namespace Linp::Core::Components {

enum class LightType {
    Directional,
    Point,
    Spot
};

struct LightComponent {
    LightType type = LightType::Directional;

    // Light properties
    raylib::Color color     = WHITE;
    float         intensity = 1.0f;
    bool          enabled   = true;

    // Directional light properties
    raylib::Vector3 direction = raylib::Vector3(0.0f, -1.0f, 0.0f); // Default pointing down

    // Point/Spot light properties
    float range       = 10.0f;
    float attenuation = 1.0f;

    // Spot light specific
    float innerConeAngle = 45.0f; // degrees
    float outerConeAngle = 60.0f; // degrees

    // Shadow properties
    bool  castShadows = true;
    float shadowBias  = 0.001f;

    // Convert to raylib Light structure
    Light toRaylibLight(const raylib::Vector3& position = raylib::Vector3(0, 0, 0)) const {
        Light light = {};

        switch (type) {
            case LightType::Directional:
                light.type     = LIGHT_DIRECTIONAL;
                light.position = Vector3Zero(); // Not used for directional
                light.target   = Vector3Add(Vector3Zero(), direction);
                break;

            case LightType::Point:
                light.type     = LIGHT_POINT;
                light.position = position;
                light.target   = Vector3Zero(); // Not used for point
                break;

            case LightType::Spot:
                light.type     = LIGHT_SPOT;
                light.position = position;
                light.target   = Vector3Add(position, direction);
                break;
        }

        light.color       = color;
        light.enabled     = enabled ? 1 : 0;
        light.attenuation = attenuation;

        return light;
    }
};

}
