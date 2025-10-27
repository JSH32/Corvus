#include "linp/scene.hpp"

#include <entt/entt.hpp>

#include "RenderTexture.hpp"
#include "linp/components/entity_info.hpp"
#include "linp/components/mesh_renderer.hpp"
#include "linp/components/transform.hpp"
#include "linp/entity.hpp"
#include "linp/log.hpp"
#include "linp/scene.hpp"

namespace Linp::Core {
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

void Scene::render(raylib::RenderTexture& target) {
    for (auto it = rootOrderedEntities.rbegin(); it != rootOrderedEntities.rend(); ++it) {
        auto& entity = *it;

        if (!entity.hasComponent<Components::EntityInfoComponent>()) {
            LINP_ERROR("An Entity did not have a EntityInfo component, this should not happen. It "
                       "has been added automatically.");
            entity.addComponent<Components::EntityInfoComponent>();
        }
    }

    auto meshView
        = registry.view<Components::TransformComponent, Components::MeshRendererComponent>();
    for (auto entityHandle : meshView) {
        Entity entity { entityHandle, this };

        // Check if entity is enabled
        if (entity.hasComponent<Components::EntityInfoComponent>()) {
            auto& entityInfo = entity.getComponent<Components::EntityInfoComponent>();
            if (!entityInfo.enabled)
                continue;
        }

        auto& transform    = meshView.get<Components::TransformComponent>(entityHandle);
        auto& meshRenderer = meshView.get<Components::MeshRendererComponent>(entityHandle);

        Linp::Core::Material* material = meshRenderer.getMaterial(assetManager);
        if (!material)
            continue;

        raylib::Material* rlMat = material->getRaylibMaterial(assetManager);
        if (!rlMat)
            continue;

        if (auto model = meshRenderer.getModel(assetManager)) {
            for (int i = 0; i < model->meshCount; ++i) {
                DrawMesh(model->meshes[i], *rlMat, transform.getMatrix());
            }
        }
    }
}

}
