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

    const std::vector<Entity>& getRootOrderedEntities() {
        return rootOrderedEntities;
    }

    Entity createEntity(const std::string& entityName = std::string());
    void   destroyEntity(Entity entity);

    void render(raylib::RenderTexture& target);

    std::string    name;
    entt::registry registry;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(name));
        ar(cereal::make_nvp("entities", rootOrderedEntities));
    }

    // Save scene to file
    void saveToFile(const std::string& filename) {
        std::ofstream             file(filename);
        cereal::JSONOutputArchive ar(file);
        ar(cereal::make_nvp("scene", *this));
    }

    // Load scene from file
    void loadFromFile(const std::string& filename) {
        std::ifstream            file(filename);
        cereal::JSONInputArchive ar(file);
        ar(cereal::make_nvp("scene", *this));
    }

private:
    std::vector<Entity> rootOrderedEntities;
};
} // namespace Linp::Core
