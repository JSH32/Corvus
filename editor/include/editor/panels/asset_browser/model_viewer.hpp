#pragma once
#include "asset_viewer.hpp"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/camera.hpp"
#include "corvus/renderer/model.hpp"
#include "corvus/renderer/scene_renderer.hpp"
#include "imgui.h"
#include <glm/glm.hpp>

namespace Corvus::Editor {

class ModelViewer final : public AssetViewer {
    Core::AssetHandle<Renderer::Model> modelHandle;
    Graphics::GraphicsContext*         context_ = nullptr;

    Renderer::Camera        previewCamera;
    Renderer::SceneRenderer sceneRenderer;

    Graphics::Framebuffer framebuffer;
    Graphics::Texture2D   colorTexture;
    Graphics::Texture2D   depthTexture;
    uint32_t              previewResolution = 512;

    bool needsPreviewUpdate = true;

    // Camera controls
    ImVec2 lastMousePos    = { 0, 0 };
    bool   isDragging      = false;
    float  cameraAngleX    = 0.0f;
    float  cameraAngleY    = 0.0f;
    float  cameraDistance  = 3.0f;
    bool   autoRotate      = false;
    float  autoRotateSpeed = 0.3f;

    // Display options
    bool showWireframe   = false;
    bool showBoundingBox = false;
    bool showGrid        = true;

    // Cached bounds
    glm::vec3 boundsMin { 0.0f };
    glm::vec3 boundsMax { 0.0f };
    glm::vec3 modelCenter { 0.0f };

    void renderPreview();
    void updatePreview();
    void handleCameraControls();
    void setupPreviewLights();
    void updateCameraPosition();
    void calculateModelBounds();

    void renderModelInfo(const Renderer::Model& model);
    void renderDisplayOptions();

public:
    ModelViewer(const Core::UUID&          id,
                Core::AssetManager*        manager,
                Graphics::GraphicsContext& context);
    ~ModelViewer() override;

    void render() override;
};

}
