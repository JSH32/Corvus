#pragma once

// Forward declarations only
namespace Linp::Core::Components {
struct TransformComponent;
struct MeshRendererComponent;
struct EntityInfoComponent;
}

namespace Linp::Core {
class Entity;
}

namespace Linp::Core::Systems {

// Shared renderable entity structure
struct RenderableEntity {
    Linp::Core::Entity*                            entity;
    Linp::Core::Components::TransformComponent*    transform;
    Linp::Core::Components::MeshRendererComponent* meshRenderer;
    Linp::Core::Components::EntityInfoComponent*   entityInfo;
    bool                                           isEnabled;
};

}
