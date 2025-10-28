#pragma once

#include "corvus/asset/asset_handle.hpp"
#include "corvus/systems/lighting_system.hpp"
#include "editor/panels/asset_browser/asset_viewer.hpp"
#include "raylib-cpp.hpp"
#include <array>

namespace Corvus::Editor {

class ModelViewer : public AssetViewer {
private:
    Core::AssetHandle<raylib::Model> modelHandle;

    // Preview rendering
    RenderTexture2D previewTexture     = { 0 };
    Camera3D        previewCamera      = { 0 };
    bool            previewInitialized = false;

    // Camera controls
    float   cameraAngleY   = 0.0f;
    float   cameraAngleX   = 0.0f;
    float   cameraDistance = 5.0f;
    bool    isDragging     = false;
    Vector2 lastMousePos   = { 0, 0 };
    bool    isPanning      = false;

    // Display options
    bool  showWireframe   = false;
    bool  showBoundingBox = false;
    bool  showGrid        = true;
    bool  autoRotate      = false;
    float autoRotateSpeed = 0.5f;

    // Lighting
    Core::Systems::LightingSystem previewLighting;
    bool                          needsPreviewUpdate = true;

    // Model info
    BoundingBox modelBounds = { 0 };

    void initPreview();
    void cleanupPreview();
    void updatePreview();
    void renderPreview();
    void handleCameraControls();
    void renderModelInfo(raylib::Model* model);
    void renderDisplayOptions();
    void calculateModelBounds(raylib::Model* model);

public:
    ModelViewer(const Core::UUID& id, Core::AssetManager* manager);
    ~ModelViewer() override;

    void render() override;
};

}
