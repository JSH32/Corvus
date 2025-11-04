#include "corvus/renderer/scene_renderer.hpp"
#include "corvus/log.hpp"
#include "corvus/renderer/material_renderer.hpp"

namespace Corvus::Renderer {

SceneRenderer::SceneRenderer(Graphics::GraphicsContext& context)
    : context_(context), materialRenderer_(context) { }

void SceneRenderer::clear(const glm::vec4& color, bool clearDepth) {
    auto cmd = context_.createCommandBuffer();
    cmd.begin();
    cmd.clear(color.r, color.g, color.b, color.a, clearDepth);
    cmd.end();
    cmd.submit();
}

void SceneRenderer::collectLights(entt::registry& registry, LightingSystem* lightingSystem) {
    if (!lightingSystem)
        return;

    lightingSystem->clear();

    auto lightView
        = registry.view<Core::Components::LightComponent, Core::Components::TransformComponent>();

    for (auto entityHandle : lightView) {
        auto& lightComp = lightView.get<Core::Components::LightComponent>(entityHandle);
        auto& transform = lightView.get<Core::Components::TransformComponent>(entityHandle);

        // Check if entity is enabled
        auto* entityInfo = registry.try_get<Core::Components::EntityInfoComponent>(entityHandle);
        if (entityInfo && !entityInfo->enabled) {
            continue;
        }

        if (!lightComp.enabled) {
            continue;
        }

        // Convert ECS light component to renderer light
        Light light;

        // Map the light type enum
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

        lightingSystem->addLight(light);
    }
}

std::vector<RenderableEntity> SceneRenderer::collectRenderables(entt::registry& registry) {

    std::vector<RenderableEntity> renderables;

    auto meshView = registry.view<Core::Components::MeshRendererComponent,
                                  Core::Components::TransformComponent>();

    for (auto entityHandle : meshView) {
        auto& meshRenderer = meshView.get<Core::Components::MeshRendererComponent>(entityHandle);
        auto& transform    = meshView.get<Core::Components::TransformComponent>(entityHandle);

        // Check if entity is enabled
        auto* entityInfo = registry.try_get<Core::Components::EntityInfoComponent>(entityHandle);
        if (entityInfo && !entityInfo->enabled) {
            continue;
        }

        RenderableEntity renderable;
        renderable.meshRenderer = &meshRenderer;
        renderable.transform    = &transform;
        renderable.isEnabled    = true;

        renderables.push_back(renderable);
    }

    return renderables;
}

void SceneRenderer::renderDirectionalShadowMap(ShadowMap&       shadowMap,
                                               const Light&     light,
                                               const glm::mat4& lightSpaceMatrix,
                                               const std::vector<RenderableEntity>& renderables,
                                               Core::AssetManager*                  assetManager,
                                               Graphics::Shader&                    shadowShader) {

    if (!shadowShader.valid())
        return;

    auto cmd = context_.createCommandBuffer();
    cmd.begin();

    // Bind shadow map framebuffer
    cmd.bindFramebuffer(shadowMap.framebuffer);
    cmd.setViewport(0, 0, shadowMap.resolution, shadowMap.resolution);
    cmd.clear(1.0f, 1.0f, 1.0f, 1.0f, true, false);

    // Set shadow render state
    cmd.setShader(shadowShader);
    cmd.setDepthTest(true);
    cmd.setDepthMask(true);
    cmd.setCullFace(true, false);

    // Render all objects from light's perspective
    for (const auto& renderable : renderables) {
        if (!renderable.meshRenderer || !renderable.transform)
            continue;

        auto* model = renderable.meshRenderer->getModel(assetManager, &context_);
        if (!model || !model->valid())
            continue;

        glm::mat4 modelMatrix = renderable.transform->getMatrix();

        // Set shadow uniforms
        shadowShader.setMat4(cmd, "u_LightSpaceMatrix", lightSpaceMatrix);
        shadowShader.setMat4(cmd, "u_Model", modelMatrix);

        // Draw model
        model->draw(cmd);
    }

    cmd.unbindFramebuffer();
    cmd.end();
    cmd.submit();
}

void SceneRenderer::renderPointShadowMap(CubemapShadow&                       cubemap,
                                         const Light&                         light,
                                         const std::array<glm::mat4, 6>&      lightMatrices,
                                         const std::vector<RenderableEntity>& renderables,
                                         Core::AssetManager*                  assetManager,
                                         Graphics::Shader&                    shadowShader) {

    if (!shadowShader.valid())
        return;

    // Render each face of the cubemap
    for (int face = 0; face < 6; ++face) {
        auto cmd = context_.createCommandBuffer();
        cmd.begin();

        // Attach this face of the cubemap to the framebuffer
        cubemap.framebuffer.attachTextureCubeFace(cubemap.depthCubemap, face);
        cmd.bindFramebuffer(cubemap.framebuffer);
        cmd.setViewport(0, 0, cubemap.resolution, cubemap.resolution);
        cmd.clear(1.0f, 1.0f, 1.0f, 1.0f, true, false);

        cmd.setShader(shadowShader);
        cmd.setDepthTest(true);
        cmd.setDepthMask(true);
        cmd.setCullFace(true, false);

        // Render all objects from this face's perspective
        for (const auto& renderable : renderables) {
            if (!renderable.meshRenderer || !renderable.transform)
                continue;

            auto* model = renderable.meshRenderer->getModel(assetManager, &context_);
            if (!model || !model->valid())
                continue;

            glm::mat4 modelMatrix = renderable.transform->getMatrix();

            shadowShader.setMat4(cmd, "u_LightSpaceMatrix", lightMatrices[face]);
            shadowShader.setMat4(cmd, "u_Model", modelMatrix);

            model->draw(cmd);
        }

        cmd.unbindFramebuffer();
        cmd.end();
        cmd.submit();
    }
}

void SceneRenderer::renderShadowMaps(entt::registry&     registry,
                                     LightingSystem*     lightingSystem,
                                     Core::AssetManager* assetManager) {

    if (!lightingSystem)
        return;

    // Prepare shadow maps (ensure they're initialized)
    lightingSystem->prepareShadowMaps(context_);

    // Get shadow shader from lighting system
    auto& shadowShader = lightingSystem->getShadowShader();
    if (!shadowShader.valid()) {
        CORVUS_CORE_WARN("Shadow shader not available, skipping shadow rendering");
        return;
    }

    // Collect renderables
    auto renderables = collectRenderables(registry);
    if (renderables.empty())
        return;

    // Calculate scene center for directional light shadows
    glm::vec3 sceneCenter(0.0f);
    for (const auto& r : renderables) {
        sceneCenter += r.transform->position;
    }
    if (!renderables.empty()) {
        sceneCenter /= static_cast<float>(renderables.size());
    }

    // Get all lights
    const auto& lights = lightingSystem->getLights();

    size_t shadowMapIndex = 0;
    size_t cubemapIndex   = 0;

    // Render shadow maps for each shadow-casting light
    for (const auto& light : lights) {
        if (!light.castShadows)
            continue;

        if (light.type == LightType::Directional) {
            if (shadowMapIndex >= LightingSystem::MAX_SHADOW_MAPS)
                break;

            auto& shadowMaps = lightingSystem->getShadowMaps();
            if (shadowMapIndex >= shadowMaps.size())
                break;

            auto& shadowMap = shadowMaps[shadowMapIndex];

            // Use lighting system helper to calculate matrix
            glm::mat4 lightSpaceMatrix
                = lightingSystem->calculateDirectionalLightMatrix(light, sceneCenter);
            shadowMap.lightSpaceMatrix = lightSpaceMatrix;

            renderDirectionalShadowMap(
                shadowMap, light, lightSpaceMatrix, renderables, assetManager, shadowShader);
            shadowMapIndex++;
        } else if (light.type == LightType::Spot) {
            if (shadowMapIndex >= LightingSystem::MAX_SHADOW_MAPS)
                break;

            auto& shadowMaps = lightingSystem->getShadowMaps();
            if (shadowMapIndex >= shadowMaps.size())
                break;

            auto& shadowMap = shadowMaps[shadowMapIndex];

            // Use lighting system helper to calculate matrix
            glm::mat4 lightSpaceMatrix = lightingSystem->calculateSpotLightMatrix(light);
            shadowMap.lightSpaceMatrix = lightSpaceMatrix;

            renderDirectionalShadowMap(
                shadowMap, light, lightSpaceMatrix, renderables, assetManager, shadowShader);
            shadowMapIndex++;
        } else if (light.type == LightType::Point) {
            if (cubemapIndex >= LightingSystem::MAX_POINT_SHADOWS)
                break;

            auto& cubemaps = lightingSystem->getCubemapShadows();
            if (cubemapIndex >= cubemaps.size())
                break;

            auto& cubemap         = cubemaps[cubemapIndex];
            cubemap.lightPosition = light.position;
            cubemap.farPlane      = light.range;

            // Use lighting system helper to calculate matrices
            auto lightMatrices
                = lightingSystem->calculatePointLightMatrices(light.position, 0.1f, light.range);

            renderPointShadowMap(
                cubemap, light, lightMatrices, renderables, assetManager, shadowShader);
            cubemapIndex++;
        }
    }
}

void SceneRenderer::setupStandardUniforms(Graphics::CommandBuffer& cmd,
                                          Graphics::Shader&        shader,
                                          const glm::mat4&         model,
                                          const glm::mat4&         view,
                                          const glm::mat4&         proj) {

    glm::mat4 viewProj = proj * view;
    glm::mat4 normal   = glm::transpose(glm::inverse(model));

    shader.setMat4(cmd, "u_Model", model);
    shader.setMat4(cmd, "u_View", view);
    shader.setMat4(cmd, "u_Projection", proj);
    shader.setMat4(cmd, "u_ViewProjection", viewProj);
    shader.setMat4(cmd, "u_NormalMatrix", normal);
}

void SceneRenderer::setupLightingUniforms(Graphics::CommandBuffer& cmd,
                                          Graphics::Shader&        shader,
                                          LightingSystem*          lightingSystem,
                                          const glm::vec3&         objectPos,
                                          float                    objectRadius,
                                          const glm::vec3&         cameraPos) {

    if (!lightingSystem)
        return;

    // Use the lighting system's method to set all lighting uniforms
    lightingSystem->applyLightingUniforms(cmd, shader, objectPos, objectRadius, cameraPos);
}

void SceneRenderer::render(const std::vector<RenderableEntity>& renderables,
                           const glm::mat4&                     view,
                           const glm::mat4&                     proj,
                           const glm::vec3&                     cameraPos,
                           Core::AssetManager*                  assetManager,
                           LightingSystem*                      lightingSystem,
                           const Graphics::Framebuffer*         targetFB) {

    stats_.reset();

    auto cmd = context_.createCommandBuffer();
    cmd.begin();

    // Bind framebuffer
    if (targetFB && targetFB->valid()) {
        cmd.bindFramebuffer(*targetFB);
        cmd.setViewport(0, 0, targetFB->width, targetFB->height);
    } else {
        cmd.unbindFramebuffer();
    }

    for (const auto& renderable : renderables) {
        if (!renderable.meshRenderer || !renderable.transform)
            continue;

        auto* model = renderable.meshRenderer->getModel(assetManager, &context_);
        if (!model || !model->valid())
            continue;

        auto* materialAsset = renderable.meshRenderer->getMaterial(assetManager);
        if (!materialAsset)
            continue;

        auto* shader = materialRenderer_.apply(*materialAsset, cmd, assetManager);
        if (!shader || !shader->valid())
            continue;

        glm::mat4 modelMat = renderable.transform->getMatrix();
        setupStandardUniforms(cmd, *shader, modelMat, view, proj);

        if (lightingSystem) {
            float radius = renderable.meshRenderer->getBoundingRadius();
            setupLightingUniforms(
                cmd, *shader, lightingSystem, renderable.transform->position, radius, cameraPos);
            lightingSystem->bindShadowTextures(cmd);
        }

        float det      = glm::determinant(modelMat);
        bool  mirrored = det < 0.0f;
        bool  cull     = !materialAsset->doubleSided;
        cmd.setCullFace(cull, mirrored);

        model->draw(cmd, renderable.meshRenderer->renderWireframe);

        stats_.entitiesRendered++;
        for (const auto& mesh : model->getMeshes()) {
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

void SceneRenderer::render(const std::vector<RenderableEntity>& renderables,
                           const Camera&                        camera,
                           Core::AssetManager*                  assetManager,
                           LightingSystem*                      lightingSystem,
                           const Graphics::Framebuffer*         targetFB) {
    glm::mat4 view      = camera.getViewMatrix();
    glm::mat4 proj      = camera.getProjectionMatrix();
    glm::vec3 cameraPos = camera.getPosition();

    render(renderables, view, proj, cameraPos, assetManager, lightingSystem, targetFB);
}

void SceneRenderer::render(entt::registry&              registry,
                           const Camera&                camera,
                           Core::AssetManager*          assetManager,
                           LightingSystem*              lightingSystem,
                           const Graphics::Framebuffer* targetFB) {

    if (lightingSystem) {
        if (!lightingSystem->isInitialized()) {
            lightingSystem->initialize(context_);
        }

        collectLights(registry, lightingSystem);
        renderShadowMaps(registry, lightingSystem, assetManager);
    }

    auto renderables = collectRenderables(registry);

    render(renderables, camera, assetManager, lightingSystem, targetFB);
}

} // namespace Corvus::Renderer
