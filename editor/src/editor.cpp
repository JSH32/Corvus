#include "editor/editor.hpp"
#include "ImGuiFileDialog.h"
#include "editor/panels/asset_browser_panel.hpp"
#include "editor/panels/inspector/inspector.hpp"
#include "editor/panels/scene_hierarchy.hpp"
#include "editor/panels/scene_view/scene_view.hpp"
#include "imgui.h"
#include "linp/asset/asset_handle.hpp"
#include "linp/log.hpp"
#include <filesystem>
#include <memory>

namespace Linp::Editor {

EditorLayer::EditorLayer(Core::Application* application)
    : Core::Layer("Editor"), application(application) {

    currentProject = Core::Project::loadOrCreate("./temp_project", "Default Project");
    if (currentProject) {
        currentProject->startFileWatcher();
        recreatePanels();
    } else {
        LINP_CORE_ERROR("Failed to create temporary project");
    }
}

void EditorLayer::recreatePanels() {
    panels.clear();

    if (!currentProject || !currentProject->getCurrentScene()) {
        LINP_CORE_WARN("No project or scene available for panels");
        return;
    }

    auto sceneHierarchy = std::make_unique<SceneHierarchyPanel>(*currentProject, selectedEntity);
    panels.push_back(std::make_unique<InspectorPanel>(*currentProject, sceneHierarchy.get()));
    panels.push_back(std::make_unique<SceneViewPanel>(*currentProject, sceneHierarchy.get()));
    panels.push_back(std::move(sceneHierarchy));
    panels.push_back(std::make_unique<AssetBrowserPanel>(currentProject->getAssetManager(),
                                                         currentProject.get()));

    LINP_CORE_INFO("Recreated editor panels");
}

void EditorLayer::onImGuiRender() {
    startDockspace();
    renderMenuBar();
    ImGui::End();

    handleFileDialogs();

    // Render all panels
    for (const auto& panel : panels) {
        if (panel) {
            panel->onUpdate();
        }
    }
}

void EditorLayer::handleFileDialogs() {
    // Handle New Project Dialog
    if (ImGuiFileDialog::Instance()->Display("ChooseFolderDlg")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string folderPath  = ImGuiFileDialog::Instance()->GetCurrentPath();
            std::string projectName = std::filesystem::path(folderPath).filename().string();
            if (projectName.empty()) {
                projectName = "New Project";
            }
            createNewProject(folderPath, projectName);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Handle Open Project Dialog
    if (ImGuiFileDialog::Instance()->Display("OpenProjectDlg")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath    = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string projectPath = std::filesystem::path(filePath).parent_path().string();
            openProject(projectPath);
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void EditorLayer::renderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project...")) {
                showNewProjectDialog();
            }

            if (ImGui::MenuItem("Open Project...")) {
                showOpenProjectDialog();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (currentProject) {
                    if (currentProject->saveCurrentScene()) {
                        LINP_CORE_INFO("Scene saved successfully");
                    } else {
                        LINP_CORE_ERROR("Failed to save scene");
                    }
                }
            }

            if (ImGui::MenuItem("Set as Main Scene")) {
                if (currentProject) {
                    currentProject->setMainScene(currentProject->getCurrentSceneID());
                    LINP_CORE_INFO("Set current scene as main scene");
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit")) {
                application->stop();
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

void EditorLayer::showNewProjectDialog() {
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFolderDlg", "Choose Project Folder", nullptr);
}

void EditorLayer::showOpenProjectDialog() {
    ImGuiFileDialog::Instance()->OpenDialog("OpenProjectDlg", "Open Project", ".json");
}

void EditorLayer::openProject(const std::string& path) {
    auto project = Core::Project::load(path);
    if (project) {
        if (currentProject) {
            currentProject->stopFileWatcher();
        }
        currentProject = std::move(project);
        currentProject->startFileWatcher();
        recreatePanels();
        LINP_CORE_INFO("Opened project: {}", currentProject->getProjectName());
    } else {
        LINP_CORE_ERROR("Failed to open project at: {}", path);
    }
}

void EditorLayer::createNewProject(const std::string& path, const std::string& name) {
    auto project = Core::Project::create(path, name);
    if (project) {
        if (currentProject) {
            currentProject->stopFileWatcher();
        }
        currentProject = std::move(project);
        currentProject->startFileWatcher();
        recreatePanels();
        LINP_CORE_INFO("Created new project: {}", name);
    } else {
        LINP_CORE_ERROR("Failed to create project at: {}", path);
    }
}

}
