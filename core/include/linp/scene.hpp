#pragma once

#include "entt/entt.hpp"
#include "linp/entity.hpp"
#include "raylib-cpp.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace Linp::Core {
class Scene {
public:
    explicit Scene(const std::string_view& name) : name(name) { }

    const std::vector<Entity>& getRootOrderedEntities() {
        return rootOrderedEntities;
    }

    Entity createEntity(const std::string& entityName = std::string());
    void destroyEntity(Entity entity);

    void render(raylib::RenderTexture& target);

    std::string name;
    entt::registry registry;

private:
    std::vector<Entity> rootOrderedEntities;
};
} // namespace Linp::Core