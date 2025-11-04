#include "corvus/renderer/lighting.hpp"
#include "corvus/log.hpp"
#include "glm/gtc/epsilon.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace Corvus::Renderer {

// ShadowMap
void ShadowMap::initialize(Graphics::GraphicsContext& ctx, uint32_t res) {
    if (initialized && resolution == res) {
        return;
    }

    cleanup();
    resolution = res;

    depthTexture = ctx.createDepthTexture(res, res);
    framebuffer  = ctx.createFramebuffer(res, res);
    framebuffer.attachDepthTexture(depthTexture);

    initialized = true;
}

void ShadowMap::cleanup() {
    if (initialized) {
        framebuffer.release();
        depthTexture.release();
        initialized = false;
        resolution  = 0;
    }
}

// CubemapShadow
void CubemapShadow::initialize(Graphics::GraphicsContext& ctx, uint32_t res) {
    if (initialized && resolution == res) {
        return;
    }

    cleanup();
    resolution = res;

    depthCubemap = ctx.createTextureCube(res);
    framebuffer  = ctx.createFramebuffer(res, res);

    initialized = true;
}

void CubemapShadow::cleanup() {
    if (initialized) {
        framebuffer.release();
        depthCubemap.release();
        initialized = false;
        resolution  = 0;
    }
}

// LightingSystem
LightingSystem::~LightingSystem() { shutdown(); }

LightingSystem::LightingSystem(LightingSystem&& other) noexcept
    : initialized_(other.initialized_), context_(other.context_), lights_(std::move(other.lights_)),
      ambientColor_(other.ambientColor_), shadowMaps_(std::move(other.shadowMaps_)),
      cubemapShadows_(std::move(other.cubemapShadows_)),
      shadowShader_(std::move(other.shadowShader_)),
      shadowShaderInitialized_(other.shadowShaderInitialized_),
      shadowBiases_(std::move(other.shadowBiases_)),
      shadowStrengths_(std::move(other.shadowStrengths_)) {

    other.initialized_             = false;
    other.context_                 = nullptr;
    other.shadowShaderInitialized_ = false;
}

LightingSystem& LightingSystem::operator=(LightingSystem&& other) noexcept {
    if (this != &other) {
        shutdown();

        initialized_             = other.initialized_;
        context_                 = other.context_;
        lights_                  = std::move(other.lights_);
        ambientColor_            = other.ambientColor_;
        shadowMaps_              = std::move(other.shadowMaps_);
        cubemapShadows_          = std::move(other.cubemapShadows_);
        shadowShader_            = std::move(other.shadowShader_);
        shadowShaderInitialized_ = other.shadowShaderInitialized_;
        shadowBiases_            = std::move(other.shadowBiases_);
        shadowStrengths_         = std::move(other.shadowStrengths_);

        other.initialized_             = false;
        other.context_                 = nullptr;
        other.shadowShaderInitialized_ = false;
    }
    return *this;
}

void LightingSystem::initialize(Graphics::GraphicsContext& ctx) {
    if (initialized_) {
        return;
    }

    context_     = &ctx;
    initialized_ = true;

    CORVUS_CORE_INFO("LightingSystem initialized");
}

void LightingSystem::clear() {
    lights_.clear();
    shadowBiases_.clear();
    shadowStrengths_.clear();
}

void LightingSystem::addLight(const Light& light) { lights_.push_back(light); }

void LightingSystem::setShadowProperties(const std::vector<float>& biases,
                                         const std::vector<float>& strengths) {
    shadowBiases_    = biases;
    shadowStrengths_ = strengths;
}

glm::vec3 LightingSystem::normalizeColor(const glm::vec3& color) {
    if (color.r > 1.0f || color.g > 1.0f || color.b > 1.0f) {
        return color / 255.0f;
    }
    return color;
}

std::vector<const Light*> LightingSystem::getDirectionalLights() const {
    std::vector<const Light*> result;
    for (const auto& light : lights_) {
        if (light.type == LightType::Directional) {
            result.push_back(&light);
        }
    }
    return result;
}

std::vector<const Light*> LightingSystem::getPointLights() const {
    std::vector<const Light*> result;
    for (const auto& light : lights_) {
        if (light.type == LightType::Point) {
            result.push_back(&light);
        }
    }
    return result;
}

std::vector<const Light*> LightingSystem::getSpotLights() const {
    std::vector<const Light*> result;
    for (const auto& light : lights_) {
        if (light.type == LightType::Spot) {
            result.push_back(&light);
        }
    }
    return result;
}

const Light* LightingSystem::getPrimaryDirectionalLight() const {
    auto dirLights = getDirectionalLights();
    if (!dirLights.empty()) {
        return dirLights[0];
    }
    return nullptr;
}

LightingSystem::CulledLights LightingSystem::cullLightsForObject(const glm::vec3& position,
                                                                 float            radius) const {

    CulledLights result;

    struct LightDistance {
        const Light* light;
        float        distance;
    };

    // Point lights
    std::vector<LightDistance> pointLights;
    for (const auto& light : lights_) {
        if (light.type != LightType::Point)
            continue;

        float dist = glm::distance(position, light.position);
        if (dist <= light.range + radius) {
            pointLights.push_back({ &light, dist });
        }
    }

    // Sort by distance
    std::sort(
        pointLights.begin(), pointLights.end(), [](const LightDistance& a, const LightDistance& b) {
            return a.distance < b.distance;
        });

    // Take closest MAX_LIGHTS
    for (size_t i = 0; i < pointLights.size() && i < MAX_LIGHTS; ++i) {
        result.pointLights.push_back(pointLights[i].light);
    }

    // Spot lights
    std::vector<LightDistance> spotLights;
    for (const auto& light : lights_) {
        if (light.type != LightType::Spot)
            continue;

        float dist = glm::distance(position, light.position);
        if (dist <= light.range + radius) {
            spotLights.push_back({ &light, dist });
        }
    }

    std::sort(
        spotLights.begin(), spotLights.end(), [](const LightDistance& a, const LightDistance& b) {
            return a.distance < b.distance;
        });

    // Take closest remaining slots
    size_t remaining = MAX_LIGHTS - result.pointLights.size();
    for (size_t i = 0; i < spotLights.size() && i < remaining; ++i) {
        result.spotLights.push_back(spotLights[i].light);
    }

    return result;
}

ShadowMap& LightingSystem::getShadowMap(size_t index) {
    while (index >= shadowMaps_.size()) {
        shadowMaps_.emplace_back();
    }
    return shadowMaps_[index];
}

CubemapShadow& LightingSystem::getCubemapShadow(size_t index) {
    while (index >= cubemapShadows_.size()) {
        cubemapShadows_.emplace_back();
    }
    return cubemapShadows_[index];
}

void LightingSystem::prepareShadowMaps(Graphics::GraphicsContext& ctx) {
    if (!initialized_) {
        initialize(ctx);
    }

    // Clear old shadow maps if light configuration changed
    size_t neededShadowMaps = 0;
    size_t neededCubemaps   = 0;

    for (const auto& light : lights_) {
        if (!light.castShadows)
            continue;

        if (light.type == LightType::Directional || light.type == LightType::Spot) {
            if (neededShadowMaps < MAX_SHADOW_MAPS) {
                auto& shadowMap = getShadowMap(neededShadowMaps);
                shadowMap.initialize(ctx, light.shadowMapResolution);
                neededShadowMaps++;
            }
        } else if (light.type == LightType::Point) {
            if (neededCubemaps < MAX_POINT_SHADOWS) {
                auto& cubemap = getCubemapShadow(neededCubemaps);
                cubemap.initialize(ctx, light.shadowMapResolution);
                neededCubemaps++;
            }
        }
    }
}

Graphics::Shader& LightingSystem::getShadowShader() {
    if (!shadowShaderInitialized_ && context_) {
        // Simple shadow shader for depth rendering
        std::string vertexShader = R"(
            #version 330 core
            layout(location = 0) in vec3 vertexPosition;
            
            uniform mat4 u_LightSpaceMatrix;
            uniform mat4 u_Model;
            
            void main() {
                gl_Position = u_LightSpaceMatrix * u_Model * vec4(vertexPosition, 1.0);
            }
        )";

        std::string fragmentShader = R"(
            #version 330 core
            
            void main() {
                // Depth is written automatically
            }
        )";

        shadowShader_            = context_->createShader(vertexShader, fragmentShader);
        shadowShaderInitialized_ = shadowShader_.valid();

        if (shadowShaderInitialized_) {
            CORVUS_CORE_INFO("Shadow shader created successfully");
        } else {
            CORVUS_CORE_ERROR("Failed to create shadow shader");
        }
    }

    return shadowShader_;
}

glm::mat4 LightingSystem::calculateDirectionalLightMatrix(const Light&     light,
                                                          const glm::vec3& sceneCenter) {

    glm::vec3 lightDir = glm::normalize(light.direction);
    glm::vec3 lightPos = sceneCenter - lightDir * (light.shadowDistance * 0.5f);

    glm::vec3 up = (std::abs(glm::dot(lightDir, glm::vec3(0, 1, 0))) > 0.99f) ? glm::vec3(1, 0, 0)
                                                                              : glm::vec3(0, 1, 0);

    glm::mat4 view = glm::lookAt(lightPos, sceneCenter, up);

    float     half = light.shadowDistance * 0.5f;
    glm::mat4 proj
        = glm::ortho(-half, half, -half, half, light.shadowNearPlane, light.shadowFarPlane);

    return proj * view;
}

glm::mat4 LightingSystem::calculateSpotLightMatrix(const Light& light) {
    glm::vec3 lightDir = glm::normalize(light.direction);
    glm::vec3 up = (std::abs(glm::dot(lightDir, glm::vec3(0, 1, 0))) > 0.99f) ? glm::vec3(1, 0, 0)
                                                                              : glm::vec3(0, 1, 0);

    glm::mat4 view = glm::lookAt(light.position, light.position + lightDir, up);
    glm::mat4 proj = glm::perspective(glm::radians(light.outerCutoff * 1.1f),
                                      1.0f,
                                      std::max(0.5f, light.shadowNearPlane),
                                      light.range);

    return proj * view;
}

std::array<glm::mat4, 6> LightingSystem::calculatePointLightMatrices(const glm::vec3& lightPos,
                                                                     float            nearPlane,
                                                                     float            farPlane) {

    std::array<glm::mat4, 6> matrices;
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

    glm::vec3 directions[6]
        = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

    glm::vec3 ups[6]
        = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };

    for (int i = 0; i < 6; ++i) {
        glm::mat4 view = glm::lookAt(lightPos, lightPos + directions[i], ups[i]);
        matrices[i]    = proj * view;
    }

    return matrices;
}

void LightingSystem::applyLightingUniforms(Graphics::CommandBuffer& cmd,
                                           Graphics::Shader&        shader,
                                           const glm::vec3&         objectPosition,
                                           float                    objectRadius,
                                           const glm::vec3&         cameraPosition) {

    // Ambient color - normalize to 0-1 range
    shader.setVec3(cmd, "u_AmbientColor", normalizeColor(ambientColor_));

    // Camera position
    shader.setVec3(cmd, "u_ViewPos", cameraPosition);

    // Primary directional light
    auto* dirLight = getPrimaryDirectionalLight();
    if (dirLight) {
        shader.setVec3(cmd, "u_DirLightDir", glm::normalize(dirLight->direction));
        // Normalize color before multiplying by intensity
        shader.setVec3(
            cmd, "u_DirLightColor", normalizeColor(dirLight->color) * dirLight->intensity);
    } else {
        shader.setVec3(cmd, "u_DirLightDir", glm::vec3(0.0f));
        shader.setVec3(cmd, "u_DirLightColor", glm::vec3(0.0f));
    }

    // Cull lights for this object
    auto culled = cullLightsForObject(objectPosition, objectRadius);

    // Point lights
    shader.setInt(cmd, "u_PointLightCount", static_cast<int>(culled.pointLights.size()));
    for (size_t i = 0; i < culled.pointLights.size() && i < MAX_LIGHTS; ++i) {
        const auto* light = culled.pointLights[i];

        std::string base = "u_PointLights[" + std::to_string(i) + "].";
        shader.setVec3(cmd, (base + "position").c_str(), light->position);
        // Normalize color before multiplying by intensity
        shader.setVec3(
            cmd, (base + "color").c_str(), normalizeColor(light->color) * light->intensity);
        shader.setFloat(cmd, (base + "range").c_str(), light->range);
    }

    // Spot lights
    shader.setInt(cmd, "u_SpotLightCount", static_cast<int>(culled.spotLights.size()));
    for (size_t i = 0; i < culled.spotLights.size() && i < MAX_LIGHTS; ++i) {
        const auto* light = culled.spotLights[i];

        std::string base = "u_SpotLights[" + std::to_string(i) + "].";
        shader.setVec3(cmd, (base + "position").c_str(), light->position);
        shader.setVec3(cmd, (base + "direction").c_str(), glm::normalize(light->direction));
        shader.setVec3(
            cmd, (base + "color").c_str(), normalizeColor(light->color) * light->intensity);
        shader.setFloat(cmd, (base + "range").c_str(), light->range);
        shader.setFloat(
            cmd, (base + "innerCutoff").c_str(), std::cos(glm::radians(light->innerCutoff)));
        shader.setFloat(
            cmd, (base + "outerCutoff").c_str(), std::cos(glm::radians(light->outerCutoff)));
    }

    for (size_t i = 0; i < culled.spotLights.size() && i < MAX_LIGHTS; ++i) {
        const auto* light       = culled.spotLights[i];
        int         shadowIndex = -1;

        for (const auto& fullLight : lights_) {
            if (fullLight.type == LightType::Spot
                && glm::all(glm::epsilonEqual(fullLight.position, light->position, 0.0001f))) {
                shadowIndex = fullLight.shadowMapIndex;
                break;
            }
        }

        std::string uniformName = "u_SpotLightShadowIndices[" + std::to_string(i) + "]";
        shader.setInt(cmd, uniformName.c_str(), shadowIndex);
    }

    shader.setInt(cmd, "u_PointLightShadowCount", static_cast<int>(cubemapShadows_.size()));
    for (size_t i = 0; i < cubemapShadows_.size() && i < MAX_POINT_SHADOWS; ++i) {
        // Find which light this shadow belongs to (you may need to track this when rendering
        // shadows)
        const auto&  shadow = cubemapShadows_[i];
        const Light* light  = nullptr;

        // Find first matching point light (simple fallback)
        for (const auto& l : lights_) {
            if (l.type == LightType::Point && l.castShadows) {
                light = &l;
                break;
            }
        }

        if (light) {
            shader.setVec3(cmd,
                           ("u_PointLightShadowPositions[" + std::to_string(i) + "]").c_str(),
                           light->position);
            shader.setFloat(cmd,
                            ("u_PointLightShadowFarPlanes[" + std::to_string(i) + "]").c_str(),
                            light->range);
            shader.setInt(cmd, ("u_PointLightShadowIndices[" + std::to_string(i) + "]").c_str(), i);
        }
    }

    // Shadow uniforms with proper bias and strength
    size_t validShadows = 0;
    for (size_t i = 0; i < shadowMaps_.size() && i < MAX_SHADOW_MAPS; ++i) {
        if (shadowMaps_[i].initialized) {
            std::string matrixName = "u_LightSpaceMatrices[" + std::to_string(validShadows) + "]";
            shader.setMat4(cmd, matrixName.c_str(), shadowMaps_[i].lightSpaceMatrix);

            // Set bias and strength from stored values
            if (validShadows < shadowBiases_.size()) {
                std::string biasName = "u_ShadowBias[" + std::to_string(validShadows) + "]";
                shader.setFloat(cmd, biasName.c_str(), shadowBiases_[validShadows]);
            }

            if (validShadows < shadowStrengths_.size()) {
                std::string strengthName = "u_ShadowStrength[" + std::to_string(validShadows) + "]";
                shader.setFloat(cmd, strengthName.c_str(), shadowStrengths_[validShadows]);
            }

            validShadows++;
        }
    }
    shader.setInt(cmd, "u_ShadowMapCount", static_cast<int>(validShadows));
}

void LightingSystem::bindShadowTextures(Graphics::CommandBuffer& cmd) {
    uint32_t textureSlot = 3; // Reserve 0-2 for material textures

    // Directional & spot shadow maps
    for (size_t i = 0; i < shadowMaps_.size() && i < MAX_SHADOW_MAPS; ++i) {
        if (shadowMaps_[i].initialized) {
            std::string uniformName = "u_ShadowMaps[" + std::to_string(i) + "]";
            cmd.bindTexture(textureSlot, shadowMaps_[i].depthTexture, uniformName.c_str());
            textureSlot++;
        }
    }

    // Point light cubemap shadows
    for (size_t i = 0; i < cubemapShadows_.size() && i < MAX_POINT_SHADOWS; ++i) {
        if (cubemapShadows_[i].initialized) {
            std::string uniformName = "u_PointLightShadowMaps[" + std::to_string(i) + "]";
            cmd.bindTextureCube(textureSlot, cubemapShadows_[i].depthCubemap, uniformName);
            textureSlot++;
        }
    }
}

void LightingSystem::shutdown() {
    for (auto& sm : shadowMaps_) {
        sm.cleanup();
    }
    shadowMaps_.clear();

    for (auto& cm : cubemapShadows_) {
        cm.cleanup();
    }
    cubemapShadows_.clear();

    if (shadowShaderInitialized_) {
        shadowShader_.release();
        shadowShaderInitialized_ = false;
    }

    lights_.clear();
    shadowBiases_.clear();
    shadowStrengths_.clear();
    initialized_ = false;
    context_     = nullptr;
}
}
