#pragma once

#include "cereal/cereal.hpp"
#include "component_registry.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/array.hpp>
#include <string>

namespace Linp::Core::Components {

class EntityInfoComponent : public Linp::Core::Components::SerializableComponent<EntityInfoComponent> {
public:
    explicit EntityInfoComponent(std::string tag = "New entity", const bool enabled = true)
        : SerializableComponent("EntityInfo"), tag(std::move(tag)), enabled(enabled) {
        boost::uuids::random_generator gen;
        id = boost::uuids::to_string(gen());
    }

    std::string id;
    std::string tag;
    bool        enabled {};

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(id), CEREAL_NVP(tag), CEREAL_NVP(enabled));
    }
};

}
