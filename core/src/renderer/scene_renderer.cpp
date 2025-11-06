// scene_renderer.cpp
#include "corvus/renderer/scene_renderer.hpp"

#include "corvus/application.hpp"
#include "corvus/components/light.hpp"
#include "corvus/log.hpp"

namespace Corvus::Renderer {

SceneRenderer::SceneRenderer(Graphics::GraphicsContext& context)
    : context_(context), materialRenderer_(context) {
    // Initialize lighting system
    lighting_.initialize(context_);
}

void SceneRenderer::clear(const glm::vec4&             color,
                          bool                         clearDepth,
                          const Graphics::Framebuffer* targetFB) const {
    auto cmd = context_.createCommandBuffer();
    cmd.begin();

    if (targetFB && targetFB->valid()) {
        cmd.bindFramebuffer(*targetFB);
        cmd.setViewport(0, 0, targetFB->width, targetFB->height);
    }

    cmd.clear(color.r, color.g, color.b, color.a, clearDepth);

    if (targetFB && targetFB->valid()) {
        cmd.unbindFramebuffer();
    }

    cmd.end();
    cmd.submit();
}

void SceneRenderer::render(const std::vector<Renderable>& renderables,
                           const glm::mat4&               view,
                           const glm::mat4&               proj,
                           const glm::vec3&               cameraPos,
                           const Graphics::Framebuffer*   targetFB) {

    stats_.reset();

    // Render shadow maps if there are shadow-casting lights
    renderShadowMaps(renderables);

    auto cmd = context_.createCommandBuffer();
    cmd.begin();

    // Bind framebuffer
    if (targetFB && targetFB->valid()) {
        cmd.bindFramebuffer(*targetFB);
        cmd.setViewport(0, 0, targetFB->width, targetFB->height);
    } else {
        cmd.unbindFramebuffer();
    }

    for (auto& renderable : renderables) {
        if (!renderable.enabled)
            continue;

        if (!renderable.model || !renderable.model->valid())
            continue;

        if (!renderable.material)
            continue;

        // Apply material
        auto* shader = materialRenderer_.apply(*renderable.material, cmd);
        if (!shader || !shader->valid())
            continue;

        // Setup standard uniforms
        setupStandardUniforms(cmd, *shader, renderable.transform, view, proj);

        // Setup lighting uniforms
        setupLightingUniforms(
            cmd, *shader, renderable.position, renderable.boundingRadius, cameraPos);
        lighting_.bindShadowTextures(cmd);

        // Culling
        float det      = glm::determinant(renderable.transform);
        bool  mirrored = det < 0.0f;
        bool  cull     = renderable.material->getRenderState().cullFace;
        cmd.setCullFace(cull, mirrored);

        // Draw
        renderable.model->draw(cmd, renderable.wireframe);

        // Stats
        stats_.entitiesRendered++;
        for (const auto& mesh : renderable.model->getMeshes()) {
            if (mesh && mesh->valid()) {
                stats_.drawCalls++;
                stats_.triangles += mesh->getIndexCount() / 3;
                stats_.vertices += mesh->getIndexCount();
            }
        }
    }

    if (targetFB && targetFB->valid())
        cmd.unbindFramebuffer();

    cmd.end();
    cmd.submit();
}

void SceneRenderer::render(const std::vector<Renderable>& renderables,
                           const Camera&                  camera,
                           const Graphics::Framebuffer*   targetFB) {
    glm::mat4 view      = camera.getViewMatrix();
    glm::mat4 proj      = camera.getProjectionMatrix();
    glm::vec3 cameraPos = camera.getPosition();

    render(renderables, view, proj, cameraPos, targetFB);
}

// ECS Integration
void SceneRenderer::renderScene(entt::registry&              registry,
                                const Camera&                camera,
                                Core::AssetManager*          assetManager,
                                const Graphics::Framebuffer* targetFB) {

    // Convert ECS lights to renderer lights
    collectLightsFromRegistry(registry);

    // Convert ECS entities to renderables
    auto renderables = collectRenderables(registry, assetManager);

    // Use the primary render method
    render(renderables, camera, targetFB);
}

void SceneRenderer::collectLightsFromRegistry(entt::registry& registry) {
    // Clear lights from previous frame
    lighting_.clear();

    const auto lightView
        = registry.view<Core::Components::LightComponent, Core::Components::TransformComponent>();

    for (const auto entityHandle : lightView) {
        const auto& lightComp = lightView.get<Core::Components::LightComponent>(entityHandle);
        auto&       transform = lightView.get<Core::Components::TransformComponent>(entityHandle);

        // Check if entity is enabled
        if (const auto* entityInfo
            = registry.try_get<Core::Components::EntityInfoComponent>(entityHandle);
            entityInfo && !entityInfo->enabled)
            continue;

        if (!lightComp.enabled)
            continue;

        // Convert ECS light component to renderer light
        Light light;

        switch (lightComp.type) {
            case Core::Components::LightType::Directional:
                light.type = LightType::Directional;
                break;
            case Core::Components::LightType::Point:
                light.type = LightType::Point;
                break;
            case Core::Components::LightType::Spot:
                light.type = LightType::Spot;
                break;
        }

        light.position    = transform.position;
        light.direction   = glm::normalize(glm::rotate(transform.rotation, glm::vec3(0, 0, -1)));
        light.color       = glm::vec3(lightComp.color.r, lightComp.color.g, lightComp.color.b);
        light.intensity   = lightComp.intensity;
        light.range       = lightComp.range;
        light.innerCutoff = lightComp.innerCutoff;
        light.outerCutoff = lightComp.outerCutoff;
        light.castShadows = lightComp.castShadows;
        light.shadowMapResolution = lightComp.shadowMapResolution;
        light.shadowBias          = lightComp.shadowBias;
        light.shadowStrength      = lightComp.shadowStrength;
        light.shadowDistance      = lightComp.shadowDistance;
        light.shadowNearPlane     = lightComp.shadowNearPlane;
        light.shadowFarPlane      = lightComp.shadowFarPlane;

        lighting_.addLight(light);
    }
}

std::vector<Renderable> SceneRenderer::collectRenderables(entt::registry&     registry,
                                                          Core::AssetManager* assetManager) {

    std::vector<Renderable> renderables;

    const auto meshView = registry.view<Core::Components::MeshRendererComponent,
                                        Core::Components::TransformComponent>();

    for (const auto entityHandle : meshView) {
        auto& meshRenderer = meshView.get<Core::Components::MeshRendererComponent>(entityHandle);
        auto& transform    = meshView.get<Core::Components::TransformComponent>(entityHandle);

        // Check if entity is enabled
        if (const auto* entityInfo
            = registry.try_get<Core::Components::EntityInfoComponent>(entityHandle);
            entityInfo && !entityInfo->enabled)
            continue;

        // Get model
        auto* model = meshRenderer.getModel(assetManager, &context_);
        if (!model || !model->valid())
            continue;

        // Get MaterialAsset and convert to Material
        auto* materialAsset = meshRenderer.getMaterial(assetManager);
        if (!materialAsset)
            continue;

        // Convert MaterialAsset -> Material
        Material* material = materialRenderer_.getMaterialFromAsset(*materialAsset, assetManager);
        if (!material)
            continue;

        // Create pure renderer Renderable
        Renderable renderable;
        renderable.model          = model;
        renderable.material       = material;
        renderable.transform      = transform.getMatrix();
        renderable.position       = transform.position;
        renderable.boundingRadius = meshRenderer.getBoundingRadius();
        renderable.wireframe      = meshRenderer.renderWireframe;
        renderable.enabled        = true;

        renderables.push_back(renderable);
    }

    return renderables;
}

void SceneRenderer::renderShadowMaps(const std::vector<Renderable>& renderables) {
    // Prepare shadow maps
    lighting_.prepareShadowMaps(context_);

    // Get shadow shader
    auto& shadowShader = lighting_.getShadowShader();
    if (!shadowShader.valid()) {
        return;
    }

    if (renderables.empty())
        return;

    // Calculate scene center for directional lights
    glm::vec3 sceneCenter(0.0f);
    for (const auto& r : renderables) {
        sceneCenter += r.position;
    }
    if (!renderables.empty()) {
        sceneCenter /= static_cast<float>(renderables.size());
    }

    // Get all lights
    auto& lights = lighting_.getLights();

    size_t shadowMapIndex = 0;
    size_t cubemapIndex   = 0;

    std::vector<float> shadowBiases;
    std::vector<float> shadowStrengths;

    // Render shadow maps for each shadow-casting light
    for (auto& light : lights) {
        if (!light.castShadows)
            continue;

        if (light.type == LightType::Directional) {
            if (shadowMapIndex >= LightingSystem::MAX_SHADOW_MAPS)
                break;

            auto& shadowMaps = lighting_.getShadowMaps();
            if (shadowMapIndex >= shadowMaps.size())
                break;

            auto&     shadowMap = shadowMaps[shadowMapIndex];
            glm::mat4 lightSpaceMatrix
                = lighting_.calculateDirectionalLightMatrix(light, sceneCenter);
            shadowMap.lightSpaceMatrix = lightSpaceMatrix;

            shadowBiases.push_back(light.shadowBias);
            shadowStrengths.push_back(light.shadowStrength);

            renderDirectionalShadowMap(shadowMap, lightSpaceMatrix, renderables, shadowShader);
            shadowMapIndex++;

        } else if (light.type == LightType::Spot) {
            if (shadowMapIndex >= LightingSystem::MAX_SHADOW_MAPS)
                break;

            auto& shadowMaps = lighting_.getShadowMaps();
            if (shadowMapIndex >= shadowMaps.size())
                break;

            auto&     shadowMap        = shadowMaps[shadowMapIndex];
            glm::mat4 lightSpaceMatrix = lighting_.calculateSpotLightMatrix(light);
            shadowMap.lightSpaceMatrix = lightSpaceMatrix;

            shadowBiases.push_back(light.shadowBias);
            shadowStrengths.push_back(light.shadowStrength);

            renderDirectionalShadowMap(shadowMap, lightSpaceMatrix, renderables, shadowShader);

            light.shadowMapIndex = static_cast<int>(shadowMapIndex);

            shadowMapIndex++;

        } else if (light.type == LightType::Point) {
            if (cubemapIndex >= LightingSystem::MAX_POINT_SHADOWS)
                break;

            auto& cubemaps = lighting_.getCubemapShadows();
            if (cubemapIndex >= cubemaps.size())
                break;

            auto& cubemap         = cubemaps[cubemapIndex];
            cubemap.lightPosition = light.position;
            cubemap.farPlane      = light.range;

            auto lightMatrices
                = lighting_.calculatePointLightMatrices(light.position, 0.1f, light.range);
            renderPointShadowMap(cubemap, light, lightMatrices, renderables, shadowShader);
            cubemapIndex++;
        }
        lighting_.setShadowProperties(shadowBiases, shadowStrengths);
    }
}

void SceneRenderer::renderDirectionalShadowMap(const ShadowMap&               shadowMap,
                                               const glm::mat4&               lightSpaceMatrix,
                                               const std::vector<Renderable>& renderables,
                                               Shader&                        shadowShader) const {

    if (!shadowShader.valid())
        return;

    auto cmd = context_.createCommandBuffer();
    cmd.begin();

    cmd.bindFramebuffer(shadowMap.framebuffer);
    cmd.setViewport(0, 0, shadowMap.resolution, shadowMap.resolution);
    cmd.clear(1.0f, 1.0f, 1.0f, 1.0f, true, false);

    cmd.setShader(shadowShader);
    cmd.setDepthTest(true);
    cmd.setDepthMask(true);
    cmd.setCullFace(true, false);

    for (const auto& renderable : renderables) {
        if (!renderable.enabled || !renderable.model || !renderable.model->valid())
            continue;

        shadowShader.setMat4(cmd, "u_LightSpaceMatrix", lightSpaceMatrix);
        shadowShader.setMat4(cmd, "u_Model", renderable.transform);
        renderable.model->draw(cmd);
    }

    cmd.unbindFramebuffer();
    cmd.end();
    cmd.submit();
}

void SceneRenderer::renderPointShadowMap(CubemapShadow&                  cubemap,
                                         const Light&                    light,
                                         const std::array<glm::mat4, 6>& lightMatrices,
                                         const std::vector<Renderable>&  renderables,
                                         Shader&                         shadowShader) const {

    if (!shadowShader.valid())
        return;

    for (int face = 0; face < 6; ++face) {
        auto cmd = context_.createCommandBuffer();
        cmd.begin();

        cubemap.framebuffer.attachTextureCubeFace(cubemap.depthCubemap, face);
        cmd.bindFramebuffer(cubemap.framebuffer);
        cmd.setViewport(0, 0, cubemap.resolution, cubemap.resolution);
        cmd.clear(1.0f, 1.0f, 1.0f, 1.0f, true, false);

        cmd.setShader(shadowShader);
        cmd.setDepthTest(true);
        cmd.setDepthMask(true);
        cmd.setCullFace(true, false);

        for (const auto& renderable : renderables) {
            if (!renderable.enabled || !renderable.model || !renderable.model->valid())
                continue;

            shadowShader.setMat4(cmd, "u_LightSpaceMatrix", lightMatrices[face]);
            shadowShader.setMat4(cmd, "u_Model", renderable.transform);
            renderable.model->draw(cmd);
        }

        cmd.unbindFramebuffer();
        cmd.end();
        cmd.submit();
    }
}

void SceneRenderer::setupStandardUniforms(CommandBuffer&   cmd,
                                          Shader&          shader,
                                          const glm::mat4& model,
                                          const glm::mat4& view,
                                          const glm::mat4& proj) {

    const glm::mat4 viewProj = proj * view;
    const glm::mat4 normal   = glm::transpose(glm::inverse(model));

    shader.setMat4(cmd, "u_Model", model);
    shader.setMat4(cmd, "u_View", view);
    shader.setMat4(cmd, "u_Projection", proj);
    shader.setMat4(cmd, "u_ViewProjection", viewProj);
    shader.setMat4(cmd, "u_NormalMatrix", normal);
}

void SceneRenderer::setupLightingUniforms(CommandBuffer&   cmd,
                                          Shader&          shader,
                                          const glm::vec3& objectPos,
                                          float            objectRadius,
                                          const glm::vec3& cameraPos) {

    lighting_.applyLightingUniforms(cmd, shader, objectPos, objectRadius, cameraPos);
}

}
