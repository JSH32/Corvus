// material_viewer.hpp
#pragma once
#include "asset_viewer.hpp"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/asset/material/material.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/camera.hpp"
#include "corvus/renderer/model.hpp"
#include "corvus/renderer/scene_renderer.hpp"
#include "imgui.h"
#include <array>

namespace Corvus::Editor {

class MaterialViewer final : public AssetViewer {
    Core::AssetHandle<Core::MaterialAsset> materialHandle;

    // Graphics context and rendering
    Graphics::GraphicsContext* context_ = nullptr;

    // Preview scene
    Renderer::Camera        previewCamera;
    Renderer::SceneRenderer sceneRenderer;

    // Preview mesh (just store the model directly)
    Renderer::Model previewModel;
    glm::mat4       previewTransform = glm::mat4(1.0f);

    // Render target
    Graphics::Framebuffer framebuffer;
    Graphics::Texture2D   colorTexture;
    Graphics::Texture2D   depthTexture;
    uint32_t              previewResolution = 512;

    bool needsPreviewUpdate = true;

    // Camera rotation
    ImVec2 lastMousePos   = { 0, 0 };
    bool   isDragging     = false;
    float  cameraAngleX   = 0.0f;
    float  cameraAngleY   = 0.0f;
    float  cameraDistance = 3.0f;

    // UI state
    std::array<char, 256> propertyNameBuffer;
    bool                  showAddPropertyPopup = false;

    void renderPreview();
    void updatePreview();
    void handleCameraControls();
    void setupPreviewLights();
    void updateCameraPosition();

    bool renderColorProperty(const std::string& name, const Core::MaterialProperty& prop) const;
    bool renderFloatProperty(const std::string& name, const Core::MaterialProperty& prop) const;
    bool renderTextureProperty(const std::string& name, const Core::MaterialProperty& prop) const;
    bool renderVectorProperty(const std::string& name, const Core::MaterialProperty& prop) const;
    void renderAddPropertyPopup();

public:
    MaterialViewer(const Core::UUID&          id,
                   Core::AssetManager*        manager,
                   Graphics::GraphicsContext& context);
    ~MaterialViewer() override;

    void render() override;
};

}
