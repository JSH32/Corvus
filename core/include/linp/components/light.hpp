#pragma once
#include "cereal/cereal.hpp"
#include "component_registry.hpp"
#include "linp/components/serializers.hpp"
#include <raylib.h>

namespace Linp::Core::Components {

enum class LightType {
    Directional,
    Point,
    Spot
};

struct LightComponent {
    LightType type      = LightType::Directional;
    Color     color     = WHITE;
    float     intensity = 1.0f;

    // Point/Spot light properties
    float range       = 10.0f;
    float attenuation = 1.0f;

    // Spot light properties
    float innerCutoff = 12.5f; // degrees
    float outerCutoff = 17.5f; // degrees

    bool enabled     = true;
    bool castShadows = true;

    // Shadow properties
    int   shadowMapResolution = 1024; // 512, 1024, 2048, 4096
    float shadowBias          = 0.005f;
    float shadowStrength      = 1.0f; // 0-1, how dark shadows are

    // Directional light shadow frustum
    float shadowDistance  = 50.0f; // How far shadows render
    float shadowNearPlane = 0.1f;
    float shadowFarPlane  = 100.0f;

    template <class Archive>
    void serialize(Archive& ar) {
        int lightTypeInt = static_cast<int>(type);
        ar(cereal::make_nvp("type", lightTypeInt));
        if constexpr (Archive::is_loading::value)
            type = static_cast<LightType>(lightTypeInt);

        ar(cereal::make_nvp("color", color));
        ar(cereal::make_nvp("intensity", intensity));
        ar(cereal::make_nvp("range", range));
        ar(cereal::make_nvp("attenuation", attenuation));
        ar(cereal::make_nvp("innerCutoff", innerCutoff));
        ar(cereal::make_nvp("outerCutoff", outerCutoff));
        ar(cereal::make_nvp("enabled", enabled));
        ar(cereal::make_nvp("castShadows", castShadows));
        ar(cereal::make_nvp("shadowMapResolution", shadowMapResolution));
        ar(cereal::make_nvp("shadowBias", shadowBias));
        ar(cereal::make_nvp("shadowStrength", shadowStrength));
        ar(cereal::make_nvp("shadowDistance", shadowDistance));
        ar(cereal::make_nvp("shadowNearPlane", shadowNearPlane));
        ar(cereal::make_nvp("shadowFarPlane", shadowFarPlane));
    }
};

REGISTER_COMPONENT(LightComponent, "Light");

}
