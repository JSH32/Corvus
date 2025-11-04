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
 * Primary interface: Apply low-level Renderer::Material
 * Utility interface: Convert and apply Core::MaterialAsset
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
     * PRIMARY METHOD: Apply a low-level Material to the current render pass.
     * This is the core rendering logic - no asset system dependencies.
     *
     * @param material The renderer material to apply
     * @param cmd Command buffer to record commands to
     * @return The shader that was bound, or nullptr if no valid shader
     */
    Graphics::Shader* apply(Material& material, Graphics::CommandBuffer& cmd);

    /**
     * UTILITY METHOD: Apply a MaterialAsset by converting it to a Material first.
     * This is an adapter for the asset system.
     *
     * @param materialAsset The asset material (data) to convert and apply
     * @param cmd Command buffer to record commands to
     * @param assetMgr Asset manager for loading shader/textures
     * @return The shader that was bound, or nullptr if no valid shader
     */
    Graphics::Shader* apply(const Core::MaterialAsset& materialAsset,
                            Graphics::CommandBuffer&   cmd,
                            Core::AssetManager*        assetMgr);

    /**
     * Get or create a Material from a MaterialAsset.
     * Useful if you need the Material object directly.
     */
    Material* getMaterialFromAsset(const Core::MaterialAsset& materialAsset,
                                   Core::AssetManager*        assetMgr);

    /**
     * Clear all cached materials (call when scene changes)
     */
    void clearCache();

    /**
     * Get default resources
     */
    Graphics::Shader&    getDefaultShader();
    Graphics::Texture2D& getDefaultTexture();

private:
    Graphics::GraphicsContext& context_;

    // Default resources
    Graphics::Shader    defaultShader_;
    Graphics::Texture2D defaultTexture_;
    bool                defaultsInitialized_ = false;
    void                initializeDefaults();

    // Cache for MaterialAsset to Material conversion
    struct AssetMaterialCache {
        std::optional<Material>                                                 material;
        std::unordered_map<std::string, Core::AssetHandle<Graphics::Texture2D>> textureHandles;
        Core::UUID                                                              shaderID;
        bool                                                                    needsUpdate = true;
    };

    std::unordered_map<const Core::MaterialAsset*, AssetMaterialCache> assetMaterialCache_;

    // Convert MaterialAsset to Material
    Material* convertAssetToMaterial(const Core::MaterialAsset& materialAsset,
                                     Core::AssetManager*        assetMgr);
};

}
