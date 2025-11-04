// scene_renderer.hpp
#pragma once
#include "corvus/asset/asset_manager.hpp"
#include "corvus/components/entity_info.hpp"
#include "corvus/components/light.hpp"
#include "corvus/components/mesh_renderer.hpp"
#include "corvus/components/transform.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/camera.hpp"
#include "corvus/renderer/lighting.hpp"
#include "corvus/renderer/material_renderer.hpp"
#include "corvus/renderer/renderable.hpp"
#include <array>
#include <entt/entt.hpp>

namespace Corvus::Renderer {

struct RenderStats {
    uint32_t drawCalls        = 0;
    uint32_t triangles        = 0;
    uint32_t vertices         = 0;
    uint32_t entitiesRendered = 0;

    void reset() {
        drawCalls        = 0;
        triangles        = 0;
        vertices         = 0;
        entitiesRendered = 0;
    }
};

/**
 * Unified scene renderer with integrated lighting
 */
class SceneRenderer {
public:
    explicit SceneRenderer(Graphics::GraphicsContext& context);

    /**
     * Render a collection of renderables (low-level, fully manual)
     *
     * @param renderables Pure renderer renderables to draw
     * @param view View matrix
     * @param proj Projection matrix
     * @param cameraPos Camera world position
     * @param targetFB Optional framebuffer target (nullptr = screen)
     */
    void render(const std::vector<Renderable>& renderables,
                const glm::mat4&               view,
                const glm::mat4&               proj,
                const glm::vec3&               cameraPos,
                const Graphics::Framebuffer*   targetFB = nullptr);

    /**
     * Render with camera (convenience wrapper)
     */
    void render(const std::vector<Renderable>& renderables,
                const Camera&                  camera,
                const Graphics::Framebuffer*   targetFB = nullptr);

    /**
     * Render an entire ECS scene
     * This is a utility method that converts ECS components to renderables
     *
     * @param registry ECS registry containing entities
     * @param camera Camera to render from
     * @param assetManager Asset manager for loading resources
     * @param targetFB Optional framebuffer target (nullptr = screen)
     */
    void renderScene(entt::registry&              registry,
                     const Camera&                camera,
                     Core::AssetManager*          assetManager,
                     const Graphics::Framebuffer* targetFB = nullptr);

    /**
     * Get the lighting system
     */
    LightingSystem&       getLighting() { return lighting_; }
    const LightingSystem& getLighting() const { return lighting_; }

    /**
     * Clear all lights (call at start of frame)
     */
    void clearLights() { lighting_.clear(); }

    /**
     * Add a light to the scene
     */
    void addLight(const Light& light) { lighting_.addLight(light); }

    /**
     * Set ambient color
     */
    void setAmbientColor(const glm::vec3& color) { lighting_.setAmbientColor(color); }

    /**
     * Clear the render target
     */
    void clear(const glm::vec4&             color      = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f),
               bool                         clearDepth = true,
               const Graphics::Framebuffer* targetFB   = nullptr);

    /**
     * Get rendering statistics
     */
    const RenderStats& getStats() const { return stats_; }
    void               resetStats() { stats_.reset(); }

    /**
     * Direct access to graphics context (use sparingly)
     */
    Graphics::GraphicsContext& getContext() { return context_; }

    MaterialRenderer& getMaterialRenderer() { return materialRenderer_; };

private:
    // INTERNAL HELPERS

    void setupStandardUniforms(Graphics::CommandBuffer& cmd,
                               Graphics::Shader&        shader,
                               const glm::mat4&         model,
                               const glm::mat4&         view,
                               const glm::mat4&         proj);

    void setupLightingUniforms(Graphics::CommandBuffer& cmd,
                               Graphics::Shader&        shader,
                               const glm::vec3&         objectPos,
                               float                    objectRadius,
                               const glm::vec3&         cameraPos);

    /**
     * Render shadow maps for current lights
     */
    void renderShadowMaps(const std::vector<Renderable>& renderables);

    void renderDirectionalShadowMap(ShadowMap&                     shadowMap,
                                    const Light&                   light,
                                    const glm::mat4&               lightSpaceMatrix,
                                    const std::vector<Renderable>& renderables,
                                    Graphics::Shader&              shadowShader);

    void renderPointShadowMap(CubemapShadow&                  cubemap,
                              const Light&                    light,
                              const std::array<glm::mat4, 6>& lightMatrices,
                              const std::vector<Renderable>&  renderables,
                              Graphics::Shader&               shadowShader);

    // ECS CONVERSION HELPERS

    /**
     * Collect lights from ECS registry and add them to our lighting system
     */
    void collectLightsFromRegistry(entt::registry& registry);

    /**
     * Collect renderables from ECS registry
     */
    std::vector<Renderable> collectRenderables(entt::registry&     registry,
                                               Core::AssetManager* assetManager);

    Graphics::GraphicsContext& context_;
    RenderStats                stats_;
    MaterialRenderer           materialRenderer_;
    LightingSystem             lighting_; // Integrated lighting system
};

}
