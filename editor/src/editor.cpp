#include "editor/editor.hpp"
#include "ImGuiFileDialog.h"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/log.hpp"
#include "editor/panels/asset_browser/asset_browser_panel.hpp"
#include "editor/panels/inspector/inspector.hpp"
#include "editor/panels/project_settings.hpp"
#include "editor/panels/scene_hierarchy.hpp"
#include "editor/panels/scene_view/scene_view.hpp"
#include "editor/project_selector.hpp"
#include "imgui.h"
#include <filesystem>
#include <memory>

namespace Corvus::Editor {

// Panel Registry, add new panels here!
const std::vector<EditorLayer::PanelDefinition> EditorLayer::PANEL_REGISTRY
    = { { "Scene Hierarchy",
          true,
          [](EditorLayer* editor) -> std::unique_ptr<EditorPanel> {
              return std::make_unique<SceneHierarchyPanel>(*editor->currentProject,
                                                           editor->selectedEntity);
          } },
        { "Inspector",
          true,
          [](EditorLayer* editor) -> std::unique_ptr<EditorPanel> {
              return std::make_unique<InspectorPanel>(*editor->currentProject,
                                                      editor->currentProject->getAssetManager(),
                                                      editor->getApplication()->getGraphics(),
                                                      editor->getPanel<SceneHierarchyPanel>());
          } },
        { "Scene View",
          true,
          [](EditorLayer* editor) -> std::unique_ptr<EditorPanel> {
              return std::make_unique<SceneViewPanel>(*editor->currentProject,
                                                      *editor->getApplication()->getGraphics(),
                                                      editor->getPanel<SceneHierarchyPanel>());
          } },
        { "Asset Browser",
          true,
          [](EditorLayer* editor) -> std::unique_ptr<EditorPanel> {
              return std::make_unique<AssetBrowserPanel>(editor->currentProject->getAssetManager(),
                                                         editor->currentProject.get(),
                                                         editor->getApplication()->getGraphics());
          } },
        { "Project Settings", false, [](EditorLayer* editor) -> std::unique_ptr<EditorPanel> {
             return std::make_unique<ProjectSettingsPanel>(editor->currentProject.get());
         } } };

EditorLayer::EditorLayer(Core::Application* application, std::unique_ptr<Core::Project> project)
    : Core::Layer("Editor"), application(application), currentProject(std::move(project)) {

    if (currentProject) {
        if (!currentProject->fileWatcherRunning()) {
            currentProject->startFileWatcher();
        }
        recreatePanels();
        CORVUS_CORE_INFO("Loaded project from selector: {}", currentProject->getProjectName());
    } else {
        CORVUS_CORE_ERROR("Failed to initialize project");
    }
}

EditorLayer::~EditorLayer() {
    // Destroy panels first
    panels.clear();
    currentProject.reset();
}

void EditorLayer::recreatePanels() {
    panels.clear();

    if (!currentProject || !currentProject->getCurrentScene()) {
        CORVUS_CORE_WARN("No project or scene available for panels");
        return;
    }

    // Create all panels from registry
    panels.resize(PANEL_REGISTRY.size());
    for (size_t i = 0; i < PANEL_REGISTRY.size(); ++i) {
        panels[i].panel   = PANEL_REGISTRY[i].factory(this);
        panels[i].visible = PANEL_REGISTRY[i].visibleOnBoot;
    }

    CORVUS_CORE_INFO("Recreated editor panels");
}

void EditorLayer::onImGuiRender() {
    startDockspace();
    renderMenuBar();
    ImGui::End();

    // Render panels based on visibility flags
    for (auto& panelInstance : panels) {
        if (panelInstance.panel && panelInstance.visible) {
            panelInstance.panel->onUpdate();
        }
    }
}

void EditorLayer::renderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Projects")) {
                returnToProjectSelector();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (currentProject) {
                    if (currentProject->saveCurrentScene()) {
                        CORVUS_CORE_INFO("Scene saved successfully");
                    } else {
                        CORVUS_CORE_ERROR("Failed to save scene");
                    }
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit")) {
                application->stop();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            // Dynamically generate menu items from registry
            for (size_t i = 0; i < PANEL_REGISTRY.size(); ++i) {
                ImGui::MenuItem(PANEL_REGISTRY[i].displayName, nullptr, &panels[i].visible);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void EditorLayer::startDockspace() {
    ImGuiWindowFlags     windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport    = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
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

void EditorLayer::returnToProjectSelector() {
    CORVUS_CORE_INFO("Returning to project selector");

    // Stop file watcher and clean up project
    if (currentProject)
        currentProject->stopFileWatcher();
    panels.clear();
    currentProject.reset();

    application->getLayerStack().pushLayer(std::make_unique<ProjectSelector>(application));
    application->getLayerStack().popLayer(this);
}

void EditorLayer::openProject(const std::string& path) {
    auto project = Core::Project::load(application->getGraphics(), path);
    if (project) {
        if (currentProject) {
            currentProject->stopFileWatcher();
        }
        currentProject = std::move(project);
        currentProject->startFileWatcher();
        recreatePanels();
        CORVUS_CORE_INFO("Opened project: {}", currentProject->getProjectName());
    } else {
        CORVUS_CORE_ERROR("Failed to open project at: {}", path);
    }
}

void EditorLayer::createNewProject(const std::string& path, const std::string& name) {
    auto project = Core::Project::create(application->getGraphics(), path, name);
    if (project) {
        if (currentProject) {
            currentProject->stopFileWatcher();
        }
        currentProject = std::move(project);
        currentProject->startFileWatcher();
        recreatePanels();
        CORVUS_CORE_INFO("Created new project: {}", name);
    } else {
        CORVUS_CORE_ERROR("Failed to create project at: {}", path);
    }
}

}
