#include "corvus/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "corvus/scene.hpp"

namespace Corvus::Core {
entt::registry& Entity::getRegistry() const { return scene->registry; }
}
