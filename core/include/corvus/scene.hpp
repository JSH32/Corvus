#pragma once

#include "corvus/asset/asset_manager.hpp"
#include "corvus/components/component_registry.hpp"
#include "corvus/components/mesh_renderer.hpp"
#include "corvus/entity.hpp"
#include "corvus/systems/lighting_system.hpp"
#include "entt/entt.hpp"
#include "raylib-cpp.hpp"
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace Corvus::Core {
class Scene {
public:
    explicit Scene(const std::string_view& name, AssetManager* assetManager)
        : name(name), assetManager(assetManager) { }
    const std::vector<Entity>& getRootOrderedEntities() { return rootOrderedEntities; }

    Entity createEntity(const std::string& entityName = std::string());
    void   destroyEntity(Entity entity);

    void render(raylib::RenderTexture& target, const Vector3& viewPos);

    std::string    name;
    entt::registry registry;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(name));

        if constexpr (Archive::is_loading::value) {
            CORVUS_CORE_TRACE("Starting scene deserialization for scene: {}", name);

            // Clear out existing scene components and registry to prepare for next scene to load.
            registry.clear();
            rootOrderedEntities.clear();

            std::vector<std::map<std::string, std::string>> entityData;
            size_t                                          entityCount;
            ar.setNextName("entities");
            ar.startNode();
            ar(cereal::make_size_tag(entityCount));

            CORVUS_CORE_TRACE("Found {} entities to deserialize", entityCount);

            for (size_t i = 0; i < entityCount; ++i) {
                CORVUS_CORE_TRACE("Deserializing entity {}/{}", i + 1, entityCount);
                ar.startNode();

                // Create a fresh entity in the registry
                entt::entity handle = registry.create();
                CORVUS_CORE_TRACE("Created entity with handle: {}", static_cast<uint32_t>(handle));
                Entity entity { handle, this };

                // Let the entity deserialize its components
                entity.serialize(ar);

                ar.finishNode();
                rootOrderedEntities.push_back(entity);
                CORVUS_CORE_TRACE("Entity {} added to root entities", i + 1);
            }

            ar.finishNode();
            CORVUS_CORE_TRACE("Scene deserialization complete. Total entities: {}",
                              rootOrderedEntities.size());
        } else {
            CORVUS_CORE_TRACE("Starting scene serialization for scene: {}", name);
            CORVUS_CORE_TRACE("Serializing {} entities", rootOrderedEntities.size());
            ar(cereal::make_nvp("entities", rootOrderedEntities));
            CORVUS_CORE_TRACE("Scene serialization complete");
        }
    }

private:
    std::vector<Entity> rootOrderedEntities;
    AssetManager*       assetManager;

    Systems::LightingSystem lightingSystem;

    // Cache of renderable entities to be passed to systems to avoid redundancy.
    std::vector<Systems::RenderableEntity> cachedRenderables;

    void collectRenderables();
};
}
