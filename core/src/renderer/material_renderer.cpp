// material_renderer.cpp
#include "corvus/renderer/material_renderer.hpp"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/log.hpp"

namespace Corvus::Renderer {

MaterialRenderer::MaterialRenderer(Graphics::GraphicsContext& ctx) : context_(ctx) {
    initializeDefaults();
}

MaterialRenderer::~MaterialRenderer() {
    // Clear caches first to release asset handles while AssetManager still exists
    assetMaterialCache_.clear();

    // Release default resources
    if (defaultsInitialized_) {
        defaultShader_.release();
        defaultTexture_.release();
    }
}

void MaterialRenderer::initializeDefaults() {
    if (defaultsInitialized_)
        return;

    // Load default shader
    auto vsBytes
        = Core::StaticResourceFile::create("engine/shaders/default_lit.vert")->readAllBytes();
    auto fsBytes
        = Core::StaticResourceFile::create("engine/shaders/default_lit.frag")->readAllBytes();
    std::string vsSrc(vsBytes.begin(), vsBytes.end());
    std::string fsSrc(fsBytes.begin(), fsBytes.end());

    defaultShader_ = context_.createShader(vsSrc, fsSrc);

    if (defaultShader_.valid()) {
        CORVUS_CORE_INFO("Loaded default shader");
    } else {
        CORVUS_CORE_ERROR("Failed to load default shader");
    }

    // Create 1x1 white texture
    defaultTexture_  = context_.createTexture2D(1, 1);
    uint8_t pixel[4] = { 255, 255, 255, 255 };
    defaultTexture_.setData(pixel, sizeof(pixel));

    CORVUS_CORE_INFO("Created default white texture");

    defaultsInitialized_ = true;
}

Graphics::Shader& MaterialRenderer::getDefaultShader() {
    if (!defaultsInitialized_)
        initializeDefaults();
    return defaultShader_;
}

Graphics::Texture2D& MaterialRenderer::getDefaultTexture() {
    if (!defaultsInitialized_)
        initializeDefaults();
    return defaultTexture_;
}

void MaterialRenderer::clearCache() { assetMaterialCache_.clear(); }

// Apply low-level Material
Graphics::Shader* MaterialRenderer::apply(Material& material, Graphics::CommandBuffer& cmd) {

    // Get shader from material
    Graphics::Shader* shader = &material.getShader();

    if (!shader || !shader->valid()) {
        CORVUS_CORE_WARN("Material has invalid shader, using default");
        shader = &getDefaultShader();
    }

    if (!shader || !shader->valid()) {
        CORVUS_CORE_ERROR("No valid shader available");
        return nullptr;
    }

    const auto& textures = material.getTextures();
    bool        hasSlot0 = textures.find(0) != textures.end();

    // Apply the material
    const_cast<Material&>(material).bind(cmd);

    // Always apply a default texture to slot 0 if not provided when converting.
    if (!hasSlot0) {
        cmd.bindTexture(0, getDefaultTexture());
    }

    return shader;
}

// Convert MaterialAsset and apply
Graphics::Shader* MaterialRenderer::apply(const Core::MaterialAsset& materialAsset,
                                          Graphics::CommandBuffer&   cmd,
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

    // Get or create cache entry
    auto& cache = assetMaterialCache_[&materialAsset];

    // Check if we need to rebuild the material
    bool shaderChanged = cache.shaderID != materialAsset.shaderAsset;
    bool needsRebuild  = shaderChanged || cache.needsUpdate || !cache.material.has_value();

    if (needsRebuild) {
        // Get shader
        Graphics::Shader* shader = nullptr;

        if (!materialAsset.shaderAsset.is_nil() && assetMgr) {
            auto shaderHandle = assetMgr->loadByID<Graphics::Shader>(materialAsset.shaderAsset);
            if (shaderHandle.isValid()) {
                auto shaderPtr = shaderHandle.get();
                if (shaderPtr) {
                    shader = shaderPtr.get();
                }
            }
        }

        // Fall back to default shader
        if (!shader || !shader->valid()) {
            shader = &getDefaultShader();
        }

        if (!shader || !shader->valid()) {
            CORVUS_CORE_ERROR("No valid shader available for MaterialAsset");
            return nullptr;
        }

        // Create new Material with shader (use emplace to construct in-place)
        cache.material.emplace(*shader);
        cache.shaderID = materialAsset.shaderAsset;

        // Setup render state
        RenderState state;
        state.depthTest  = true;
        state.depthWrite = true;
        state.blend      = materialAsset.alphaBlend;
        state.cullFace   = !materialAsset.doubleSided;
        cache.material->setRenderState(state);

        // Apply all properties
        for (const auto& [name, prop] : materialAsset.properties) {
            switch (prop.value.type) {
                case Core::MaterialPropertyType::Float:
                    cache.material->setFloat(name, prop.value.floatValue);
                    break;

                case Core::MaterialPropertyType::Vector2:
                    cache.material->setVec2(name, prop.value.vec2Value);
                    break;

                case Core::MaterialPropertyType::Vector3:
                    cache.material->setVec3(name, prop.value.vec3Value);
                    break;

                case Core::MaterialPropertyType::Vector4:
                    cache.material->setVec4(name, prop.value.vec4Value);
                    break;

                case Core::MaterialPropertyType::Int:
                    cache.material->setInt(name, prop.value.intValue);
                    break;

                case Core::MaterialPropertyType::Bool:
                    cache.material->setInt(name, prop.value.boolValue ? 1 : 0);
                    break;

                case Core::MaterialPropertyType::Texture: {
                    Core::UUID texId = prop.value.getTexture();
                    int        slot  = prop.value.getTextureSlot();

                    if (texId.is_nil()) {
                        // Use default white texture
                        cache.material->setTexture(slot, getDefaultTexture());
                        break;
                    }

                    if (!assetMgr)
                        break;

                    // Check if texture needs reload
                    bool needsReload = cache.textureHandles.find(name) == cache.textureHandles.end()
                        || !cache.textureHandles[name].isValid()
                        || cache.textureHandles[name].getID() != texId;

                    if (needsReload) {
                        cache.textureHandles[name] = assetMgr->loadByID<Graphics::Texture2D>(texId);
                    }

                    if (cache.textureHandles[name].isValid()) {
                        auto texPtr = cache.textureHandles[name].get();
                        if (texPtr) {
                            cache.material->setTexture(slot, *texPtr);
                        }
                    } else {
                        // Fallback to default texture
                        cache.material->setTexture(slot, getDefaultTexture());
                    }
                    break;
                }
            }
        }

        cache.needsUpdate = false;
    }

    return &(*cache.material);
}

}
