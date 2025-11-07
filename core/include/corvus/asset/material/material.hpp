#pragma once
#include "cereal/types/vector.hpp"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/material.hpp"

#include <cereal/cereal.hpp>
#include <glm/glm.hpp>
#include <map>
#include <ranges>
#include <string>

namespace Corvus::Renderer {
class MaterialRenderer;
}

namespace Corvus::Core {

// Property types
enum class MaterialPropertyType {
    Float,
    Vector2,
    Vector3,
    Vector4,
    Texture,
    Int,
    Bool
};

struct MaterialPropertyValue {
    MaterialPropertyType type { MaterialPropertyType::Float };

    union {
        float     floatValue;
        glm::vec2 vec2Value;
        glm::vec3 vec3Value;
        glm::vec4 vec4Value;
        UUID      textureValue;
        int       intValue;
        bool      boolValue;
    };
    glm::vec4 colorValue { 1.0f };
    int       textureSlot { 0 };

    MaterialPropertyValue() : floatValue(0.0f) { }
    explicit MaterialPropertyValue(float v) : floatValue(v) { }
    explicit MaterialPropertyValue(glm::vec2 v)
        : type(MaterialPropertyType::Vector2), vec2Value(v) { }
    explicit MaterialPropertyValue(glm::vec3 v)
        : type(MaterialPropertyType::Vector3), vec3Value(v) { }
    explicit MaterialPropertyValue(glm::vec4 v)
        : type(MaterialPropertyType::Vector4), vec4Value(v) { }
    explicit MaterialPropertyValue(UUID tex, int slot = 0)
        : type(MaterialPropertyType::Texture), textureValue(tex), textureSlot(slot) { }
    explicit MaterialPropertyValue(int v) : type(MaterialPropertyType::Int), intValue(v) { }
    explicit MaterialPropertyValue(bool v) : type(MaterialPropertyType::Bool), boolValue(v) { }

    float     getFloat() const { return (type == MaterialPropertyType::Float) ? floatValue : 0.0f; }
    glm::vec2 getVector2() const {
        return (type == MaterialPropertyType::Vector2) ? vec2Value : glm::vec2(0.0f);
    }
    glm::vec3 getVector3() const {
        return (type == MaterialPropertyType::Vector3) ? vec3Value : glm::vec3(0.0f);
    }
    glm::vec4 getVector4() const {
        return (type == MaterialPropertyType::Vector4) ? vec4Value : glm::vec4(0.0f);
    }
    UUID getTexture() const {
        return (type == MaterialPropertyType::Texture) ? textureValue : UUID();
    }
    int getTextureSlot() const { return (type == MaterialPropertyType::Texture) ? textureSlot : 0; }
    int getInt() const { return (type == MaterialPropertyType::Int) ? intValue : 0; }
    bool getBool() const { return (type == MaterialPropertyType::Bool) ? boolValue : false; }
};

struct MaterialProperty {
    std::string           name;
    MaterialPropertyValue value;

    MaterialProperty() = default;
    MaterialProperty(std::string n, MaterialPropertyValue v)
        : name(std::move(n)), value(std::move(v)) { }

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(name));
        int typeInt = static_cast<int>(value.type);
        ar(cereal::make_nvp("type", typeInt));
        if constexpr (Archive::is_loading::value)
            value.type = static_cast<MaterialPropertyType>(typeInt);

        switch (value.type) {
            case MaterialPropertyType::Float:
                ar(cereal::make_nvp("value", value.floatValue));
                break;
            case MaterialPropertyType::Int:
                ar(cereal::make_nvp("value", value.intValue));
                break;
            case MaterialPropertyType::Bool:
                ar(cereal::make_nvp("value", value.boolValue));
                break;
            case MaterialPropertyType::Vector2:
                ar(cereal::make_nvp("x", value.vec2Value.x),
                   cereal::make_nvp("y", value.vec2Value.y));
                break;
            case MaterialPropertyType::Vector3:
                ar(cereal::make_nvp("x", value.vec3Value.x),
                   cereal::make_nvp("y", value.vec3Value.y),
                   cereal::make_nvp("z", value.vec3Value.z));
                break;
            case MaterialPropertyType::Vector4:
                ar(cereal::make_nvp("x", value.vec4Value.x),
                   cereal::make_nvp("y", value.vec4Value.y),
                   cereal::make_nvp("z", value.vec4Value.z),
                   cereal::make_nvp("w", value.vec4Value.w));
                break;
            case MaterialPropertyType::Texture: {
                std::string uuidStr;
                if constexpr (Archive::is_saving::value)
                    uuidStr = boost::uuids::to_string(value.textureValue);
                ar(cereal::make_nvp("textureID", uuidStr),
                   cereal::make_nvp("textureSlot", value.textureSlot));
                if constexpr (Archive::is_loading::value)
                    value.textureValue
                        = uuidStr.empty() ? UUID() : boost::uuids::string_generator()(uuidStr);
                break;
            }
        }
    }
};

/**
 * Pure data structure for material properties.
 * No rendering logic - just properties and serialization.
 * This is what gets saved to disk and managed by the asset system.
 */
class MaterialAsset {
public:
    MaterialAsset() { setDefaultProperties(); }

    MaterialAsset& operator=(const MaterialAsset&) = delete;
    ~MaterialAsset()                               = default;
    MaterialAsset(const MaterialAsset& other)      = delete;

    MaterialAsset(MaterialAsset&&) noexcept = default;
    MaterialAsset& operator=(MaterialAsset&& other) noexcept {
        if (this != &other) {
            shaderAsset  = std::move(other.shaderAsset);
            properties   = std::move(other.properties);
            doubleSided  = other.doubleSided;
            alphaBlend   = other.alphaBlend;
            needsRebuild = true;
        }
        return *this;
    }

    Renderer::Material* getRuntimeMaterial(Renderer::MaterialRenderer& renderer,
                                           AssetManager&               assets) const;

    void markDirty() const { needsRebuild = true; }

    // Serialization
    template <class Archive>
    void serialize(Archive& ar) {
        std::string shaderUuidStr;
        if constexpr (Archive::is_saving::value)
            if (!shaderAsset.is_nil())
                shaderUuidStr = boost::uuids::to_string(shaderAsset);
        ar(cereal::make_nvp("shader", shaderUuidStr));
        if constexpr (Archive::is_loading::value)
            shaderAsset
                = shaderUuidStr.empty() ? UUID() : boost::uuids::string_generator()(shaderUuidStr);

        std::vector<MaterialProperty> props;
        if constexpr (Archive::is_saving::value) {
            for (auto& p : properties | std::views::values)
                props.push_back(p);
            ar(cereal::make_nvp("properties", props));
        } else {
            ar(cereal::make_nvp("properties", props));
            properties.clear();
            for (auto& p : props)
                properties[p.name] = std::move(p);
        }

        ar(CEREAL_NVP(doubleSided));
        ar(CEREAL_NVP(alphaBlend));
    }

    bool                    hasProperty(std::string_view name) const;
    const MaterialProperty* getProperty(std::string_view name) const;
    MaterialProperty*       getProperty(std::string_view name);
    void                    setProperty(const MaterialProperty& prop);
    void                    setProperty(std::string_view name, const MaterialPropertyValue& value);
    const UUID&             getShaderAsset() const { return shaderAsset; }
    size_t                  getPropertyCount() const { return properties.size(); }
    bool                    removeProperty(std::string_view name);

    template <typename Fn>
    void forEachProperty(Fn&& fn) const {
        for (const auto& [name, prop] : properties)
            fn(name, prop);
    }

    template <typename Fn>
    void forEachProperty(Fn&& fn) {
        for (auto& [name, prop] : properties)
            fn(name, prop);
    }

    template <typename T>
    void setValue(const std::string_view name, const T& v) {
        setProperty(std::string(name), MaterialPropertyValue(v));
    }

private:
    UUID                                    shaderAsset;
    std::map<std::string, MaterialProperty> properties;
    bool                                    doubleSided { false };
    bool                                    alphaBlend { false };

    // Default property setup
    void setDefaultProperties() {
        properties["_MainColor"]
            = MaterialProperty("_MainColor", MaterialPropertyValue(glm::vec4(1.0f)));
        properties["_MainTex"]    = MaterialProperty("_MainTex", MaterialPropertyValue(UUID(), 0));
        properties["_Metallic"]   = MaterialProperty("_Metallic", MaterialPropertyValue(0.0f));
        properties["_Smoothness"] = MaterialProperty("_Smoothness", MaterialPropertyValue(0.5f));
    }

    mutable std::unique_ptr<Renderer::Material> runtimeMaterial;
    mutable bool                                needsRebuild = true;
};

}
