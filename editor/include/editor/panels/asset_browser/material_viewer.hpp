#pragma once
#include "asset_viewer.hpp"
#include "imgui.h"
#include "linp/asset/asset_handle.hpp"
#include "linp/asset/material/material_loader.hpp"
#include "linp/systems/lighting_system.hpp"
#include "raylib-cpp.hpp"
#include "raylib.h"
#include <array>

namespace Linp::Editor {

class MaterialViewer : public AssetViewer {
private:
    Core::AssetHandle<Core::Material> materialHandle;
    Core::Systems::LightingSystem     previewLighting;

    // Preview rendering
    RenderTexture2D previewTexture     = { 0 };
    Camera3D        previewCamera      = { 0 };
    Model           previewSphere      = { 0 };
    bool            previewInitialized = false;
    bool            needsPreviewUpdate = true;

    // Camera rotation
    Vector2 lastMousePos = { 0, 0 };
    bool    isDragging   = false;
    float   cameraAngleX = 0.0f;
    float   cameraAngleY = 0.0f;

    // UI state
    std::array<char, 256> nameBuffer;
    std::array<char, 256> propertyNameBuffer;
    bool                  showAddPropertyPopup = false;

    void initPreview();
    void cleanupPreview();
    void renderPreview();
    void updatePreview();
    void handleCameraControls();

    bool renderColorProperty(const std::string& name, Core::MaterialProperty& prop);
    bool renderFloatProperty(const std::string& name, Core::MaterialProperty& prop);
    bool renderTextureProperty(const std::string& name, Core::MaterialProperty& prop);
    bool renderVectorProperty(const std::string& name, Core::MaterialProperty& prop);
    void renderAddPropertyPopup();

public:
    MaterialViewer(const Core::UUID& id, Core::AssetManager* manager);
    ~MaterialViewer();
    void render() override;
};

}
