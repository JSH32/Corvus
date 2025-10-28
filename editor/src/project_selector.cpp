#include "editor/project_selector.hpp"
#include "IconsFontAwesome6.h"
#include "ImGuiFileDialog.h"
#include "cereal/archives/json.hpp"
#include "corvus/log.hpp"
#include "editor/editor.hpp"
#include "imgui.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Corvus::Editor {

static constexpr const char* recentProjectsFile     = "recent_projects.json";
static constexpr const char* createProjectDialogKey = "ChooseFolderDlg";
static constexpr const char* openProjectDialogKey   = "OpenProjectDlg";
static constexpr const char* projectFileName        = "project.caw";

ProjectSelector::ProjectSelector(Core::Application* app)
    : Core::Layer("ProjectSelector"), application(app) {
    loadRecentProjects();
}

void ProjectSelector::loadRecentProjects() {
    if (!std::filesystem::exists(recentProjectsFile)) {
        return;
    }

    try {
        std::ifstream            file(recentProjectsFile);
        cereal::JSONInputArchive archive(file);
        archive(cereal::make_nvp("recentProjects", recentProjects));

        // Don't remove invalid projects - just keep them to show as greyed out
    } catch (const std::exception& e) {
        CORVUS_CORE_WARN("Failed to load recent projects: {}", e.what());
        recentProjects.clear();
    }
}

void ProjectSelector::saveRecentProjects() {
    try {
        std::ofstream             file(recentProjectsFile);
        cereal::JSONOutputArchive archive(file);
        archive(cereal::make_nvp("recentProjects", recentProjects));
    } catch (const std::exception& e) {
        CORVUS_CORE_WARN("Failed to save recent projects: {}", e.what());
    }
}

void ProjectSelector::onImGuiRender() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    constexpr ImGuiWindowFlags windowFlags
        = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
    ImGui::Begin("Project Selector", nullptr, windowFlags);

    renderHeader();

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    renderRecentProjects();

    ImGui::End();
    ImGui::PopStyleVar();

    handleFileDialogs();

    if (selectedPath.has_value()) {
        transitionToEditor();
    }
}

void ProjectSelector::renderHeader() {
    const float windowWidth  = ImGui::GetWindowWidth();
    const float contentWidth = windowWidth - 40.0f; // Account for padding

    // Left side: Icon + Title
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text(ICON_FA_CROW);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 10.0f);

    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Corvus Engine");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::EndGroup();

    float leftGroupWidth = ImGui::GetItemRectSize().x;

    // Right side: Action buttons
    const float buttonWidth  = 180.0f;
    const float spacing      = 10.0f;
    const float buttonsWidth = buttonWidth * 2 + spacing;
    const float rightStartX  = contentWidth - buttonsWidth;

    // Only show buttons on same line if there's enough space
    if (rightStartX > leftGroupWidth + 40.0f) {
        ImGui::SameLine(rightStartX);
    } else {
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
    }

    ImGui::BeginGroup();

    // Create New Project button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 0.3f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.8f, 0.3f, 1.0f));

    if (ImGui::Button(ICON_FA_PLUS "  Create New Project", ImVec2(buttonWidth, 0))) {
        ImGuiFileDialog::Instance()->OpenDialog(
            createProjectDialogKey, "Select Project Folder", nullptr);
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, spacing);

    // Open Existing Project button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.9f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 1.0f, 1.0f));

    if (ImGui::Button(ICON_FA_FOLDER_OPEN "  Open Existing Project", ImVec2(buttonWidth, 0))) {
        ImGuiFileDialog::Instance()->OpenDialog(
            openProjectDialogKey, "Select Project Folder", nullptr);
    }
    ImGui::PopStyleColor(3);

    ImGui::EndGroup();
}

void ProjectSelector::renderRecentProjects() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::Text(ICON_FA_CLOCK "  Recent Projects");
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.4f, 0.4f, 0.5f, 0.8f));
    ImGui::Separator();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (recentProjects.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 40.0f));
        const float windowWidth = ImGui::GetWindowWidth();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        const char* emptyText  = ICON_FA_INBOX "  No recent projects found";
        const float emptyWidth = ImGui::CalcTextSize(emptyText).x;
        ImGui::SetCursorPosX((windowWidth - emptyWidth) * 0.5f);
        ImGui::Text("%s", emptyText);
        ImGui::PopStyleColor();
        return;
    }

    const float listHeight = ImGui::GetContentRegionAvail().y - 10.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 0.9f));
    ImGui::BeginChild("ProjectList", ImVec2(0, listHeight), true);

    for (size_t i = 0; i < recentProjects.size(); ++i) {
        const auto& project = recentProjects[i];

        // Check if project.caw exists
        auto projectFile    = std::filesystem::path(project.path) / projectFileName;
        bool hasProjectFile = std::filesystem::exists(projectFile);

        ImGui::PushID(static_cast<int>(i));

        // Project card styling
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));

        if (hasProjectFile) {
            // Normal styling for valid projects
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.22f, 0.27f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.59f, 0.98f, 0.6f));
        } else {
            // Greyed out styling for invalid projects
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.17f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.18f, 0.20f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.18f, 0.20f, 0.6f));
        }

        const bool selected = ImGui::Selectable(("##project_" + std::to_string(i)).c_str(),
                                                false,
                                                hasProjectFile ? ImGuiSelectableFlags_None
                                                               : ImGuiSelectableFlags_Disabled,
                                                ImVec2(0, 45.0f));

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);

        // Draw content over the selectable
        ImVec2 selectableMin = ImGui::GetItemRectMin();
        ImVec2 selectableMax = ImGui::GetItemRectMax();

        ImGui::SetCursorScreenPos(ImVec2(selectableMin.x + 15.0f, selectableMin.y + 8.0f));

        // Project icon
        if (hasProjectFile) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
        }
        ImGui::Text(ICON_FA_DIAGRAM_PROJECT);
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 10.0f);

        // Project name and path
        ImGui::BeginGroup();

        if (hasProjectFile) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        }
        ImGui::Text("%s", project.name.c_str());
        ImGui::PopStyleColor();

        if (hasProjectFile) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.6f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
        }
        ImGui::Text(ICON_FA_FOLDER "  %s", project.path.c_str());
        ImGui::PopStyleColor();

        if (!hasProjectFile) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.4f, 0.4f, 1.0f));
            ImGui::Text(" " ICON_FA_TRIANGLE_EXCLAMATION " Missing");
            ImGui::PopStyleColor();
        }

        ImGui::EndGroup();

        if (selected && hasProjectFile) {
            selectedPath = project.path;
        }

        ImGui::PopID();

        if (i < recentProjects.size() - 1) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void ProjectSelector::handleFileDialogs() {
    handleCreateProjectDialog();
    handleOpenProjectDialog();
}

void ProjectSelector::handleCreateProjectDialog() {
    if (!ImGuiFileDialog::Instance()->Display(createProjectDialogKey)) {
        return;
    }

    if (ImGuiFileDialog::Instance()->IsOk()) {
        const std::string folder = ImGuiFileDialog::Instance()->GetCurrentPath();
        const std::string name   = std::filesystem::path(folder).filename().string();

        addToRecentProjects(name, folder);
        selectedPath = folder;
    }

    ImGuiFileDialog::Instance()->Close();
}

void ProjectSelector::handleOpenProjectDialog() {
    if (!ImGuiFileDialog::Instance()->Display(openProjectDialogKey)) {
        return;
    }

    if (ImGuiFileDialog::Instance()->IsOk()) {
        const std::string folder = ImGuiFileDialog::Instance()->GetCurrentPath();

        // Check if the selected folder contains project.caw
        auto projectFile = std::filesystem::path(folder) / projectFileName;
        if (!std::filesystem::exists(projectFile)) {
            CORVUS_CORE_WARN("Selected folder does not contain {}: {}", projectFileName, folder);
            ImGuiFileDialog::Instance()->Close();
            return;
        }

        const std::string name = std::filesystem::path(folder).filename().string();

        addToRecentProjects(name, folder);
        selectedPath = folder;
    }

    ImGuiFileDialog::Instance()->Close();
}

void ProjectSelector::addToRecentProjects(const std::string& name, const std::string& path) {
    // Remove existing entry if present
    recentProjects.erase(
        std::remove_if(recentProjects.begin(),
                       recentProjects.end(),
                       [&path](const auto& project) { return project.path == path; }),
        recentProjects.end());

    // Add to front of list
    recentProjects.insert(recentProjects.begin(), { name, path });

    // Optionally limit the number of recent projects
    constexpr size_t kMaxRecentProjects = 10;
    if (recentProjects.size() > kMaxRecentProjects) {
        recentProjects.resize(kMaxRecentProjects);
    }

    saveRecentProjects();
}

void ProjectSelector::transitionToEditor() {
    const std::string& path = *selectedPath;
    CORVUS_CORE_INFO("Project selected: {}", path);

    auto project
        = Core::Project::loadOrCreate(path, std::filesystem::path(path).filename().string());

    if (!project) {
        CORVUS_CORE_ERROR("Failed to load or create project at {}", path);
        selectedPath.reset();
        return;
    }

    application->getLayerStack().pushLayer(
        std::make_unique<EditorLayer>(application, std::move(project)));
    application->getLayerStack().popLayer(this);

    CORVUS_CORE_INFO("Transitioned to EditorLayer for project: {}", path);
}

}
