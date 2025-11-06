#include "editor/panels/scene_view/scene_view.hpp"

#include "corvus/components/transform.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/graphics/opengl_context.hpp"
#include "editor/imguiutils.hpp"
#include "imgui.h"
#include <algorithm>

namespace Corvus::Editor {

SceneViewPanel::SceneViewPanel(Core::Project&             project,
                               Graphics::GraphicsContext& ctx,
                               SceneHierarchyPanel*       hierarchy)
    : viewport(project, ctx), sceneHierarchyPanel(hierarchy), currentViewportSize({ 1.0f, 1.0f }) {
}

void SceneViewPanel::updateMouseState(const ImVec2& topLeft, bool hovered) {
    bool leftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool leftDown    = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    mouseInViewport = hovered;
    mousePressed    = leftClicked && hovered;
    mouseDown       = leftDown;

    if (hovered) {
        ImVec2 abs      = ImGui::GetMousePos();
        currentMousePos = { abs.x - topLeft.x, abs.y - topLeft.y };
    } else {
        currentMousePos = { 0, 0 };
    }
}

void SceneViewPanel::handleEntityPicking() {
    if (!mousePressed || !mouseInViewport)
        return;
    if (viewport.getGizmo().isActive() || viewport.getGizmo().isHovered())
        return;

    Core::Entity e = viewport.pickEntity(currentMousePos);
    if (sceneHierarchyPanel)
        sceneHierarchyPanel->selectedEntity = e;
}

void SceneViewPanel::renderGizmoTooltip() {
    const auto pos = ImVec2(ImGui::GetItemRectMin().x + 5.f, ImGui::GetItemRectMin().y + 5.f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    if (ImGui::Begin("GizmoOverlay",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                         | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize
                         | ImGuiWindowFlags_NoSavedSettings
                         | ImGuiWindowFlags_NoFocusOnAppearing)) {
        auto mode = viewport.getGizmo().getMode();
        struct Btn {
            const char*       icon;
            EditorGizmo::Mode mode;
            const char*       tip;
        };
        const Btn btns[] = {
            { ICON_FA_WRENCH, EditorGizmo::Mode::All, "All (Q)" },
            { ICON_FA_UP_DOWN_LEFT_RIGHT, EditorGizmo::Mode::Translate, "Move (W)" },
            { ICON_FA_ROTATE, EditorGizmo::Mode::Rotate, "Rotate (E)" },
            { ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, EditorGizmo::Mode::Scale, "Scale (R)" }
        };
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.6f, 1, 1));

        for (int i = 0; i < 4; i++) {
            bool active = mode == btns[i].mode;
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 1, 1));
            if (ImGui::Button((std::string(btns[i].icon) + "##" + std::to_string(i)).c_str()))
                viewport.getGizmo().setMode(btns[i].mode);
            if (active)
                ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", btns[i].tip);
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
    bool focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        || sceneHierarchyPanel->isFocused();

    if (!focus)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_F) && sceneHierarchyPanel->selectedEntity) {
        auto tr = sceneHierarchyPanel->selectedEntity
                      .getComponent<Core::Components::TransformComponent>();
        viewport.getCamera().focusOn(tr.position);
    }

    static const std::unordered_map<ImGuiKey, EditorGizmo::Mode> map
        = { { ImGuiKey_Q, EditorGizmo::Mode::All },
            { ImGuiKey_W, EditorGizmo::Mode::Translate },
            { ImGuiKey_E, EditorGizmo::Mode::Rotate },
            { ImGuiKey_R, EditorGizmo::Mode::Scale } };
    for (auto& [key, mode] : map)
        if (ImGui::IsKeyPressed(key)) {
            viewport.getGizmo().setMode(mode);
            break;
        }
}

void SceneViewPanel::onUpdate() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin(title().c_str())) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }
    ImGui::PopStyleVar();

    handleShortcuts();

    bool   focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    bool   hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    ImVec2 newSize = ImGui::GetContentRegionAvail();
    newSize.x      = std::max(1.f, newSize.x);
    newSize.y      = std::max(1.f, newSize.y);

    if (newSize.x != currentViewportSize.x || newSize.y != currentViewportSize.y)
        currentViewportSize = newSize;

    // Get cursor position BEFORE rendering (needed for mouse coordinate conversion)
    ImVec2 pos = ImGui::GetCursorScreenPos();

    viewport.updateCamera(ImGui::GetIO(), !viewport.getGizmo().isActive() && (focused || hovered));

    Core::Entity* selectedEntity = (sceneHierarchyPanel && sceneHierarchyPanel->selectedEntity)
        ? &sceneHierarchyPanel->selectedEntity
        : nullptr;

    if (!viewport.isValid() || currentViewportSize.x <= 0 || currentViewportSize.y <= 0) {
        // Render without mouse input if viewport is invalid
        viewport.render(currentViewportSize, selectedEntity, { 0, 0 }, false, false, false);
        ImGui::TextDisabled("Scene View unavailable.");
    } else {
        // Render the framebuffer image
        ImGui::RenderFramebuffer(
            viewport.getFramebuffer(), viewport.getColorTexture(), currentViewportSize, true);

        // Get mouse state AFTER rendering the image
        bool imgHovered = ImGui::IsItemHovered();
        updateMouseState(pos, imgHovered);

        // First render the viewport (updates gizmo hover/active state for current frame)
        viewport.render(currentViewportSize,
                        selectedEntity,
                        currentMousePos,
                        mousePressed,
                        mouseDown,
                        mouseInViewport);

        // Handle entity picking AFTER gizmo updated hover/active state to avoid background selection
        handleEntityPicking();
    }

    if (sceneHierarchyPanel && sceneHierarchyPanel->selectedEntity)
        renderGizmoTooltip();

    ImGui::End();
}

}
