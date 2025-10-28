#pragma once
#include "asset_viewer.hpp"
#include "imgui.h"
#include "linp/asset/asset_handle.hpp"
#include "raylib-cpp.hpp"
#include "raylib.h"

namespace Linp::Editor {

class TextureViewer : public AssetViewer {
private:
    Core::AssetHandle<raylib::Texture> textureHandle;

    // Zoom and pan
    float   zoom         = 1.0f;
    Vector2 panOffset    = { 0.0f, 0.0f };
    Vector2 lastMousePos = { 0, 0 };
    bool    isPanning    = false;

    // Display options
    bool showAlpha      = true;
    bool showGrid       = true;
    bool fitToWindow    = false;
    int  displayChannel = 0; // 0=RGB, 1=R, 2=G, 3=B, 4=A

    void handleZoomAndPan();
    void renderTexturePreview();
    void renderTextureInfo(raylib::Texture* tex);

public:
    TextureViewer(const Core::UUID& id, Core::AssetManager* manager);
    ~TextureViewer() = default;

    void render() override;
};

}
