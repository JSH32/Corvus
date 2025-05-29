#pragma once

#include "editor/editor.hpp"
#include "editor/panels/inspector/inspector.hpp"
#include "editor/panels/scene_hierarchy.hpp"
#include "editor/panels/scene_view/scene_view.hpp"
#include "imgui.h"

namespace Linp::Editor {
EditorLayer::EditorLayer(Core::Application* application) : Core::Layer("Editor"), scene(Core::Scene("Test Scene")), application(application) {
    auto sceneHierarchy = std::make_unique<SceneHierarchyPanel>(scene, selectedEntity);
    panels.push_back(std::make_unique<InspectorPanel>(scene, sceneHierarchy.get()));
    panels.push_back(std::make_unique<SceneViewPanel>(scene, sceneHierarchy.get()));
    panels.push_back(std::move(sceneHierarchy));
}

void EditorLayer::onImGuiRender() {
    startDockspace();
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                application->stop();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End(); // End dock space after menu bar

    for (const auto& panel : panels)
        panel->onUpdate();
}

void EditorLayer::startDockspace() {
    ImGuiWindowFlags     windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport    = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar();

    ImGui::PopStyleVar(2);

    if (ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        const ImGuiID dockspaceId = ImGui::GetID("Dockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    }
}
}
