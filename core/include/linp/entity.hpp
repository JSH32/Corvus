#pragma once

#include "components/component_registry.hpp"
#include "entt/core/fwd.hpp"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"

#include <cstdint>
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
    bool hasComponent() const {
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

    template <class Archive>
    void serialize(Archive& ar) const {
        auto& registry = Components::ComponentRegistry::get();

        if constexpr (Archive::is_saving::value) {
            // Serialize all components this entity has
            for (const auto& typeIdx : registry.getRegisteredTypeIndices()) {
                if (registry.hasComponent(typeIdx, entityHandle, getRegistry())) {
                    std::string typeName = registry.getTypeName(typeIdx);
                    registry.serializeComponent(typeIdx, entityHandle, getRegistry(), ar, typeName);
                }
            }
        } else {
            for (const auto& componentName : registry.getRegisteredTypes()) {
                try {
                    registry.deserializeComponent(componentName, entityHandle, getRegistry(), ar);
                } catch (...) {
                    /* Component doesn't exist on entity */
                }
            }
        }
    }

private:
    entt::entity entityHandle { entt::null };

    entt::registry& getRegistry() const;

    Scene* scene = nullptr;
};
}
