#include "corvus/renderer/material_renderer.hpp"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/log.hpp"

namespace Corvus::Renderer {

MaterialRenderer::MaterialRenderer(Graphics::GraphicsContext& ctx) : context_(ctx) {
    initializeDefaults();
}

MaterialRenderer::~MaterialRenderer() {
    // Clear caches first to release asset handles while AssetManager still exists
    materialCaches_.clear();

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

void MaterialRenderer::clearCache() { materialCaches_.clear(); }

Graphics::Shader* MaterialRenderer::apply(const Core::MaterialAsset& material,
                                          Graphics::CommandBuffer&   cmd,
                                          Core::AssetManager*        assetMgr) {

    // Get shader
    Graphics::Shader* shader = nullptr;

    if (!material.shaderAsset.is_nil() && assetMgr) {
        auto shaderHandle = assetMgr->loadByID<Graphics::Shader>(material.shaderAsset);
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
        CORVUS_CORE_WARN("No valid shader available for material");
        return nullptr;
    }

    // Bind shader
    cmd.setShader(*shader);

    // Apply render state
    applyRenderState(cmd, material);

    // Apply material properties
    applyMaterialProperties(material, cmd, *shader, assetMgr);

    return shader;
}

void MaterialRenderer::applyRenderState(Graphics::CommandBuffer&   cmd,
                                        const Core::MaterialAsset& material) {
    cmd.setDepthTest(true);
    cmd.setDepthMask(true);
    cmd.setBlendState(material.alphaBlend);
    cmd.setCullFace(!material.doubleSided, false);
}

void MaterialRenderer::applyMaterialProperties(const Core::MaterialAsset& material,
                                               Graphics::CommandBuffer&   cmd,
                                               Graphics::Shader&          shader,
                                               Core::AssetManager*        assetMgr) {

    // Get or create cache for this material
    auto& cache = materialCaches_[&material];

    for (const auto& [name, prop] : material.properties) {
        switch (prop.value.type) {
            case Core::MaterialPropertyType::Float:
                shader.setFloat(cmd, name.c_str(), prop.value.floatValue);
                break;

            case Core::MaterialPropertyType::Vector2:
                shader.setVec2(cmd, name.c_str(), prop.value.vec2Value);
                break;

            case Core::MaterialPropertyType::Vector3:
                shader.setVec3(cmd, name.c_str(), prop.value.vec3Value);
                break;

            case Core::MaterialPropertyType::Vector4:
                shader.setVec4(cmd, name.c_str(), prop.value.vec4Value);
                break;

            case Core::MaterialPropertyType::Int:
                shader.setInt(cmd, name.c_str(), prop.value.intValue);
                break;

            case Core::MaterialPropertyType::Bool:
                shader.setInt(cmd, name.c_str(), prop.value.boolValue ? 1 : 0);
                break;

            case Core::MaterialPropertyType::Texture: {
                Core::UUID texId = prop.value.getTexture();
                int        slot  = prop.value.getTextureSlot();

                if (texId.is_nil()) {
                    // Use default white texture
                    auto& defaultTex = getDefaultTexture();
                    cmd.bindTexture(slot, defaultTex);
                    shader.setInt(cmd, name.c_str(), slot);
                    break;
                }

                if (!assetMgr)
                    break;

                // Check cache
                bool needsReload = cache.textureHandles.find(name) == cache.textureHandles.end()
                    || !cache.textureHandles[name].isValid()
                    || cache.textureHandles[name].getID() != texId;

                if (needsReload) {
                    cache.textureHandles[name] = assetMgr->loadByID<Graphics::Texture2D>(texId);
                }

                if (cache.textureHandles[name].isValid()) {
                    auto texPtr = cache.textureHandles[name].get();
                    if (texPtr) {
                        cmd.bindTexture(slot, *texPtr);
                        shader.setInt(cmd, name.c_str(), slot);
                    }
                }
                break;
            }
        }
    }
}

}
