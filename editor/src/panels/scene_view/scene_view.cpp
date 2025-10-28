#include "editor/panels/scene_view/scene_view.hpp"

#include "imgui.h"
#include "corvus/components/transform.hpp"
#include <algorithm>

namespace Corvus::Editor {

SceneViewPanel::SceneViewPanel(Core::Project& project, SceneHierarchyPanel* sceneHierarchy)
    : viewport(project), sceneHierarchyPanel(sceneHierarchy), currentViewportSize({ 1.0f, 1.0f }) {
}

void SceneViewPanel::updateMouseState(const ImVec2& imageTopLeft, bool imageHovered) {
    bool leftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool leftDown    = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    mouseInViewport = imageHovered;
    mousePressed    = leftClicked && imageHovered;
    mouseDown       = leftDown;

    if (imageHovered) {
        ImVec2 mousePosAbsolute = ImGui::GetMousePos();
        currentMousePos
            = { mousePosAbsolute.x - imageTopLeft.x, mousePosAbsolute.y - imageTopLeft.y };
    } else {
        currentMousePos = { 0, 0 };
    }
}

void SceneViewPanel::handleEntityPicking() {
    if (!mousePressed || !mouseInViewport) {
        return;
    }

    // Don't pick if gizmo is active or hovered
    if (viewport.getGizmo().isActive() || viewport.getGizmo().isHovered()) {
        return;
    }

    // Perform entity picking
    Core::Entity pickedEntity = viewport.pickEntity(currentMousePos);

    // Update selection in hierarchy panel
    if (sceneHierarchyPanel) {
        sceneHierarchyPanel->selectedEntity = pickedEntity; // Will be empty if nothing picked
    }
}

void SceneViewPanel::renderGizmoTooltip() {
    // Position overlay at top-left of viewport
    ImVec2 overlayPos = ImVec2(ImGui::GetItemRectMin().x + 5.0f, ImGui::GetItemRectMin().y + 5.0f);

    ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    constexpr ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("GizmoOverlay", nullptr, overlayFlags)) {
        EditorGizmo::Mode currentMode = viewport.getGizmo().getMode();

        // Setup button styling
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));

        // Gizmo mode data
        struct GizmoButton {
            const char*       icon;
            EditorGizmo::Mode mode;
            const char*       tooltip;
        };

        const GizmoButton buttons[] = {
            { ICON_FA_WRENCH, EditorGizmo::Mode::All, "All modes (Q)" },
            { ICON_FA_UP_DOWN_LEFT_RIGHT, EditorGizmo::Mode::Translate, "Move (W)" },
            { ICON_FA_ROTATE, EditorGizmo::Mode::Rotate, "Rotate (E)" },
            { ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, EditorGizmo::Mode::Scale, "Scale (R)" }
        };

        // Render buttons dynamically
        for (int i = 0; i < 4; ++i) {
            const auto& btn      = buttons[i];
            bool        isActive = (currentMode == btn.mode);

            if (isActive)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));

            if (ImGui::Button((std::string(btn.icon) + "##" + std::to_string(i)).c_str())) {
                viewport.getGizmo().setMode(btn.mode);
            }

            if (isActive)
                ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", btn.tooltip);
            if (i < 3)
                ImGui::SameLine();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void SceneViewPanel::handleShortcuts() {
    bool sceneViewFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    bool hierarchyFocused = sceneHierarchyPanel->isFocused();

    if (!sceneViewFocused && !hierarchyFocused)
        return;

    // Focus
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        if (sceneHierarchyPanel->selectedEntity) {
            auto transform = sceneHierarchyPanel->selectedEntity
                                 .getComponent<Core::Components::TransformComponent>();
            getCamera().focusOn(transform.position);
        }
    }

    static const std::unordered_map<ImGuiKey, EditorGizmo::Mode> keyMap
        = { { ImGuiKey_Q, EditorGizmo::Mode::All },
            { ImGuiKey_W, EditorGizmo::Mode::Translate },
            { ImGuiKey_E, EditorGizmo::Mode::Rotate },
            { ImGuiKey_R, EditorGizmo::Mode::Scale } };

    for (const auto& [key, mode] : keyMap) {
        if (ImGui::IsKeyPressed(key)) {
            viewport.getGizmo().setMode(mode);
            break;
        }
    }
}

void SceneViewPanel::onUpdate() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin(ICON_FA_CUBES " Scene View")) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }
    ImGui::PopStyleVar();

    this->handleShortcuts();

    // Update window state
    bool isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    bool isWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    // Handle viewport size changes
    ImVec2 newViewportSize = ImGui::GetContentRegionAvail();
    newViewportSize.x      = std::max(1.0f, newViewportSize.x);
    newViewportSize.y      = std::max(1.0f, newViewportSize.y);

    if (newViewportSize.x != currentViewportSize.x || newViewportSize.y != currentViewportSize.y) {
        currentViewportSize = newViewportSize;
    }

    // Update camera input (only when panel is interactive and gizmo isn't active)
    bool cameraInputAllowed
        = !viewport.getGizmo().isActive() && (isWindowFocused || isWindowHovered);
    viewport.updateCamera(ImGui::GetIO(), cameraInputAllowed);

    // Render viewport
    viewport.render(currentViewportSize);

    // Display the rendered scene
    if (!viewport.isValid() || currentViewportSize.x <= 0 || currentViewportSize.y <= 0) {
        ImGui::TextDisabled("Scene View not available (texture error or zero size).");
    } else {
        ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();

        const RenderTexture& renderTexture = viewport.getRenderTexture();
        ImGui::Image((ImTextureID)(intptr_t)renderTexture.texture.id,
                     currentViewportSize,
                     ImVec2(0, 1), // Flip Y coordinate for proper display
                     ImVec2(1, 0));

        // Update mouse state for viewport interaction
        bool imageHovered = ImGui::IsItemHovered();
        updateMouseState(imageTopLeft, imageHovered);

        // Handle entity picking
        handleEntityPicking();

        // Update gizmo for selected entity (if any)
        if (sceneHierarchyPanel && sceneHierarchyPanel->selectedEntity) {
            viewport.updateGizmo(sceneHierarchyPanel->selectedEntity,
                                 currentMousePos,
                                 mousePressed,
                                 mouseDown,
                                 mouseInViewport,
                                 currentViewportSize.x,
                                 currentViewportSize.y);
        }
    }

    if (sceneHierarchyPanel && sceneHierarchyPanel->selectedEntity) {
        renderGizmoTooltip();
    }

    ImGui::End();
}

}
