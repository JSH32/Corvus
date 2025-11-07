#include "corvus/asset/material/material.hpp"

#include "corvus/asset/asset_manager.hpp"
#include "corvus/renderer/material_renderer.hpp"

namespace Corvus::Core {
Renderer::Material* MaterialAsset::getRuntimeMaterial(Renderer::MaterialRenderer& renderer,
                                                      AssetManager&               assets) const {
    if (!runtimeMaterial) {
        runtimeMaterial = std::make_unique<Renderer::Material>(renderer.getDefaultShader());
        needsRebuild    = true;
    }

    if (needsRebuild) {
        const Graphics::Shader* shader = nullptr;

        if (!shaderAsset.is_nil()) {
            if (const auto shaderHandle = assets.loadByID<Graphics::Shader>(shaderAsset);
                shaderHandle.isValid()) {
                const auto shaderPtr = shaderHandle.get();
                if (shaderPtr)
                    shader = shaderPtr.get();
            }
        }

        if (!shader || !shader->valid())
            shader = &renderer.getDefaultShader();

        // If this is the default shader, nothing will be released
        runtimeMaterial->setShader(*shader, false);

        Renderer::RenderState state;
        state.depthTest  = true;
        state.depthWrite = true;
        state.blend      = alphaBlend;
        state.cullFace   = !doubleSided;
        runtimeMaterial->setRenderState(state);

        // Apply all properties
        for (const auto& [name, prop] : properties) {
            switch (prop.value.type) {
                case MaterialPropertyType::Float:
                    runtimeMaterial->setFloat(name, prop.value.floatValue);
                    break;

                case MaterialPropertyType::Vector2:
                    runtimeMaterial->setVec2(name, prop.value.vec2Value);
                    break;

                case MaterialPropertyType::Vector3:
                    runtimeMaterial->setVec3(name, prop.value.vec3Value);
                    break;

                case MaterialPropertyType::Vector4:
                    runtimeMaterial->setVec4(name, prop.value.vec4Value);
                    break;

                case MaterialPropertyType::Int:
                    runtimeMaterial->setInt(name, prop.value.intValue);
                    break;

                case MaterialPropertyType::Bool:
                    runtimeMaterial->setInt(name, prop.value.boolValue ? 1 : 0);
                    break;

                case MaterialPropertyType::Texture: {
                    const UUID texID = prop.value.getTexture();
                    const int  slot  = prop.value.getTextureSlot();

                    if (texID.is_nil()) {
                        runtimeMaterial->setTexture(slot, renderer.getDefaultTexture());
                        break;
                    }

                    auto texHandle = assets.loadByID<Graphics::Texture2D>(texID);
                    if (texHandle.isValid()) {
                        if (auto texPtr = texHandle.get())
                            runtimeMaterial->setTexture(slot, *texPtr);
                        else
                            runtimeMaterial->setTexture(slot, renderer.getDefaultTexture());
                    } else {
                        runtimeMaterial->setTexture(slot, renderer.getDefaultTexture());
                    }
                    break;
                }
            }
        }

        needsRebuild = false;
    }

    return runtimeMaterial.get();
}

bool MaterialAsset::hasProperty(std::string_view name) const {
    return properties.contains(std::string(name));
}

const MaterialProperty* MaterialAsset::getProperty(std::string_view name) const {
    const auto it = properties.find(std::string(name));
    return it != properties.end() ? &it->second : nullptr;
}

MaterialProperty* MaterialAsset::getProperty(std::string_view name) {
    const auto it = properties.find(std::string(name));
    return it != properties.end() ? &it->second : nullptr;
}

void MaterialAsset::setProperty(const MaterialProperty& prop) {
    properties[prop.name] = prop;
    markDirty();
}

void MaterialAsset::setProperty(std::string_view name, const MaterialPropertyValue& value) {
    properties[std::string(name)] = MaterialProperty(std::string(name), value);
    markDirty();
}

bool MaterialAsset::removeProperty(const std::string_view name) {
    return properties.erase(std::string(name)) > 0;
}

}
