#include "linp/components/component_registry.hpp"

namespace Linp::Core::Components {
ComponentRegistry* ComponentRegistry::instance = nullptr;

ComponentRegistry& ComponentRegistry::get() {
    if (!instance) {
        instance = new ComponentRegistry();
    }
    return *instance;
}

std::string ComponentRegistry::getTypeName(std::type_index typeIdx) const {
    auto it = typeToName.find(typeIdx);
    return (it != typeToName.end()) ? it->second : "";
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
    auto it = serializers.find(typeIdx);
    if (it != serializers.end()) {
        it->second(entity, registry, ar, componentName);
    }
}

void ComponentRegistry::deserializeComponent(const std::string&        typeName,
                                             entt::entity              entity,
                                             entt::registry&           registry,
                                             cereal::JSONInputArchive& ar) {
    auto it = deserializers.find(typeName);
    if (it != deserializers.end()) {
        it->second(entity, registry, ar);
    }
}

bool ComponentRegistry::hasComponent(std::type_index typeIdx,
                                     entt::entity    entity,
                                     entt::registry& registry) {
    auto it = checkers.find(typeIdx);
    if (it != checkers.end()) {
        return it->second(entity, registry);
    }
    return false;
}

std::vector<std::string> ComponentRegistry::getRegisteredTypes() const {
    std::vector<std::string> types;
    for (const auto& pair : nameToType) {
        types.push_back(pair.first);
    }
    return types;
}

std::vector<std::type_index> ComponentRegistry::getRegisteredTypeIndices() const {
    std::vector<std::type_index> indices;
    for (const auto& pair : typeToName) {
        indices.push_back(pair.first);
    }
    return indices;
}

} // namespace Linp::Core::Components
