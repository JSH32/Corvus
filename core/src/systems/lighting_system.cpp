#include "linp/systems/lighting_system.hpp"
#include "linp/scene.hpp"
#include <rlgl.h>

namespace Linp::Core::Systems {

void LightingSystem::initialize() { shadowManager.initialize(); }

void LightingSystem::clear() {
    directionalLights.clear();
    pointLights.clear();
    spotLights.clear();

    shadowCastingLightCount = 0;
    shadowBiases.clear();
    shadowStrengths.clear();
}

void LightingSystem::addLight(const LightData& light) {
    switch (light.type) {
        case LightType::Directional:
            directionalLights.push_back(light);
            break;
        case LightType::Point:
            pointLights.push_back(light);
            break;
        case LightType::Spot:
            spotLights.push_back(light);
            break;
    }
}

LightData LightingSystem::getPrimaryDirectionalLight() const {
    if (!directionalLights.empty()) {
        return directionalLights[0];
    }
    // Default light if none exists
    return LightData { .position  = { 0, 0, 0 },
                       .direction = Vector3Normalize({ 0.5f, -1.0f, 0.3f }),
                       .color     = WHITE,
                       .intensity = 1.0f,
                       .type      = LightType::Directional };
}

LightingSystem::CulledLights LightingSystem::cullLightsForObject(const Vector3& objectPos,
                                                                 float objectRadius) const {
    CulledLights culled;

    // Point lights
    struct LightDistance {
        LightData* light;
        float      distance;
    };

    std::vector<LightDistance> sortedPointLights;
    sortedPointLights.reserve(pointLights.size());

    for (size_t i = 0; i < pointLights.size(); ++i) {
        const auto& light = pointLights[i];
        float       dist  = Vector3Distance(objectPos, light.position);
        // Only consider lights within range
        if (dist <= light.range + objectRadius) {
            sortedPointLights.push_back({ const_cast<LightData*>(&light), dist });
        }
    }

    // Sort by distance (closest first)
    std::sort(
        sortedPointLights.begin(),
        sortedPointLights.end(),
        [](const LightDistance& a, const LightDistance& b) { return a.distance < b.distance; });

    // Take only the closest MAX_LIGHTS_PER_OBJECT
    for (size_t i = 0; i < sortedPointLights.size() && i < MAX_LIGHTS_PER_OBJECT; ++i) {
        culled.pointLights.push_back(sortedPointLights[i].light);
    }

    // Similar for spot lights
    std::vector<LightDistance> sortedSpotLights;
    sortedSpotLights.reserve(spotLights.size());

    for (size_t i = 0; i < spotLights.size(); ++i) {
        const auto& light = spotLights[i];
        float       dist  = Vector3Distance(objectPos, light.position);
        if (dist <= light.range + objectRadius) {
            sortedSpotLights.push_back({ const_cast<LightData*>(&light), dist });
        }
    }

    std::sort(
        sortedSpotLights.begin(),
        sortedSpotLights.end(),
        [](const LightDistance& a, const LightDistance& b) { return a.distance < b.distance; });

    for (size_t i = 0; i < sortedSpotLights.size()
         && culled.pointLights.size() + culled.spotLights.size() < MAX_LIGHTS_PER_OBJECT;
         ++i) {
        culled.spotLights.push_back(sortedSpotLights[i].light);
    }

    return culled;
}

void LightingSystem::renderShadowMaps(const std::vector<RenderableEntity>& renderables,
                                      Linp::Core::AssetManager*            assetMgr,
                                      Vector3                              sceneCenter) {
    if (!shadowManager.initialized) {
        shadowManager.initialize();
    }

    shadowCastingLightCount = 0;
    shadowBiases.clear();
    shadowStrengths.clear();

    // Process directional lights
    for (const auto& lightData : directionalLights) {
        if (!lightData.castShadows)
            continue;
        if (shadowCastingLightCount >= ShadowManager::MAX_SHADOW_MAPS)
            break;

        ShadowMap* shadowMap = shadowManager.getShadowMap(shadowCastingLightCount);
        shadowMap->initialize(lightData.shadowMapResolution);

        Matrix lightSpaceMatrix
            = shadowManager.calculateDirectionalLightMatrix(lightData.direction,
                                                            sceneCenter,
                                                            lightData.shadowDistance,
                                                            lightData.shadowNearPlane,
                                                            lightData.shadowFarPlane);

        shadowManager.renderShadowMap(shadowMap, lightSpaceMatrix, renderables, assetMgr);

        shadowBiases.push_back(lightData.shadowBias);
        shadowStrengths.push_back(lightData.shadowStrength);
        shadowCastingLightCount++;
    }

    // Process spot lights
    for (const auto& lightData : spotLights) {
        if (!lightData.castShadows)
            continue;
        if (shadowCastingLightCount >= ShadowManager::MAX_SHADOW_MAPS)
            break;

        ShadowMap* shadowMap = shadowManager.getShadowMap(shadowCastingLightCount);
        shadowMap->initialize(lightData.shadowMapResolution);

        Matrix lightSpaceMatrix = shadowManager.calculateSpotLightMatrix(
            lightData.position, lightData.direction, lightData.outerCutoff, 0.1f, lightData.range);

        shadowManager.renderShadowMap(shadowMap, lightSpaceMatrix, renderables, assetMgr);

        shadowBiases.push_back(lightData.shadowBias);
        shadowStrengths.push_back(lightData.shadowStrength);
        shadowCastingLightCount++;
    }
}

void LightingSystem::applyToShaderForObject(Shader&        shader,
                                            const Vector3& objectPos,
                                            float          objectRadius) const {
    // Ambient
    int ambientLoc = GetShaderLocation(shader, "u_AmbientColor");
    if (ambientLoc != -1) {
        float ambient[3]
            = { ambientColor.r / 255.0f, ambientColor.g / 255.0f, ambientColor.b / 255.0f };
        SetShaderValue(shader, ambientLoc, ambient, SHADER_UNIFORM_VEC3);
    }

    // Directional light (always applied, not culled)
    auto dirLight = getPrimaryDirectionalLight();

    int dirLoc = GetShaderLocation(shader, "u_DirLightDir");
    if (dirLoc != -1) {
        float dir[3] = { dirLight.direction.x, dirLight.direction.y, dirLight.direction.z };
        SetShaderValue(shader, dirLoc, dir, SHADER_UNIFORM_VEC3);
    }

    int colorLoc = GetShaderLocation(shader, "u_DirLightColor");
    if (colorLoc != -1) {
        float color[3] = { (dirLight.color.r / 255.0f) * dirLight.intensity,
                           (dirLight.color.g / 255.0f) * dirLight.intensity,
                           (dirLight.color.b / 255.0f) * dirLight.intensity };
        SetShaderValue(shader, colorLoc, color, SHADER_UNIFORM_VEC3);
    }

    // Cull lights for this specific object
    auto culledLights = cullLightsForObject(objectPos, objectRadius);

    // Point lights
    int pointCount    = static_cast<int>(culledLights.pointLights.size());
    int pointCountLoc = GetShaderLocation(shader, "u_PointLightCount");
    if (pointCountLoc != -1) {
        SetShaderValue(shader, pointCountLoc, &pointCount, SHADER_UNIFORM_INT);

        for (size_t i = 0; i < culledLights.pointLights.size(); ++i) {
            const auto* light = culledLights.pointLights[i];

            char locName[64];
            snprintf(locName, sizeof(locName), "u_PointLights[%zu].position", i);
            int loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                float pos[3] = { light->position.x, light->position.y, light->position.z };
                SetShaderValue(shader, loc, pos, SHADER_UNIFORM_VEC3);
            }

            snprintf(locName, sizeof(locName), "u_PointLights[%zu].color", i);
            loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                float color[3] = { (light->color.r / 255.0f) * light->intensity,
                                   (light->color.g / 255.0f) * light->intensity,
                                   (light->color.b / 255.0f) * light->intensity };
                SetShaderValue(shader, loc, color, SHADER_UNIFORM_VEC3);
            }

            snprintf(locName, sizeof(locName), "u_PointLights[%zu].range", i);
            loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                SetShaderValue(shader, loc, &light->range, SHADER_UNIFORM_FLOAT);
            }
        }
    }

    // Spot lights
    int spotCount    = static_cast<int>(culledLights.spotLights.size());
    int spotCountLoc = GetShaderLocation(shader, "u_SpotLightCount");
    if (spotCountLoc != -1) {
        SetShaderValue(shader, spotCountLoc, &spotCount, SHADER_UNIFORM_INT);

        for (size_t i = 0; i < culledLights.spotLights.size(); ++i) {
            const auto* light = culledLights.spotLights[i];

            char locName[64];
            snprintf(locName, sizeof(locName), "u_SpotLights[%zu].position", i);
            int loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                float pos[3] = { light->position.x, light->position.y, light->position.z };
                SetShaderValue(shader, loc, pos, SHADER_UNIFORM_VEC3);
            }

            snprintf(locName, sizeof(locName), "u_SpotLights[%zu].direction", i);
            loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                float dir[3] = { light->direction.x, light->direction.y, light->direction.z };
                SetShaderValue(shader, loc, dir, SHADER_UNIFORM_VEC3);
            }

            snprintf(locName, sizeof(locName), "u_SpotLights[%zu].color", i);
            loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                float color[3] = { (light->color.r / 255.0f) * light->intensity,
                                   (light->color.g / 255.0f) * light->intensity,
                                   (light->color.b / 255.0f) * light->intensity };
                SetShaderValue(shader, loc, color, SHADER_UNIFORM_VEC3);
            }

            snprintf(locName, sizeof(locName), "u_SpotLights[%zu].range", i);
            loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                SetShaderValue(shader, loc, &light->range, SHADER_UNIFORM_FLOAT);
            }

            snprintf(locName, sizeof(locName), "u_SpotLights[%zu].innerCutoff", i);
            loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                float cutoff = cosf(light->innerCutoff * DEG2RAD);
                SetShaderValue(shader, loc, &cutoff, SHADER_UNIFORM_FLOAT);
            }

            snprintf(locName, sizeof(locName), "u_SpotLights[%zu].outerCutoff", i);
            loc = GetShaderLocation(shader, locName);
            if (loc != -1) {
                float cutoff = cosf(light->outerCutoff * DEG2RAD);
                SetShaderValue(shader, loc, &cutoff, SHADER_UNIFORM_FLOAT);
            }
        }
    }

    // Shadow uniforms
    int shadowCountLoc = GetShaderLocation(shader, "u_ShadowMapCount");
    if (shadowCountLoc != -1) {
        // Count valid shadow maps
        int validShadowCount = 0;
        for (int i = 0; i < shadowCastingLightCount && i < static_cast<int>(shadowBiases.size());
             ++i) {
            if (i < static_cast<int>(shadowManager.shadowMaps.size()) && shadowManager.shadowMaps[i]
                && shadowManager.shadowMaps[i]->initialized
                && shadowManager.shadowMaps[i]->depthTexture.depth.id > 0) {
                validShadowCount++;
            }
        }

        // Set shadow count
        SetShaderValue(shader, shadowCountLoc, &validShadowCount, SHADER_UNIFORM_INT);

        // Only set shadow uniforms if we have valid shadows
        if (validShadowCount > 0) {
            int shadowIndex = 0;
            for (int i = 0;
                 i < shadowCastingLightCount && i < static_cast<int>(shadowBiases.size());
                 ++i) {
                // Skip invalid shadow maps
                if (i >= static_cast<int>(shadowManager.shadowMaps.size())
                    || !shadowManager.shadowMaps[i] || !shadowManager.shadowMaps[i]->initialized
                    || shadowManager.shadowMaps[i]->depthTexture.depth.id == 0) {
                    continue;
                }

                char locName[64];

                // Bind shadow map texture using rlgl
                snprintf(locName, sizeof(locName), "u_ShadowMaps[%d]", shadowIndex);
                int texLoc = GetShaderLocation(shader, locName);
                if (texLoc != -1) {
                    // Activate texture slot
                    rlActiveTextureSlot(3 + shadowIndex);
                    // Enable the depth texture
                    rlEnableTexture(shadowManager.shadowMaps[i]->depthTexture.depth.id);
                    // Set shader uniform to texture unit
                    int textureUnit = 3 + shadowIndex;
                    SetShaderValue(shader, texLoc, &textureUnit, SHADER_UNIFORM_INT);
                }

                // Set light space matrix
                snprintf(locName, sizeof(locName), "u_LightSpaceMatrices[%d]", shadowIndex);
                int matLoc = GetShaderLocation(shader, locName);
                if (matLoc != -1) {
                    SetShaderValueMatrix(
                        shader, matLoc, shadowManager.shadowMaps[i]->lightSpaceMatrix);
                }

                // Set shadow bias
                snprintf(locName, sizeof(locName), "u_ShadowBias[%d]", shadowIndex);
                int biasLoc = GetShaderLocation(shader, locName);
                if (biasLoc != -1) {
                    SetShaderValue(shader, biasLoc, &shadowBiases[i], SHADER_UNIFORM_FLOAT);
                }

                // Set shadow strength
                snprintf(locName, sizeof(locName), "u_ShadowStrength[%d]", shadowIndex);
                int strengthLoc = GetShaderLocation(shader, locName);
                if (strengthLoc != -1) {
                    SetShaderValue(shader, strengthLoc, &shadowStrengths[i], SHADER_UNIFORM_FLOAT);
                }

                shadowIndex++;
            }

            // Reset to default texture slot
            rlActiveTextureSlot(0);
        }
    }
}

void LightingSystem::applyToMaterial(raylib::Material* material,
                                     const Vector3&    objectPos,
                                     float             objectRadius) const {
    if (!material || material->shader.id == 0)
        return;
    applyToShaderForObject(material->shader, objectPos, objectRadius);
}

}
