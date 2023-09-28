#pragma once

#include "entt/core/fwd.hpp"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include <utility>

namespace Linp::Core {
class Scene;

class Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene) : entityHandle(handle), scene(scene) { }

    template <typename T, typename... Args>
    T& addComponent(Args&&... args) {
        T& component = getRegistry().emplace<T>(entityHandle, std::forward<Args>(args)...);
        return component;
    }

    template <typename T>
    T& getComponent() {
        return getRegistry().get<T>(entityHandle);
    }

    template <typename T>
    [[nodiscard]] bool hasComponent() const {
        return getRegistry().all_of<T>(entityHandle);
    }

    template <typename T>
    void removeComponent() const {
        getRegistry().remove<T>(entityHandle);
    }

    bool operator==(const Entity& other) const {
        return entityHandle == other.entityHandle && scene == other.scene;
    }

    bool operator!=(const Entity& other) const {
        return !(*this == other);
    }

    operator bool() const { return entityHandle != entt::null; }
    operator uint32_t() const { return (uint32_t)entityHandle; }
    operator entt::entity() const { return entityHandle; }

private:
    entt::entity entityHandle { entt::null };

    entt::registry& getRegistry() const;

    Scene* scene = nullptr;
};
}