#include "linp/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "linp/scene.hpp"

namespace Linp::Core {
entt::registry& Entity::getRegistry() const { return scene->registry; }
}
