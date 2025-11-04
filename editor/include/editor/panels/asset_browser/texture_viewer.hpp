#pragma once
#include "asset_viewer.hpp"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/graphics/graphics.hpp"
#include "imgui.h"
#include <glm/glm.hpp>

namespace Corvus::Editor {

class TextureViewer : public AssetViewer {
public:
    TextureViewer(const Core::UUID&          id,
                  Core::AssetManager*        manager,
                  Graphics::GraphicsContext& ctx);
    ~TextureViewer();

    void render() override;

private:
    void initPreview();
    void cleanupPreview();

    void handleZoomAndPan();
    void renderTexturePreview();
    void renderTextureInfo(Graphics::Texture2D* tex);

    Graphics::GraphicsContext*             context_ { nullptr };
    Core::AssetHandle<Graphics::Texture2D> textureHandle;

    Graphics::Framebuffer previewFramebuffer;
    Graphics::Texture2D   previewColorTex;
    Graphics::Texture2D   previewDepthTex;

    float  zoom               = 1.0f;
    ImVec2 panOffset          = { 0, 0 };
    ImVec2 lastMousePos       = { 0, 0 };
    bool   isPanning          = false;
    bool   showAlpha          = true;
    bool   showGrid           = true;
    bool   fitToWindow        = false;
    bool   previewInitialized = false;

    const uint32_t previewResolution = 512;
};

}
