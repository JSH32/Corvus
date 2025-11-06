#include <ranges>

#include "corvus/components/entity_info.hpp"
#include "corvus/components/mesh_renderer.hpp"
#include "corvus/components/transform.hpp"
#include "corvus/entity.hpp"
#include "corvus/log.hpp"
#include "corvus/scene.hpp"

namespace Corvus::Core {

void Scene::destroyEntity(const Entity entity) {
    rootOrderedEntities.erase(std::ranges::remove(rootOrderedEntities, entity).begin(),
                              rootOrderedEntities.end());
    registry.destroy(entity.entityHandle);
}

Scene& Scene::operator=(Scene&& other) noexcept {
    if (this != &other) {
        name                = std::move(other.name);
        registry            = std::move(other.registry);
        rootOrderedEntities = std::move(other.rootOrderedEntities);
        assetManager        = other.assetManager;
        renderer            = other.renderer;

        // Rebind all entity.scene pointers to this new Scene instance
        for (auto& e : rootOrderedEntities) {
            e.scene = this;
        }

        other.assetManager = nullptr;
        other.renderer     = nullptr;
    }
    return *this;
}

Scene::Scene(Scene&& other) noexcept
    : name(std::move(other.name)), registry(std::move(other.registry)),
      rootOrderedEntities(std::move(other.rootOrderedEntities)), assetManager(other.assetManager),
      renderer(other.renderer) {

    // Rebind all entity.scene pointers to this new Scene instance
    for (auto& e : rootOrderedEntities) {
        e.scene = this;
    }

    other.assetManager = nullptr;
    other.renderer     = nullptr;
}

Entity Scene::createEntity(const std::string& entityName) {
    Entity entity = { registry.create(), this };

    entity.addComponent<Components::EntityInfoComponent>(entityName.empty() ? "New entity"
                                                                            : entityName);
    entity.addComponent<Components::TransformComponent>();

    rootOrderedEntities.push_back(entity);

    return entity;
}

void Scene::render(Graphics::GraphicsContext&   ctx,
                   const Renderer::Camera&      camera,
                   const Graphics::Framebuffer* targetFB) {

    // Ensure all entities have EntityInfo component
    for (auto& entity : std::ranges::reverse_view(rootOrderedEntities)) {
        if (!entity.hasComponent<Components::EntityInfoComponent>()) {
            CORVUS_ERROR("An Entity did not have a EntityInfo component, this should not happen. "
                         "It has been added automatically.");
            entity.addComponent<Components::EntityInfoComponent>();
        }
    }

    // Lazy initialize renderer
    if (!renderer) {
        renderer = new Renderer::SceneRenderer(ctx);
    }

    renderer->renderScene(registry, camera, assetManager, targetFB);
}

}
