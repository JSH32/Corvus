#pragma once
#include "cereal/cereal.hpp"
#include "linp/asset/asset_handle.hpp"
#include "linp/asset/asset_manager.hpp"
#include "linp/files/static_resource_file.hpp"
#include "raylib-cpp.hpp"
#include "raylib.h"
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <map>
#include <string>

namespace Linp::Core {

// Material property types (like Unity's shader properties)
enum class MaterialPropertyType {
    Float,
    Color,
    Vector2,
    Vector3,
    Vector4,
    Texture,
    Int,
    Bool
};

// Simple discriminated union for material properties
struct MaterialPropertyValue {
    MaterialPropertyType type;

    union {
        float   floatValue;
        Color   colorValue;
        Vector2 vec2Value;
        Vector3 vec3Value;
        Vector4 vec4Value;
        UUID    textureValue;
        int     intValue;
        bool    boolValue;
    };

    // Texture-specific data (outside union since it's only used for textures)
    int textureSlot = 0; // Which texture unit to bind to (0-15)

    // Constructors
    MaterialPropertyValue() : type(MaterialPropertyType::Float), floatValue(0.0f) { }

    explicit MaterialPropertyValue(float v) : type(MaterialPropertyType::Float), floatValue(v) { }
    explicit MaterialPropertyValue(Color v) : type(MaterialPropertyType::Color), colorValue(v) { }
    explicit MaterialPropertyValue(Vector2 v)
        : type(MaterialPropertyType::Vector2), vec2Value(v) { }
    explicit MaterialPropertyValue(Vector3 v)
        : type(MaterialPropertyType::Vector3), vec3Value(v) { }
    explicit MaterialPropertyValue(Vector4 v)
        : type(MaterialPropertyType::Vector4), vec4Value(v) { }
    explicit MaterialPropertyValue(UUID v, int slot = 0)
        : type(MaterialPropertyType::Texture), textureValue(v), textureSlot(slot) { }
    explicit MaterialPropertyValue(int v) : type(MaterialPropertyType::Int), intValue(v) { }
    explicit MaterialPropertyValue(bool v) : type(MaterialPropertyType::Bool), boolValue(v) { }

    // Getters with type checking
    float getFloat() const { return (type == MaterialPropertyType::Float) ? floatValue : 0.0f; }

    Color getColor() const { return (type == MaterialPropertyType::Color) ? colorValue : WHITE; }

    Vector2 getVector2() const {
        return (type == MaterialPropertyType::Vector2) ? vec2Value : Vector2 { 0, 0 };
    }

    Vector3 getVector3() const {
        return (type == MaterialPropertyType::Vector3) ? vec3Value : Vector3 { 0, 0, 0 };
    }

    Vector4 getVector4() const {
        return (type == MaterialPropertyType::Vector4) ? vec4Value : Vector4 { 0, 0, 0, 0 };
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

        if constexpr (Archive::is_loading::value) {
            value.type = static_cast<MaterialPropertyType>(typeInt);
        }

        // Serialize based on type
        switch (value.type) {
            case MaterialPropertyType::Float: {
                if constexpr (Archive::is_saving::value) {
                    float val = value.floatValue;
                    ar(cereal::make_nvp("value", val));
                } else {
                    ar(cereal::make_nvp("value", value.floatValue));
                }
                break;
            }
            case MaterialPropertyType::Color: {
                if constexpr (Archive::is_saving::value) {
                    Color col = value.colorValue;
                    ar(cereal::make_nvp("r", col.r));
                    ar(cereal::make_nvp("g", col.g));
                    ar(cereal::make_nvp("b", col.b));
                    ar(cereal::make_nvp("a", col.a));
                } else {
                    ar(cereal::make_nvp("r", value.colorValue.r));
                    ar(cereal::make_nvp("g", value.colorValue.g));
                    ar(cereal::make_nvp("b", value.colorValue.b));
                    ar(cereal::make_nvp("a", value.colorValue.a));
                }
                break;
            }
            case MaterialPropertyType::Vector2: {
                if constexpr (Archive::is_saving::value) {
                    Vector2 vec = value.vec2Value;
                    ar(cereal::make_nvp("x", vec.x));
                    ar(cereal::make_nvp("y", vec.y));
                } else {
                    ar(cereal::make_nvp("x", value.vec2Value.x));
                    ar(cereal::make_nvp("y", value.vec2Value.y));
                }
                break;
            }
            case MaterialPropertyType::Vector3: {
                if constexpr (Archive::is_saving::value) {
                    Vector3 vec = value.vec3Value;
                    ar(cereal::make_nvp("x", vec.x));
                    ar(cereal::make_nvp("y", vec.y));
                    ar(cereal::make_nvp("z", vec.z));
                } else {
                    ar(cereal::make_nvp("x", value.vec3Value.x));
                    ar(cereal::make_nvp("y", value.vec3Value.y));
                    ar(cereal::make_nvp("z", value.vec3Value.z));
                }
                break;
            }
            case MaterialPropertyType::Vector4: {
                if constexpr (Archive::is_saving::value) {
                    Vector4 vec = value.vec4Value;
                    ar(cereal::make_nvp("x", vec.x));
                    ar(cereal::make_nvp("y", vec.y));
                    ar(cereal::make_nvp("z", vec.z));
                    ar(cereal::make_nvp("w", vec.w));
                } else {
                    ar(cereal::make_nvp("x", value.vec4Value.x));
                    ar(cereal::make_nvp("y", value.vec4Value.y));
                    ar(cereal::make_nvp("z", value.vec4Value.z));
                    ar(cereal::make_nvp("w", value.vec4Value.w));
                }
                break;
            }
            case MaterialPropertyType::Texture: {
                std::string uuidStr;
                if constexpr (Archive::is_saving::value) {
                    UUID uuid = value.textureValue;
                    if (!uuid.is_nil()) {
                        uuidStr = boost::uuids::to_string(uuid);
                    }
                }
                ar(cereal::make_nvp("textureID", uuidStr));
                ar(cereal::make_nvp("textureSlot", value.textureSlot));
                if constexpr (Archive::is_loading::value) {
                    if (!uuidStr.empty()) {
                        value.textureValue = boost::uuids::string_generator()(uuidStr);
                    } else {
                        value.textureValue = UUID();
                    }
                }
                break;
            }
            case MaterialPropertyType::Int: {
                if constexpr (Archive::is_saving::value) {
                    int val = value.intValue;
                    ar(cereal::make_nvp("value", val));
                } else {
                    ar(cereal::make_nvp("value", value.intValue));
                }
                break;
            }
            case MaterialPropertyType::Bool: {
                if constexpr (Archive::is_saving::value) {
                    bool val = value.boolValue;
                    ar(cereal::make_nvp("value", val));
                } else {
                    ar(cereal::make_nvp("value", value.boolValue));
                }
                break;
            }
        }
    }
};

struct Material {
    // Shader reference, if nil, loads default shader
    UUID shaderAsset;

    // Dynamic properties that the shader can use
    std::map<std::string, MaterialProperty> properties;

    // Render settings
    bool doubleSided = false;
    bool alphaBlend  = false;

    // Runtime cache
    AssetHandle<raylib::Shader>                         cachedShaderHandle;
    std::map<std::string, AssetHandle<raylib::Texture>> cachedTextureHandles;
    Shader                                              defaultShader       = { 0 };
    bool                                                defaultShaderLoaded = false;

    bool             rlMaterialDirty = true;
    raylib::Material rlMaterialCache;

    Material() {
        // Set up default properties for standard material
        setDefaultProperties();
    }

    Material(const Material&)                = delete;
    Material& operator=(const Material&)     = delete;
    Material(Material&&) noexcept            = default;
    Material& operator=(Material&&) noexcept = default;
    ~Material()                              = default;

    void setDefaultProperties() {
        // Standard PBR-like properties
        properties["_MainColor"]
            = MaterialProperty("_MainColor", MaterialPropertyValue(Color { 255, 255, 255, 255 }));
        properties["_MainTex"] = MaterialProperty(
            "_MainTex", MaterialPropertyValue(UUID(), 0)); // Slot 0 (MATERIAL_MAP_ALBEDO)
        properties["_Metallic"]   = MaterialProperty("_Metallic", MaterialPropertyValue(0.0f));
        properties["_Smoothness"] = MaterialProperty("_Smoothness", MaterialPropertyValue(0.5f));
    }

    void loadDefaultShader() {
        if (defaultShaderLoaded)
            return;

        auto vert = StaticResourceFile::create("engine/shaders/default_lit.vert")->readAllBytes();
        auto frag = StaticResourceFile::create("engine/shaders/default_lit.frag")->readAllBytes();

        LINP_CORE_INFO("Loading default shader for material '{}'...");
        LINP_CORE_INFO("Vertex shader size: {} bytes", vert.size());
        LINP_CORE_INFO("Fragment shader size: {} bytes", frag.size());

        defaultShader = LoadShaderFromMemory(vert.data(), frag.data());

        if (defaultShader.id > 0) {
            defaultShaderLoaded = true;
            LINP_CORE_INFO("Default shader loaded successfully with ID: {}", defaultShader.id);
            // Get shader locations for common uniforms
            defaultShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(defaultShader, "mvp");
            defaultShader.locs[SHADER_LOC_MATRIX_MODEL]
                = GetShaderLocation(defaultShader, "matModel");
            defaultShader.locs[SHADER_LOC_MATRIX_NORMAL]
                = GetShaderLocation(defaultShader, "matNormal");
            defaultShader.locs[SHADER_LOC_COLOR_DIFFUSE]
                = GetShaderLocation(defaultShader, "colDiffuse");
        } else {
            LINP_CORE_ERROR("Failed to load default shader! Shader ID: {}", defaultShader.id);
        }
    }

    void unloadDefaultShader() {
        if (defaultShaderLoaded && defaultShader.id > 0) {
            UnloadShader(defaultShader);
            defaultShader.id    = 0;
            defaultShaderLoaded = false;
        }
    }

    template <class Archive>
    void serialize(Archive& ar) {
        // Serialize shader reference
        std::string shaderUuidStr;
        if constexpr (Archive::is_saving::value) {
            if (!shaderAsset.is_nil()) {
                shaderUuidStr = boost::uuids::to_string(shaderAsset);
            }
        }
        ar(cereal::make_nvp("shader", shaderUuidStr));
        if constexpr (Archive::is_loading::value) {
            if (!shaderUuidStr.empty()) {
                shaderAsset = boost::uuids::string_generator()(shaderUuidStr);
            }
        }

        // Serialize properties
        if constexpr (Archive::is_saving::value) {
            std::vector<MaterialProperty> propList;
            for (const auto& [key, prop] : properties) {
                propList.push_back(prop);
            }
            ar(cereal::make_nvp("properties", propList));
        } else {
            std::vector<MaterialProperty> propList;
            ar(cereal::make_nvp("properties", propList));
            properties.clear();
            for (auto& prop : propList) {
                properties[prop.name] = std::move(prop);
            }
        }

        ar(CEREAL_NVP(doubleSided));
        ar(CEREAL_NVP(alphaBlend));
        rlMaterialDirty = true;
    }

    // Property setters (Unity-style)
    void setColor(const std::string& name, Color color) {
        properties[name] = MaterialProperty(name, MaterialPropertyValue(color));
        rlMaterialDirty  = true;
    }

    void setFloat(const std::string& name, float value) {
        properties[name] = MaterialProperty(name, MaterialPropertyValue(value));
        rlMaterialDirty  = true;
    }

    void setTexture(const std::string& name, const UUID& textureID, int slot = 0) {
        properties[name] = MaterialProperty(name, MaterialPropertyValue(textureID, slot));
        rlMaterialDirty  = true;

        auto it = cachedTextureHandles.find(name);
        if (it != cachedTextureHandles.end()) {
            cachedTextureHandles.erase(it);
        }
    }

    void setVector(const std::string& name, Vector3 value) {
        properties[name] = MaterialProperty(name, MaterialPropertyValue(value));
        rlMaterialDirty  = true;
    }

    void setVector2(const std::string& name, Vector2 value) {
        properties[name] = MaterialProperty(name, MaterialPropertyValue(value));
        rlMaterialDirty  = true;
    }

    void setVector4(const std::string& name, Vector4 value) {
        properties[name] = MaterialProperty(name, MaterialPropertyValue(value));
        rlMaterialDirty  = true;
    }

    void setInt(const std::string& name, int value) {
        properties[name] = MaterialProperty(name, MaterialPropertyValue(value));
        rlMaterialDirty  = true;
    }

    void setBool(const std::string& name, bool value) {
        properties[name] = MaterialProperty(name, MaterialPropertyValue(value));
        rlMaterialDirty  = true;
    }

    // Property getters
    Color getColor(const std::string& name, Color defaultValue = WHITE) const {
        auto it = properties.find(name);
        return (it != properties.end()) ? it->second.value.getColor() : defaultValue;
    }

    float getFloat(const std::string& name, float defaultValue = 0.0f) const {
        auto it = properties.find(name);
        return (it != properties.end()) ? it->second.value.getFloat() : defaultValue;
    }

    UUID getTexture(const std::string& name) const {
        auto it = properties.find(name);
        return (it != properties.end()) ? it->second.value.getTexture() : UUID();
    }

    Vector3 getVector3(const std::string& name, Vector3 defaultValue = { 0, 0, 0 }) const {
        auto it = properties.find(name);
        return (it != properties.end()) ? it->second.value.getVector3() : defaultValue;
    }

    bool hasProperty(const std::string& name) const {
        return properties.find(name) != properties.end();
    }

    // Apply material to a raylib material
    void applyToRaylibMaterial(raylib::Material& rlMat, AssetManager* assetMgr) {
        if (!assetMgr)
            return;

        if (!defaultShaderLoaded)
            this->loadDefaultShader();

        // Resolve shader
        Shader* activeShader = nullptr;
        if (!shaderAsset.is_nil()) {
            auto& sh = const_cast<AssetHandle<raylib::Shader>&>(cachedShaderHandle);
            if (!sh.isValid())
                sh = assetMgr->loadByID<raylib::Shader>(shaderAsset);
            if (sh.isValid() && sh.get()->id > 0)
                activeShader = sh.get().get();
        }
        if (!activeShader && defaultShaderLoaded)
            activeShader = &const_cast<Shader&>(defaultShader);

        if (activeShader)
            rlMat.shader = *activeShader;

        // Apply shader uniforms & textures
        for (const auto& [name, prop] : properties) {
            switch (prop.value.type) {
                case MaterialPropertyType::Color: {
                    Color c = prop.value.getColor();
                    if (activeShader && activeShader->id > 0) {
                        int loc = GetShaderLocation(*activeShader, name.c_str());
                        if (loc != -1) {
                            float v[4] = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f };
                            SetShaderValue(*activeShader, loc, v, SHADER_UNIFORM_VEC4);
                        }
                    }
                    break;
                }
                case MaterialPropertyType::Texture: {
                    UUID id   = prop.value.getTexture();
                    int  slot = prop.value.getTextureSlot();

                    if (slot >= 0 && slot < MATERIAL_MAP_BRDF) {
                        if (id.is_nil()) {
                            // create a Texture object from the default ID
                            Texture defaultTex = { 0 };
                            defaultTex.id      = rlGetTextureIdDefault();
                            defaultTex.width   = 1;
                            defaultTex.height  = 1;
                            defaultTex.mipmaps = 1;
                            defaultTex.format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

                            rlMat.maps[slot].texture = defaultTex;
                            break;
                        }

                        auto& texMap
                            = const_cast<std::map<std::string, AssetHandle<raylib::Texture>>&>(
                                cachedTextureHandles);

                        bool needsReload = false;
                        if (texMap.find(name) == texMap.end()) {
                            needsReload = true;
                        } else {
                            auto& existingHandle = texMap[name];
                            if (!existingHandle.isValid() || existingHandle.getID() != id)
                                needsReload = true;
                        }

                        if (needsReload)
                            texMap[name] = assetMgr->loadByID<raylib::Texture>(id);

                        auto& texH = texMap[name];
                        if (texH.isValid())
                            rlMat.maps[slot].texture = *texH.get();
                        else {
                            Texture defaultTex       = { 0 };
                            defaultTex.id            = rlGetTextureIdDefault();
                            defaultTex.width         = 1;
                            defaultTex.height        = 1;
                            defaultTex.mipmaps       = 1;
                            defaultTex.format        = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                            rlMat.maps[slot].texture = defaultTex;
                        }
                    }
                    break;
                }
                case MaterialPropertyType::Float: {
                    float v = prop.value.getFloat();
                    if (activeShader && activeShader->id > 0) {
                        int loc = GetShaderLocation(*activeShader, name.c_str());
                        if (loc != -1)
                            SetShaderValue(*activeShader, loc, &v, SHADER_UNIFORM_FLOAT);
                    }
                    break;
                }
                case MaterialPropertyType::Vector2: {
                    Vector2 v = prop.value.getVector2();
                    if (activeShader && activeShader->id > 0) {
                        int loc = GetShaderLocation(*activeShader, name.c_str());
                        if (loc != -1)
                            SetShaderValue(*activeShader, loc, &v, SHADER_UNIFORM_VEC2);
                    }
                    break;
                }
                case MaterialPropertyType::Vector3: {
                    Vector3 v = prop.value.getVector3();
                    if (activeShader && activeShader->id > 0) {
                        int loc = GetShaderLocation(*activeShader, name.c_str());
                        if (loc != -1)
                            SetShaderValue(*activeShader, loc, &v, SHADER_UNIFORM_VEC3);
                    }
                    break;
                }
                case MaterialPropertyType::Vector4: {
                    Vector4 v = prop.value.getVector4();
                    if (activeShader && activeShader->id > 0) {
                        int loc = GetShaderLocation(*activeShader, name.c_str());
                        if (loc != -1)
                            SetShaderValue(*activeShader, loc, &v, SHADER_UNIFORM_VEC4);
                    }
                    break;
                }
                case MaterialPropertyType::Int: {
                    int v = prop.value.getInt();
                    if (activeShader && activeShader->id > 0) {
                        int loc = GetShaderLocation(*activeShader, name.c_str());
                        if (loc != -1)
                            SetShaderValue(*activeShader, loc, &v, SHADER_UNIFORM_INT);
                    }
                    break;
                }
                case MaterialPropertyType::Bool: {
                    int v = prop.value.getBool() ? 1 : 0;
                    if (activeShader && activeShader->id > 0) {
                        int loc = GetShaderLocation(*activeShader, name.c_str());
                        if (loc != -1)
                            SetShaderValue(*activeShader, loc, &v, SHADER_UNIFORM_INT);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        int tex0Loc = GetShaderLocation(rlMat.shader, "texture0");
        if (tex0Loc != -1) {
            int unit0 = 0;
            SetShaderValue(rlMat.shader, tex0Loc, &unit0, SHADER_UNIFORM_INT);
        }
    }

    raylib::Material* getRaylibMaterial(AssetManager* assetMgr) {
        if (rlMaterialCache.shader.id == 0)
            rlMaterialCache = LoadMaterialDefault();

        if (!assetMgr) {
            if (!defaultShaderLoaded)
                loadDefaultShader();
            rlMaterialCache.shader = defaultShader;
            return &rlMaterialCache;
        }

        if (rlMaterialDirty) {
            applyToRaylibMaterial(rlMaterialCache, assetMgr);
            rlMaterialDirty = false;
        }

        return &rlMaterialCache;
    }
};

}
