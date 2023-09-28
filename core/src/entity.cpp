#include "linp/entity.hpp"

namespace Linp::Core {
Entity::Entity(const entt::entity handle, Scene* scene)
    : entityHandle(handle), scene(scene) { }
}