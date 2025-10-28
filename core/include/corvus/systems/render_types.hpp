#pragma once

// Forward declarations only
namespace Corvus::Core::Components {
struct TransformComponent;
struct MeshRendererComponent;
struct EntityInfoComponent;
}

namespace Corvus::Core {
class Entity;
}

namespace Corvus::Core::Systems {

// Shared renderable entity structure
struct RenderableEntity {
    Corvus::Core::Entity*                            entity;
    Corvus::Core::Components::TransformComponent*    transform;
    Corvus::Core::Components::MeshRendererComponent* meshRenderer;
    Corvus::Core::Components::EntityInfoComponent*   entityInfo;
    bool                                           isEnabled;
};

}
