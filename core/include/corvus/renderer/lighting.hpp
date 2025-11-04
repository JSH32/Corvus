#pragma once
#include "corvus/graphics/graphics.hpp"
#include "entt/entity/fwd.hpp"
#include <array>
#include <glm/glm.hpp>
#include <vector>

namespace Corvus::Renderer {

enum class LightType {
    Directional,
    Point,
    Spot
};

/**
 * Light data structure
 */
struct Light {
    LightType type = LightType::Directional;

    // Transform
    glm::vec3 position  = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);

    // Color and intensity
    glm::vec3 color     = glm::vec3(1.0f);
    float     intensity = 1.0f;

    // Point/Spot light properties
    float range = 10.0f;

    // Spot light properties
    float innerCutoff    = 12.5f; // degrees
    float outerCutoff    = 17.5f; // degrees
    int   shadowMapIndex = -1;

    // Shadow properties
    bool     castShadows         = false;
    uint32_t shadowMapResolution = 1024;
    float    shadowBias          = 0.005f;
    float    shadowStrength      = 1.0f;

    // Directional light shadow frustum
    float shadowDistance  = 50.0f;
    float shadowNearPlane = 0.1f;
    float shadowFarPlane  = 100.0f;
};

/**
 * Shadow map for directional/spot lights
 */
struct ShadowMap {
    Graphics::Framebuffer framebuffer;
    Graphics::Texture2D   depthTexture;
    glm::mat4             lightSpaceMatrix;
    uint32_t              resolution  = 1024;
    bool                  initialized = false;

    void initialize(Graphics::GraphicsContext& ctx, uint32_t res);
    void cleanup();
};

/**
 * Cubemap shadow for point lights
 */
struct CubemapShadow {
    Graphics::Framebuffer framebuffer;
    Graphics::TextureCube depthCubemap;
    glm::vec3             lightPosition;
    float                 farPlane    = 25.0f;
    uint32_t              resolution  = 1024;
    bool                  initialized = false;

    void initialize(Graphics::GraphicsContext& ctx, uint32_t res);
    void cleanup();
};

/**
 * Lighting manager - handles all lights and shadow rendering
 */
class LightingSystem {
public:
    static constexpr uint32_t MAX_LIGHTS        = 16;
    static constexpr uint32_t MAX_SHADOW_MAPS   = 4;
    static constexpr uint32_t MAX_POINT_SHADOWS = 4;

    LightingSystem() = default;
    ~LightingSystem();

    LightingSystem(const LightingSystem&)            = delete;
    LightingSystem& operator=(const LightingSystem&) = delete;
    LightingSystem(LightingSystem&&) noexcept;
    LightingSystem& operator=(LightingSystem&&) noexcept;

    /**
     * Initialize the lighting system
     */
    void initialize(Graphics::GraphicsContext& ctx);

    /**
     * Check if initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * Clear all lights for the frame
     */
    void clear();

    /**
     * Add a light to the scene
     */
    void addLight(const Light& light);

    /**
     * Set ambient lighting
     */
    void             setAmbientColor(const glm::vec3& color) { ambientColor_ = color; }
    const glm::vec3& getAmbientColor() const { return ambientColor_; }

    /**
     * Get all lights
     */
    std::vector<Light>& getLights() { return lights_; }

    /**
     * Get lights by type
     */
    std::vector<const Light*> getDirectionalLights() const;
    std::vector<const Light*> getPointLights() const;
    std::vector<const Light*> getSpotLights() const;

    /**
     * Get primary directional light (for sun/moon)
     */
    const Light* getPrimaryDirectionalLight() const;

    /**
     * Cull lights for a specific object (returns closest lights)
     */
    struct CulledLights {
        std::vector<const Light*> pointLights;
        std::vector<const Light*> spotLights;
    };
    CulledLights cullLightsForObject(const glm::vec3& position, float radius) const;

    /**
     * Get shadow maps (for SceneRenderer)
     */
    const std::vector<ShadowMap>& getShadowMaps() const { return shadowMaps_; }
    std::vector<ShadowMap>&       getShadowMaps() { return shadowMaps_; }

    /**
     * Get cubemap shadows (for SceneRenderer)
     */
    const std::vector<CubemapShadow>& getCubemapShadows() const { return cubemapShadows_; }
    std::vector<CubemapShadow>&       getCubemapShadows() { return cubemapShadows_; }

    /**
     * Prepare shadow maps for rendering (ensures they're initialized)
     */
    void prepareShadowMaps(Graphics::GraphicsContext& ctx);

    /**
     * Get the shadow shader (used for rendering shadow maps)
     */
    Graphics::Shader& getShadowShader();

    /**
     * Calculate light space matrix for directional light
     */
    glm::mat4 calculateDirectionalLightMatrix(const Light& light, const glm::vec3& sceneCenter);

    /**
     * Calculate light space matrix for spot light
     */
    glm::mat4 calculateSpotLightMatrix(const Light& light);

    /**
     * Calculate 6 view matrices for point light cubemap
     */
    std::array<glm::mat4, 6>
    calculatePointLightMatrices(const glm::vec3& lightPos, float nearPlane, float farPlane);

    /**
     * Apply lighting uniforms to a shader for a specific object
     */
    void applyLightingUniforms(Graphics::CommandBuffer& cmd,
                               Graphics::Shader&        shader,
                               const glm::vec3&         objectPosition,
                               float                    objectRadius,
                               const glm::vec3&         cameraPosition);

    /**
     * Bind shadow textures to shader
     */
    void bindShadowTextures(Graphics::CommandBuffer& cmd);

    /**
     * Set shadow properties (called by SceneRenderer after rendering shadow maps)
     */
    void setShadowProperties(const std::vector<float>& biases, const std::vector<float>& strengths);

    /**
     * Cleanup
     */
    void shutdown();

private:
    bool                       initialized_ = false;
    Graphics::GraphicsContext* context_     = nullptr;

    // Lights
    std::vector<Light> lights_;
    glm::vec3          ambientColor_ = glm::vec3(0.1f, 0.1f, 0.15f);

    // Shadow maps
    std::vector<ShadowMap>     shadowMaps_;
    std::vector<CubemapShadow> cubemapShadows_;

    // Shadow properties (stored per shadow map)
    std::vector<float> shadowBiases_;
    std::vector<float> shadowStrengths_;

    // Shadow shader
    Graphics::Shader shadowShader_;
    bool             shadowShaderInitialized_ = false;

    // Shadow map management
    ShadowMap&     getShadowMap(size_t index);
    CubemapShadow& getCubemapShadow(size_t index);

    // Helper to normalize color from 0-255 or 0-1 range to 0-1
    static glm::vec3 normalizeColor(const glm::vec3& color);
};

}
