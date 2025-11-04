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
 * Renderable entity data for rendering passes
 */
struct RenderableEntity {
    Core::Components::MeshRendererComponent* meshRenderer = nullptr;
    Core::Components::TransformComponent*    transform    = nullptr;
    bool                                     isEnabled    = true;
};

/**
 * Unified scene renderer that handles everything:
 * - Camera management
 * - Lighting uniforms
 * - Shadow maps
 * - Material binding
 * - Mesh rendering
 *
 * This is the main rendering interface - no direct graphics calls needed!
 */
class SceneRenderer {
public:
    explicit SceneRenderer(Graphics::GraphicsContext& context);

    /**
     * Render an entire scene with ECS registry
     *
     * @param registry ECS registry containing entities
     * @param camera Camera to render from
     * @param assetManager Asset manager for loading resources
     * @param lightingSystem Lighting system for lights and shadows
     * @param targetFB Optional framebuffer target (nullptr = screen)
     */
    void render(entt::registry&              registry,
                const Camera&                camera,
                Core::AssetManager*          assetManager,
                LightingSystem*              lightingSystem = nullptr,
                const Graphics::Framebuffer* targetFB       = nullptr);

    // Mid-level camera-based renderer (no ECS, manual renderables)
    void render(const std::vector<RenderableEntity>& renderables,
                const Camera&                        camera,
                Core::AssetManager*                  assetManager,
                LightingSystem*                      lightingSystem = nullptr,
                const Graphics::Framebuffer*         targetFB       = nullptr);

    // Low-level, fully manual variant (explicit matrices)
    void render(const std::vector<RenderableEntity>& renderables,
                const glm::mat4&                     view,
                const glm::mat4&                     proj,
                const glm::vec3&                     cameraPos,
                Core::AssetManager*                  assetManager,
                LightingSystem*                      lightingSystem = nullptr,
                const Graphics::Framebuffer*         targetFB       = nullptr);

    /**
     * Clear the render target
     *
     * @param color Clear color
     * @param clearDepth Whether to clear depth buffer
     */
    void clear(const glm::vec4& color = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), bool clearDepth = true);

    /**
     * Get rendering statistics
     */
    const RenderStats& getStats() const { return stats_; }
    void               resetStats() { stats_.reset(); }

    /**
     * Direct access to graphics context (use sparingly)
     */
    Graphics::GraphicsContext& getContext() { return context_; }

private:
    /**
     * Setup standard transform uniforms (mvp, model, normal matrix)
     */
    void setupStandardUniforms(Graphics::CommandBuffer& cmd,
                               Graphics::Shader&        shader,
                               const glm::mat4&         model,
                               const glm::mat4&         view,
                               const glm::mat4&         proj);

    /**
     * Setup lighting uniforms for a specific object
     */
    void setupLightingUniforms(Graphics::CommandBuffer& cmd,
                               Graphics::Shader&        shader,
                               LightingSystem*          lightingSystem,
                               const glm::vec3&         objectPos,
                               float                    objectRadius,
                               const glm::vec3&         cameraPos);

    /**
     * Collect all lights from ECS and add to lighting system
     */
    void collectLights(entt::registry& registry, LightingSystem* lightingSystem);

    /**
     * Collect all renderable entities for rendering
     */
    std::vector<RenderableEntity> collectRenderables(entt::registry& registry);

    /**
     * Render shadow maps for all shadow-casting lights
     */
    void renderShadowMaps(entt::registry&     registry,
                          LightingSystem*     lightingSystem,
                          Core::AssetManager* assetManager);

    /**
     * Render a single directional/spot shadow map
     */
    void renderDirectionalShadowMap(ShadowMap&                           shadowMap,
                                    const Light&                         light,
                                    const glm::mat4&                     lightSpaceMatrix,
                                    const std::vector<RenderableEntity>& renderables,
                                    Core::AssetManager*                  assetManager,
                                    Graphics::Shader&                    shadowShader);

    /**
     * Render a single point light cubemap shadow
     */
    void renderPointShadowMap(CubemapShadow&                       cubemap,
                              const Light&                         light,
                              const std::array<glm::mat4, 6>&      lightMatrices,
                              const std::vector<RenderableEntity>& renderables,
                              Core::AssetManager*                  assetManager,
                              Graphics::Shader&                    shadowShader);

    Graphics::GraphicsContext& context_;
    RenderStats                stats_;
    MaterialRenderer           materialRenderer_;
};

}
