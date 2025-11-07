// material_renderer.hpp
#pragma once
#include "corvus/asset/asset_manager.hpp"
#include "corvus/asset/material/material.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/material.hpp"
#include <unordered_map>

namespace Corvus::Renderer {

/**
 * Handles the rendering side of materials.
 * Apply low-level Renderer::Material
 * Convert and apply Core::MaterialAsset
 */
class MaterialRenderer {
public:
    explicit MaterialRenderer(Graphics::GraphicsContext& ctx);
    ~MaterialRenderer();

    MaterialRenderer(const MaterialRenderer&)            = delete;
    MaterialRenderer& operator=(const MaterialRenderer&) = delete;
    MaterialRenderer(MaterialRenderer&&)                 = delete;
    MaterialRenderer& operator=(MaterialRenderer&&)      = delete;

    /**
     * Apply a Material to the current render pass.
     *
     * @param material The renderer material to apply
     * @param cmd Command buffer to record commands to
     * @return The shader that was bound or nullptr if no valid shader
     */
    Shader* apply(Material& material, CommandBuffer& cmd);

    /**
     * Apply a MaterialAsset by converting it to a Material first.
     * This is an adapter for the asset system.
     *
     * @param materialAsset The asset material (data) to convert and apply
     * @param cmd Command buffer to record commands to
     * @param assetMgr Asset manager for loading shader/textures
     * @return The shader that was bound or nullptr if no valid shader
     */
    Shader* apply(const Core::MaterialAsset& materialAsset,
                  CommandBuffer&             cmd,
                  Core::AssetManager*        assetMgr);

    /**
     * Get or create a Material from a MaterialAsset.
     * Useful if you need the Material object directly.
     */
    Material* getMaterialFromAsset(const Core::MaterialAsset& materialAsset,
                                   Core::AssetManager*        assetMgr);

    /**
     * Get default resources
     */
    Shader&    getDefaultShader();
    Texture2D& getDefaultTexture();

private:
    Graphics::GraphicsContext& context;

    // Default resources
    Shader    defaultShader;
    Texture2D defaultTexture;
    bool      defaultsInitialized = false;
    void      initializeDefaults();
};

}
