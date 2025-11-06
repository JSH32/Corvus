#include "corvus/components/component_registry.hpp"

#include <ranges>

namespace Corvus::Core::Components {
ComponentRegistry* ComponentRegistry::instance = nullptr;

ComponentRegistry& ComponentRegistry::get() {
    if (!instance) {
        instance = new ComponentRegistry();
    }
    return *instance;
}

std::string ComponentRegistry::getTypeName(std::type_index typeIdx) const {
    const auto it = typeToName.find(typeIdx);
    return it != typeToName.end() ? it->second : "";
}

std::type_index ComponentRegistry::getTypeIndex(const std::string& typeName) const {
    auto it = nameToType.find(typeName);
    return (it != nameToType.end()) ? it->second : std::type_index(typeid(void));
}

void ComponentRegistry::serializeComponent(std::type_index            typeIdx,
                                           entt::entity               entity,
                                           entt::registry&            registry,
                                           cereal::JSONOutputArchive& ar,
                                           const std::string&         componentName) {
    if (const auto it = serializers.find(typeIdx); it != serializers.end()) {
        it->second(entity, registry, ar, componentName);
    }
}

void ComponentRegistry::deserializeComponent(const std::string&        typeName,
                                             entt::entity              entity,
                                             entt::registry&           registry,
                                             cereal::JSONInputArchive& ar) {
    if (const auto it = deserializers.find(typeName); it != deserializers.end()) {
        it->second(entity, registry, ar);
    }
}

bool ComponentRegistry::hasComponent(std::type_index typeIdx,
                                     entt::entity    entity,
                                     entt::registry& registry) {
    if (const auto it = checkers.find(typeIdx); it != checkers.end()) {
        return it->second(entity, registry);
    }
    return false;
}

std::vector<std::string> ComponentRegistry::getRegisteredTypes() const {
    std::vector<std::string> types;
    for (const auto& key : nameToType | std::views::keys) {
        types.push_back(key);
    }
    return types;
}

std::vector<std::type_index> ComponentRegistry::getRegisteredTypeIndices() const {
    std::vector<std::type_index> indices;
    for (const auto& key : typeToName | std::views::keys) {
        indices.push_back(key);
    }
    return indices;
}

}
