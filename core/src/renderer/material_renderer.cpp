// material_renderer.cpp
#include "corvus/renderer/material_renderer.hpp"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/log.hpp"

namespace Corvus::Renderer {

MaterialRenderer::MaterialRenderer(Graphics::GraphicsContext& ctx) : context(ctx) {
    initializeDefaults();
}

MaterialRenderer::~MaterialRenderer() {
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

    if (!assetMgr)
        return nullptr;

    Material* material = materialAsset.getRuntimeMaterial(*this, *assetMgr);
    if (!material) {
        CORVUS_CORE_WARN("Failed to retrieve runtime material from MaterialAsset");
        return nullptr;
    }

    return apply(*material, cmd);
}

Material* MaterialRenderer::getMaterialFromAsset(const Core::MaterialAsset& materialAsset,
                                                 Core::AssetManager*        assetMgr) {
    if (!assetMgr)
        return nullptr;
    return materialAsset.getRuntimeMaterial(*this, *assetMgr);
}

}
