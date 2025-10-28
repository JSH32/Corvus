#pragma once

#include "IconsFontAwesome6.h"
#include "editor/panels/editor_panel.hpp"
#include "editor/panels/scene_hierarchy.hpp"
#include "editor/panels/scene_view/scene_viewport.hpp"
#include "imgui.h"
#include "linp/scene.hpp"
#include "raylib.h"

namespace Linp::Editor {

/**
 * @class SceneViewPanel
 * @brief An ImGui panel that displays a 3D scene viewport with camera controls and entity
 * selection.
 *
 * Wraps SceneViewport with ImGui-specific UI handling, window management, and integration
 * with the SceneHierarchyPanel for entity selection synchronization.
 */
class SceneViewPanel final : public EditorPanel {
public:
    /**
     * @brief Constructs a SceneViewPanel.
     * @param scene A reference to the main scene object.
     * @param sceneHierarchy A pointer to the SceneHierarchyPanel for selection synchronization.
     */
    SceneViewPanel(Core::Project& scene, SceneHierarchyPanel* sceneHierarchy);

    /**
     * @brief Updates and renders the panel each frame.
     *
     * Manages ImGui window, viewport display, mouse interaction, and entity selection.
     */
    void onUpdate() override;

    /**
     * @brief Gets the scene viewport for external access.
     */
    SceneViewport&       getViewport() { return viewport; }
    const SceneViewport& getViewport() const { return viewport; }

    /**
     * @brief Gets the editor camera for external access.
     */
    EditorCamera&       getCamera() { return viewport.getCamera(); }
    const EditorCamera& getCamera() const { return viewport.getCamera(); }

private:
    /**
     * @brief Updates mouse state for viewport interaction.
     * @param imageTopLeft Top-left position of the rendered image in screen coordinates.
     * @param imageHovered Whether the viewport image is currently hovered.
     */
    void updateMouseState(const ImVec2& imageTopLeft, bool imageHovered);

    /**
     * @brief Handles entity picking when the user clicks in the viewport.
     */
    void handleEntityPicking();

    void handleShortcuts();
    void renderGizmoTooltip();

    // Core components
    SceneViewport        viewport;
    SceneHierarchyPanel* sceneHierarchyPanel;

    // ImGui window state
    ImVec2 currentViewportSize;

    // Mouse state (viewport-relative coordinates)
    Vector2 currentMousePos = { 0, 0 };
    bool    mousePressed    = false;
    bool    mouseDown       = false;
    bool    mouseInViewport = false;
};

}
