
#pragma once

#include "editor/panels/editor_panel.hpp"
#include "editor/panels/scene_hierarchy.hpp"
#include "linp/scene.hpp"

#include "IconsFontAwesome6.h"
#include "imgui.h"
#include "raylib-cpp.hpp"

#include <algorithm>

namespace Linp::Editor {

/**
 * @class SceneViewPanel
 * @brief An editor panel displaying the 3D scene with camera controls and entity selection.
 *
 * Renders the scene to an ImGui texture, provides orbit/pan/zoom camera controls,
 * and allows entity selection synchronized with the SceneHierarchyPanel.
 */
class SceneViewPanel final : public EditorPanel {
public:
    /**
     * @brief Constructs a SceneViewPanel.
     * @param scene A reference to the main scene object.
     * @param sceneHierarchy A pointer to the SceneHierarchyPanel for selection synchronization.
     */
    SceneViewPanel(Core::Scene& scene, SceneHierarchyPanel* sceneHierarchy);

    /**
     * @brief Destructor. Cleans up the Raylib RenderTexture.
     */
    ~SceneViewPanel();

    /**
     * @brief Updates and renders the panel each frame.
     *
     * Manages viewport, camera, scene rendering to texture, and entity picking.
     */
    void onUpdate() override;

private:
    // Camera control constants
    static constexpr float CAMERA_MIN_DISTANCE     = 1.0f;
    static constexpr float CAMERA_MAX_DISTANCE     = 100.0f;
    static constexpr float CAMERA_ZOOM_SPEED       = 1.0f;
    static constexpr float CAMERA_ORBIT_SPEED      = 0.005f;
    static constexpr float CAMERA_PAN_SPEED_FACTOR = 0.002f;
    static constexpr float CAMERA_PITCH_MIN        = -PI / 2.0f + 0.01f;
    static constexpr float CAMERA_PITCH_MAX        = PI / 2.0f - 0.01f;
    static constexpr float CAMERA_DEFAULT_FOV      = 45.0f;

    /**
     * @brief Updates the camera's 3D position and target based on orbit angles, distance, and target point.
     */
    void updateCameraVectors();

    /**
     * @brief Handles user input for camera controls (orbit, pan, zoom).
     * @param io A reference to the ImGuiIO object for input state.
     */
    void processCameraInput(const ImGuiIO& io);

    /**
     * @brief Handles entity selection via mouse click.
     * @param mousePosInTexture Mouse position relative to the scene texture.
     * @param textureWidth Width of the scene texture.
     * @param textureHeight Height of the scene texture.
     */
    void processEntityPicking(const Vector2& mousePosInTexture, int textureWidth, int textureHeight);

    /**
     * @brief Manages the RenderTexture (creation, resizing).
     * @param size Desired size of the RenderTexture.
     */
    void manageRenderTexture(const ImVec2& size);

    /**
     * @brief Renders the current scene into the internal RenderTexture.
     */
    void renderSceneToTexture();

    Core::Scene&         scene;
    SceneHierarchyPanel* sceneHierarchyPanel;

    raylib::RenderTexture sceneTexture;
    raylib::Camera3D      camera;
    ImVec2                currentViewportSize;

    Vector3 cameraTarget;
    Vector2 cameraOrbitAngles; // x: pitch, y: yaw (radians)
    float   cameraDistance;

    bool isWindowFocused;
    bool isWindowHovered;
};

} // namespace Linp::Editor
