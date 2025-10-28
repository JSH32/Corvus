#pragma once
#include "Material.hpp"
#include "linp/components/light.hpp"
#include "render_types.hpp"
#include "shadow_manager.hpp"
#include <algorithm>
#include <raylib.h>
#include <vector>

namespace Linp::Core {
class AssetManager;
}

namespace Linp::Core::Systems {

using LightType = Linp::Core::Components::LightType;

struct LightData {
    Vector3   position;
    Vector3   direction;
    Color     color;
    float     intensity;
    LightType type;
    float     range;
    float     attenuation;
    float     innerCutoff;
    float     outerCutoff;

    // Shadow properties
    bool  castShadows;
    int   shadowMapResolution;
    float shadowBias;
    float shadowStrength;
    float shadowDistance;
    float shadowNearPlane;
    float shadowFarPlane;
};

class LightingSystem {
public:
    static constexpr int MAX_LIGHTS_PER_OBJECT = 16;
    std::vector<int>     pointLightShadowIndices;
    static constexpr int MAX_POINT_LIGHT_SHADOWS = 4;

    // Collected light data for this frame
    std::vector<LightData> directionalLights;
    std::vector<LightData> pointLights;
    std::vector<LightData> spotLights;

    Color ambientColor = { 50, 50, 50, 255 };

    ShadowManager shadowManager;
    int           shadowCastingLightCount = 0;

    void      initialize();
    void      clear();
    void      addLight(const LightData& light);
    LightData getPrimaryDirectionalLight() const;

    struct CulledLights {
        std::vector<LightData*> pointLights;
        std::vector<LightData*> spotLights;
    };

    CulledLights cullLightsForObject(const Vector3& objectPos, float objectRadius = 5.0f) const;

    void renderShadowMaps(const std::vector<RenderableEntity>& renderables,
                          Linp::Core::AssetManager*            assetMgr,
                          Vector3                              sceneCenter = { 0, 0, 0 });

    void applyToShaderForObject(Shader&        shader,
                                const Vector3& objectPos,
                                float          objectRadius = 5.0f) const;

    void applyToMaterial(raylib::Material* material,
                         const Vector3&    objectPos,
                         float             objectRadius = 5.0f) const;

private:
    std::vector<float> shadowBiases;
    std::vector<float> shadowStrengths;
};

}
