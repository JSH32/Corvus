// material_renderer.cpp
#include "corvus/renderer/material_renderer.hpp"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/log.hpp"

namespace Corvus::Renderer {

MaterialRenderer::MaterialRenderer(Graphics::GraphicsContext& ctx) : context(ctx) {
    initializeDefaults();
}

MaterialRenderer::~MaterialRenderer() {
    // Clear caches first to release asset handles while AssetManager still exists
    assetMaterialCache.clear();

    // Release default resources
    if (defaultsInitialized) {
        defaultShader.release();
        defaultTexture.release();
    }
}

void MaterialRenderer::initializeDefaults() {
    if (defaultsInitialized)
        return;

    // Load default shader
    auto vsBytes
        = Core::StaticResourceFile::create("engine/shaders/default_lit.vert")->readAllBytes();
    auto fsBytes
        = Core::StaticResourceFile::create("engine/shaders/default_lit.frag")->readAllBytes();
    const std::string vsSrc(vsBytes.begin(), vsBytes.end());
    const std::string fsSrc(fsBytes.begin(), fsBytes.end());

    defaultShader = context.createShader(vsSrc, fsSrc);

    if (defaultShader.valid()) {
        CORVUS_CORE_INFO("Loaded default shader");
    } else {
        CORVUS_CORE_ERROR("Failed to load default shader");
    }

    // Create 1x1 white texture
    defaultTexture             = context.createTexture2D(1, 1);
    constexpr uint8_t pixel[4] = { 255, 255, 255, 255 };
    defaultTexture.setData(pixel, sizeof(pixel));

    CORVUS_CORE_INFO("Created default white texture");

    defaultsInitialized = true;
}

Shader& MaterialRenderer::getDefaultShader() {
    if (!defaultsInitialized)
        initializeDefaults();
    return defaultShader;
}

Texture2D& MaterialRenderer::getDefaultTexture() {
    if (!defaultsInitialized)
        initializeDefaults();
    return defaultTexture;
}

void MaterialRenderer::clearCache() { assetMaterialCache.clear(); }

// Apply low-level Material
Shader* MaterialRenderer::apply(Material& material, CommandBuffer& cmd) {

    // Get shader from material
    Shader* shader = &material.getShader();

    if (!shader || !shader->valid()) {
        CORVUS_CORE_WARN("Material has invalid shader, using default");
        shader = &getDefaultShader();
    }

    if (!shader || !shader->valid()) {
        CORVUS_CORE_ERROR("No valid shader available");
        return nullptr;
    }

    const auto& textures = material.getTextures();
    const bool  hasSlot0 = textures.contains(0);

    // Apply the material
    material.bind(cmd);

    // Always apply a default texture to slot 0 if not provided when converting.
    if (!hasSlot0) {
        cmd.bindTexture(0, getDefaultTexture());
    }

    return shader;
}

// Convert MaterialAsset and apply
Shader* MaterialRenderer::apply(const Core::MaterialAsset& materialAsset,
                                CommandBuffer&             cmd,
                                Core::AssetManager*        assetMgr) {

    // Convert asset to material (or get from cache)
    Material* material = convertAssetToMaterial(materialAsset, assetMgr);

    if (!material) {
        CORVUS_CORE_WARN("Failed to convert MaterialAsset to Material");
        return nullptr;
    }

    // Use the primary apply method
    return apply(*material, cmd);
}

Material* MaterialRenderer::getMaterialFromAsset(const Core::MaterialAsset& materialAsset,
                                                 Core::AssetManager*        assetMgr) {
    return convertAssetToMaterial(materialAsset, assetMgr);
}

// MaterialAsset to Material conversion with caching
Material* MaterialRenderer::convertAssetToMaterial(const Core::MaterialAsset& materialAsset,
                                                   Core::AssetManager*        assetMgr) {

    // Get or create a cache entry
    auto& [material, textureHandles, shaderID, needsUpdate] = assetMaterialCache[&materialAsset];

    // Check if we need to rebuild the material
    if (const bool shaderChanged = shaderID != materialAsset.shaderAsset;
        shaderChanged || needsUpdate || !material.has_value()) {
        // Get shader
        Shader* shader = nullptr;

        if (!materialAsset.shaderAsset.is_nil() && assetMgr) {
            const auto shaderHandle = assetMgr->loadByID<Shader>(materialAsset.shaderAsset);
            if (shaderHandle.isValid()) {
                const auto shaderPtr = shaderHandle.get();
                if (shaderPtr) {
                    shader = shaderPtr.get();
                }
            }
        }

        // Fall back to the default shader
        if (!shader || !shader->valid()) {
            shader = &getDefaultShader();
        }

        if (!shader || !shader->valid()) {
            CORVUS_CORE_ERROR("No valid shader available for MaterialAsset");
            return nullptr;
        }

        // Create new Material with shader (use emplace to construct in-place)
        material.emplace(*shader);
        shaderID = materialAsset.shaderAsset;

        // Setup render state
        RenderState state;
        state.depthTest  = true;
        state.depthWrite = true;
        state.blend      = materialAsset.alphaBlend;
        state.cullFace   = !materialAsset.doubleSided;
        material->setRenderState(state);

        // Apply all properties
        for (const auto& [name, prop] : materialAsset.properties) {
            switch (prop.value.type) {
                case Core::MaterialPropertyType::Float:
                    material->setFloat(name, prop.value.floatValue);
                    break;

                case Core::MaterialPropertyType::Vector2:
                    material->setVec2(name, prop.value.vec2Value);
                    break;

                case Core::MaterialPropertyType::Vector3:
                    material->setVec3(name, prop.value.vec3Value);
                    break;

                case Core::MaterialPropertyType::Vector4:
                    material->setVec4(name, prop.value.vec4Value);
                    break;

                case Core::MaterialPropertyType::Int:
                    material->setInt(name, prop.value.intValue);
                    break;

                case Core::MaterialPropertyType::Bool:
                    material->setInt(name, prop.value.boolValue ? 1 : 0);
                    break;

                case Core::MaterialPropertyType::Texture: {
                    Core::UUID texId = prop.value.getTexture();
                    const int  slot  = prop.value.getTextureSlot();

                    if (texId.is_nil()) {
                        // Use the default white texture
                        material->setTexture(slot, getDefaultTexture());
                        break;
                    }

                    if (!assetMgr)
                        break;

                    // Check if texture needs reload
                    const bool needsReload = !textureHandles.contains(name)
                        || !textureHandles[name].isValid() || textureHandles[name].getID() != texId;

                    if (needsReload) {
                        textureHandles[name] = assetMgr->loadByID<Texture2D>(texId);
                    }

                    if (textureHandles[name].isValid()) {
                        if (auto texPtr = textureHandles[name].get()) {
                            material->setTexture(slot, *texPtr);
                        }
                    } else {
                        // Fallback to default texture
                        material->setTexture(slot, getDefaultTexture());
                    }
                    break;
                }
            }
        }

        needsUpdate = false;
    }

    return &(*material);
}

}
