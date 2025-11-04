#pragma once
#include "corvus/asset/asset_manager.hpp"
#include "corvus/asset/material/material.hpp"
#include "corvus/graphics/graphics.hpp"
#include <unordered_map>

namespace Corvus::Renderer {

/**
 * Handles the rendering side of materials.
 * Converts MaterialAsset (pure data) into GPU state and uniforms.
 */
class MaterialRenderer {
public:
    MaterialRenderer(Graphics::GraphicsContext& ctx);
    ~MaterialRenderer();

    MaterialRenderer(const MaterialRenderer&)            = delete;
    MaterialRenderer& operator=(const MaterialRenderer&) = delete;
    MaterialRenderer(MaterialRenderer&&)                 = delete;
    MaterialRenderer& operator=(MaterialRenderer&&)      = delete;

    /**
     * Apply a MaterialAsset to the current render pass.
     * Sets up shader, textures, render state, and material uniforms.
     *
     * @return The shader that was bound, or nullptr if no valid shader
     */
    Graphics::Shader* apply(const Core::MaterialAsset& material,
                            Graphics::CommandBuffer&   cmd,
                            Core::AssetManager*        assetMgr);

    /**
     * Clear all cached materials (call when scene changes)
     */
    void clearCache();

private:
    Graphics::GraphicsContext& context_;

    // Default resources
    Graphics::Shader    defaultShader_;
    Graphics::Texture2D defaultTexture_;
    bool                defaultsInitialized_ = false;

    void                 initializeDefaults();
    Graphics::Shader&    getDefaultShader();
    Graphics::Texture2D& getDefaultTexture();

    void applyRenderState(Graphics::CommandBuffer& cmd, const Core::MaterialAsset& material);
    void applyMaterialProperties(const Core::MaterialAsset& material,
                                 Graphics::CommandBuffer&   cmd,
                                 Graphics::Shader&          shader,
                                 Core::AssetManager*        assetMgr);

    // Cache texture handles per material to avoid repeated lookups
    struct MaterialCache {
        std::unordered_map<std::string, Core::AssetHandle<Graphics::Texture2D>> textureHandles;
    };
    std::unordered_map<const Core::MaterialAsset*, MaterialCache> materialCaches_;
};

}
