#pragma once
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>
#include <entt/entt.hpp>
#include <functional>
#include <sstream>
#include <typeindex>

namespace Corvus::Core::Components {

using SerializerFunc = std::function<void(
    entt::entity, entt::registry&, cereal::JSONOutputArchive&, const std::string&)>;

using DeserializerFunc
    = std::function<void(entt::entity, entt::registry&, cereal::JSONInputArchive&)>;

using ComponentCheckerFunc = std::function<bool(entt::entity, entt::registry&)>;

using TypeToNameMap   = std::unordered_map<std::type_index, std::string>;
using NameToTypeMap   = std::unordered_map<std::string, std::type_index>;
using SerializerMap   = std::unordered_map<std::type_index, SerializerFunc>;
using DeserializerMap = std::unordered_map<std::string, DeserializerFunc>;
using CheckerMap      = std::unordered_map<std::type_index, ComponentCheckerFunc>;

/**
 * @class ComponentRegistry
 * @brief Central registry that manages component metadata, serialization, and
 * type mapping
 *
 * This singleton class maintains mappings between component types and their
 * names, and stores serialization functions for each registered component type.
 * Components are automatically registered statically.
 */
class ComponentRegistry {
private:
    static ComponentRegistry* instance;

    // Type name mappings
    TypeToNameMap typeToName; ///< Maps type_index to component name string
    NameToTypeMap nameToType; ///< Maps component name string to type_index

    // Serialization function mappings
    SerializerMap   serializers;   ///< Functions to serialize components to JSON
    DeserializerMap deserializers; ///< Functions to deserialize components from JSON
    CheckerMap      checkers;      ///< Functions to check if entity has component

public:
    /**
     * @brief Get the singleton instance of ComponentRegistry
     * @return Reference to the singleton ComponentRegistry
     */
    static ComponentRegistry& get();

    /**
     * @brief Register a component type with the registry
     * @tparam T The component type to register
     * @param typeName The string name to use for this component in
     * serialization
     *
     * This creates the serialization, deserialization, and checker functions
     * for the component type and stores them in the registry.
     */
    template <typename T>
    void registerComponent(const std::string& typeName) {
        std::type_index typeIdx = std::type_index(typeid(T));
        typeToName.insert({ typeIdx, typeName });
        nameToType.insert({ typeName, typeIdx });

        // Serializer writes component directly to JSON archive with proper name
        serializers[typeIdx] = [](entt::entity               entity,
                                  entt::registry&            registry,
                                  cereal::JSONOutputArchive& ar,
                                  const std::string&         name) {
            if (registry.all_of<T>(entity)) {
                auto& component = registry.get<T>(entity);
                ar(cereal::make_nvp(name, component));
            }
        };

        // Deserializer reads component from JSON archive and adds to entity
        deserializers[typeName] = [typeName](entt::entity              entity,
                                             entt::registry&           registry,
                                             cereal::JSONInputArchive& ar) {
            T component;
            ar(cereal::make_nvp(typeName, component));
            registry.emplace<T>(entity, std::move(component));
        };

        // Checker tests if entity has this component type
        checkers[typeIdx] = [](entt::entity entity, entt::registry& registry) -> bool {
            return registry.all_of<T>(entity);
        };
    }

    /**
     * @brief Get the string name for a component type
     * @param typeIdx The type_index of the component
     * @return The string name, or empty string if not found
     */
    std::string getTypeName(std::type_index typeIdx) const;

    /**
     * @brief Get the type_index for a component name
     * @param typeName The string name of the component
     * @return The type_index, or typeid(void) if not found
     */
    std::type_index getTypeIndex(const std::string& typeName) const;

    /**
     * @brief Serialize a component to JSON archive
     * @param typeIdx The type_index of the component
     * @param entity The entity that owns the component
     * @param registry The registry containing the component
     * @param ar The JSON output archive to write to
     * @param componentName The name to use in the JSON output
     */
    void serializeComponent(std::type_index            typeIdx,
                            entt::entity               entity,
                            entt::registry&            registry,
                            cereal::JSONOutputArchive& ar,
                            const std::string&         componentName);

    /**
     * @brief Deserialize a component from JSON archive
     * @param typeName The string name of the component type
     * @param entity The entity to add the component to
     * @param registry The registry to add the component to
     * @param ar The JSON input archive to read from
     */
    void deserializeComponent(const std::string&        typeName,
                              entt::entity              entity,
                              entt::registry&           registry,
                              cereal::JSONInputArchive& ar);

    /**
     * @brief Check if an entity has a specific component type
     * @param typeIdx The type_index of the component
     * @param entity The entity to check
     * @param registry The registry containing the entity
     * @return True if the entity has the component, false otherwise
     */
    bool hasComponent(std::type_index typeIdx, entt::entity entity, entt::registry& registry);

    /**
     * @brief Get all registered component type names
     * @return Vector of component name strings
     */
    std::vector<std::string> getRegisteredTypes() const;

    /**
     * @brief Get all registered component type indices
     * @return Vector of type_index objects
     */
    std::vector<std::type_index> getRegisteredTypeIndices() const;
};

/**
 * @brief Macro to register any component type with the ComponentRegistry
 * @param ComponentType The class name of the component
 * @param ComponentName The string name for serialization
 *
 * This macro can be used with any component class
 * It automatically registers the component at program startup.
 *
 * Usage:
 * @code
 * class MyComponent {
 * public:
 *     int value = 0;
 *
 *     template<class Archive>
 *     void serialize(Archive& ar) {
 *         ar(CEREAL_NVP(value));
 *     }
 * };
 * REGISTER_COMPONENT(MyComponent, "MyComponent")
 * @endcode
 */
#define REGISTER_COMPONENT(ComponentType, ComponentName)                                           \
    namespace {                                                                                    \
        static bool _reg_##ComponentType = []() {                                                  \
            ComponentRegistry::get().registerComponent<ComponentType>(ComponentName);              \
            return true;                                                                           \
        }();                                                                                       \
    }

}
