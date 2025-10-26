#pragma once

#include "entt/entt.hpp"
#include "linp/components/component_registry.hpp"
#include "linp/entity.hpp"
#include "raylib-cpp.hpp"
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace Linp::Core {
class Scene {
public:
    explicit Scene(const std::string_view& name) : name(name) { }

    const std::vector<Entity>& getRootOrderedEntities() { return rootOrderedEntities; }

    Entity createEntity(const std::string& entityName = std::string());
    void   destroyEntity(Entity entity);

    void render(raylib::RenderTexture& target);

    std::string    name;
    entt::registry registry;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(name));
        
        if constexpr (Archive::is_loading::value) {
            LINP_CORE_TRACE("Starting scene deserialization for scene: {}", name);

            // Clear out existing scene components and registry to prepare for next scene to load.
            registry.clear();
            rootOrderedEntities.clear();

            std::vector<std::map<std::string, std::string>> entityData;
            size_t entityCount;
            ar.setNextName("entities");
            ar.startNode();
            ar(cereal::make_size_tag(entityCount));
            
            LINP_CORE_TRACE("Found {} entities to deserialize", entityCount);

            for (size_t i = 0; i < entityCount; ++i) {
                LINP_CORE_TRACE("Deserializing entity {}/{}", i + 1, entityCount);
                ar.startNode();
                
                // Create a fresh entity in the registry
                entt::entity handle = registry.create();
                LINP_CORE_TRACE("Created entity with handle: {}", static_cast<uint32_t>(handle));
                Entity entity{handle, this};
                
                // Let the entity deserialize its components
                entity.serialize(ar);
                
                ar.finishNode();
                rootOrderedEntities.push_back(entity);
                LINP_CORE_TRACE("Entity {} added to root entities", i + 1);
            }
            
            ar.finishNode();
            LINP_CORE_TRACE("Scene deserialization complete. Total entities: {}", rootOrderedEntities.size());
        } else {
            LINP_CORE_TRACE("Starting scene serialization for scene: {}", name);
            LINP_CORE_TRACE("Serializing {} entities", rootOrderedEntities.size());
            ar(cereal::make_nvp("entities", rootOrderedEntities));
            LINP_CORE_TRACE("Scene serialization complete");
        }
    }

private:
    std::vector<Entity> rootOrderedEntities;
};
} // namespace Linp::Core
