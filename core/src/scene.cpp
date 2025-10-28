#include "corvus/scene.hpp"

#include <entt/entt.hpp>

#include "RenderTexture.hpp"
#include "corvus/components/entity_info.hpp"
#include "corvus/components/mesh_renderer.hpp"
#include "corvus/components/transform.hpp"
#include "corvus/entity.hpp"
#include "corvus/log.hpp"
#include "corvus/scene.hpp"

namespace Corvus::Core {
void Scene::destroyEntity(const Entity entity) {
    rootOrderedEntities.erase(
        std::remove(rootOrderedEntities.begin(), rootOrderedEntities.end(), entity),
        rootOrderedEntities.end());
    registry.destroy(entity);
}

Entity Scene::createEntity(const std::string& entityName) {
    Entity entity = { registry.create(), this };

    entity.addComponent<Components::EntityInfoComponent>(entityName.empty() ? "New entity"
                                                                            : entityName);
    entity.addComponent<Components::TransformComponent>();

    rootOrderedEntities.push_back(entity);

    return entity;
}

void Scene::collectRenderables() {
    cachedRenderables.clear();

    auto meshView
        = registry.view<Components::TransformComponent, Components::MeshRendererComponent>();

    for (auto entityHandle : meshView) {
        Entity entity { entityHandle, this };

        Systems::RenderableEntity renderable;
        renderable.entity       = &entity;
        renderable.transform    = &meshView.get<Components::TransformComponent>(entityHandle);
        renderable.meshRenderer = &meshView.get<Components::MeshRendererComponent>(entityHandle);
        renderable.entityInfo   = entity.hasComponent<Components::EntityInfoComponent>()
              ? &entity.getComponent<Components::EntityInfoComponent>()
              : nullptr;
        renderable.isEnabled    = !renderable.entityInfo || renderable.entityInfo->enabled;

        if (renderable.isEnabled) {
            cachedRenderables.push_back(renderable);
        }
    }
}

void Scene::render(raylib::RenderTexture& target, const Vector3& viewPos) {
    for (auto it = rootOrderedEntities.rbegin(); it != rootOrderedEntities.rend(); ++it) {
        auto& entity = *it;

        if (!entity.hasComponent<Components::EntityInfoComponent>()) {
            CORVUS_ERROR(
                "An Entity did not have a EntityInfo component, this should not happen. It "
                "has been added automatically.");
            entity.addComponent<Components::EntityInfoComponent>();
        }
    }

    if (!lightingSystem.shadowManager.initialized) {
        lightingSystem.initialize();
    }

    collectRenderables();

    lightingSystem.clear();

    auto lightView = registry.view<Components::TransformComponent, Components::LightComponent>();
    for (auto entityHandle : lightView) {
        Entity entity { entityHandle, this };

        if (entity.hasComponent<Components::EntityInfoComponent>()) {
            auto& entityInfo = entity.getComponent<Components::EntityInfoComponent>();
            if (!entityInfo.enabled)
                continue;
        }

        auto& transform = lightView.get<Components::TransformComponent>(entityHandle);
        auto& light     = lightView.get<Components::LightComponent>(entityHandle);

        if (!light.enabled)
            continue;

        Systems::LightData lightData;
        lightData.position = transform.position;
        lightData.direction
            = Vector3Normalize(Vector3RotateByQuaternion({ 0, 0, -1 }, transform.rotation));
        lightData.color               = light.color;
        lightData.intensity           = light.intensity;
        lightData.type                = light.type;
        lightData.range               = light.range;
        lightData.attenuation         = light.attenuation;
        lightData.innerCutoff         = light.innerCutoff;
        lightData.outerCutoff         = light.outerCutoff;
        lightData.castShadows         = light.castShadows;
        lightData.shadowMapResolution = light.shadowMapResolution;
        lightData.shadowBias          = light.shadowBias;
        lightData.shadowStrength      = light.shadowStrength;
        lightData.shadowDistance      = light.shadowDistance;
        lightData.shadowNearPlane     = light.shadowNearPlane;
        lightData.shadowFarPlane      = light.shadowFarPlane;

        lightingSystem.addLight(lightData);
    }

    Vector3 sceneCenter = { 0, 0, 0 };
    if (!cachedRenderables.empty()) {
        // Calculate average position of all renderables
        for (const auto& renderable : cachedRenderables) {
            sceneCenter = Vector3Add(sceneCenter, renderable.transform->position);
        }
        sceneCenter = Vector3Scale(sceneCenter, 1.0f / cachedRenderables.size());
    }

    lightingSystem.renderShadowMaps(cachedRenderables, assetManager);

    for (auto& renderable : cachedRenderables) {
        Corvus::Core::Material* material = renderable.meshRenderer->getMaterial(assetManager);
        if (!material)
            continue;

        raylib::Material* rlMat = material->getRaylibMaterial(assetManager);
        if (!rlMat)
            continue;

        // Calculate object bounds center
        Vector3 objectCenter = renderable.transform->position;
        float   objectRadius = 5.0f; // Could calculate from mesh bounds

        // Apply per-object lighting with culling
        lightingSystem.applyToMaterial(rlMat, objectCenter, objectRadius, viewPos);

        if (auto model = renderable.meshRenderer->getModel(assetManager)) {
            for (int i = 0; i < model->meshCount; ++i) {
                DrawMesh(model->meshes[i], *rlMat, renderable.transform->getMatrix());
            }
        }
    }
}

}
