#include "editor/panels/project_settings.hpp"
#include "IconsFontAwesome6.h"
#include "corvus/log.hpp"
#include "corvus/scene.hpp"
#include <algorithm>
#include <cstring>

namespace Corvus::Editor {

ProjectSettingsPanel::ProjectSettingsPanel(Core::Project* project) : project(project) {
    initializeEditState();
}

void ProjectSettingsPanel::initializeEditState() {
    if (!editState.initialized) {
        // Initialize project name buffer
        std::string currentName = project->getProjectName();
        strncpy(editState.projectNameBuffer,
                currentName.c_str(),
                sizeof(editState.projectNameBuffer) - 1);
        editState.projectNameBuffer[sizeof(editState.projectNameBuffer) - 1] = '\0';

        // Initialize main scene handle
        Core::UUID mainSceneID = project->getMainSceneID();
        if (!mainSceneID.is_nil()) {
            editState.selectedMainScene
                = project->getAssetManager()->loadByID<Core::Scene>(mainSceneID);
        }

        editState.initialized = true;
    }
}

void ProjectSettingsPanel::onUpdate() {
    if (ImGui::Begin(title().c_str())) {
        drawProjectNameSetting();
        ImGui::Spacing();
        drawMainSceneSetting();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        drawSaveButton();
    }
    ImGui::End();
}

void ProjectSettingsPanel::drawProjectNameSetting() {
    // Check if this field has unsaved changes
    std::string currentName = project->getProjectName();
    bool        hasChanges  = (std::string(editState.projectNameBuffer) != currentName);

    // Label
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Project Name");

    if (hasChanges) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "*");
    }

    // Help icon
    ImGui::SameLine();
    ImGui::TextDisabled(ICON_FA_CIRCLE_INFO);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("The display name of your project");
        ImGui::EndTooltip();
    }

    // Input field
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText(
        "##ProjectName", editState.projectNameBuffer, sizeof(editState.projectNameBuffer));
}

void ProjectSettingsPanel::drawMainSceneSetting() {
    // Get all scenes
    auto allScenes = project->getAllScenes();

    // Get current main scene from project
    Core::UUID currentMainSceneID = project->getMainSceneID();

    // Check if this field has unsaved changes
    bool hasChanges = (editState.selectedMainScene.getID() != currentMainSceneID);

    // Label
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Main Scene");

    if (hasChanges) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "*");
    }

    // Help icon
    ImGui::SameLine();
    ImGui::TextDisabled(ICON_FA_CIRCLE_INFO);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("The scene that loads when the project starts");
        ImGui::EndTooltip();
    }

    // Combo box
    ImGui::SetNextItemWidth(-FLT_MIN);

    std::string currentSceneName = "None";
    if (editState.selectedMainScene.isValid()) {
        auto meta = project->getAssetManager()->getMetadata(editState.selectedMainScene.getID());
        // Extract filename without extension
        std::string path      = meta.path;
        size_t      lastSlash = path.find_last_of('/');
        size_t      lastDot   = path.find_last_of('.');
        if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
            currentSceneName = path.substr(lastSlash + 1, lastDot - lastSlash - 1);
        } else if (lastSlash != std::string::npos) {
            currentSceneName = path.substr(lastSlash + 1);
        } else {
            currentSceneName = path;
        }
    }

    if (ImGui::BeginCombo("##MainScene", currentSceneName.c_str())) {
        for (const auto& sceneHandle : allScenes) {
            if (!sceneHandle.isValid())
                continue;

            auto meta = project->getAssetManager()->getMetadata(sceneHandle.getID());

            // Extract filename without extension
            std::string path = meta.path;
            std::string sceneName;
            size_t      lastSlash = path.find_last_of('/');
            size_t      lastDot   = path.find_last_of('.');
            if (lastSlash != std::string::npos && lastDot != std::string::npos
                && lastDot > lastSlash) {
                sceneName = path.substr(lastSlash + 1, lastDot - lastSlash - 1);
            } else if (lastSlash != std::string::npos) {
                sceneName = path.substr(lastSlash + 1);
            } else {
                sceneName = path;
            }

            bool isSelected = (editState.selectedMainScene.getID() == sceneHandle.getID());

            if (ImGui::Selectable(sceneName.c_str(), isSelected)) {
                editState.selectedMainScene = sceneHandle;
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void ProjectSettingsPanel::drawSaveButton() {
    bool canSave = hasUnsavedChanges();

    if (!canSave) {
        ImGui::BeginDisabled();
    }

    // Calculate button width
    float availWidth            = ImGui::GetContentRegionAvail().x;
    float minWidthForSideBySide = 300.0f;

    bool  sideBySide = availWidth >= minWidthForSideBySide;
    float buttonWidth
        = sideBySide ? (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f : availWidth;

    // Green save button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.6f, 0.15f, 1.0f));

    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save Settings", ImVec2(buttonWidth, 0))) {
        saveSettings();
    }

    ImGui::PopStyleColor(3);

    if (sideBySide) {
        ImGui::SameLine();
    }

    // Orange revert button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.5f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.4f, 0.05f, 1.0f));

    if (ImGui::Button(ICON_FA_ROTATE_LEFT " Revert Changes", ImVec2(buttonWidth, 0))) {
        revertChanges();
    }

    ImGui::PopStyleColor(3);

    if (!canSave) {
        ImGui::EndDisabled();
    }
}

bool ProjectSettingsPanel::hasUnsavedChanges() const {
    // Check project name
    if (std::string(editState.projectNameBuffer) != project->getProjectName()) {
        return true;
    }

    // Check main scene
    if (editState.selectedMainScene.getID() != project->getMainSceneID()) {
        return true;
    }

    return false;
}

void ProjectSettingsPanel::saveSettings() {
    CORVUS_CORE_INFO("Saving project settings...");

    // Update project settings in memory
    project->setProjectName(std::string(editState.projectNameBuffer));
    project->setMainScene(editState.selectedMainScene.getID());

    // Save to disk
    if (project->saveProjectSettings()) {
        CORVUS_CORE_INFO("Project settings saved successfully");
    } else {
        CORVUS_CORE_ERROR("Failed to save project settings");
    }
}

void ProjectSettingsPanel::revertChanges() {
    CORVUS_CORE_INFO("Reverting project settings changes...");

    // Reload from disk
    if (project->loadProjectSettings()) {
        // Reset edit state to match reloaded values
        std::string currentName = project->getProjectName();
        strncpy(editState.projectNameBuffer,
                currentName.c_str(),
                sizeof(editState.projectNameBuffer) - 1);
        editState.projectNameBuffer[sizeof(editState.projectNameBuffer) - 1] = '\0';

        Core::UUID mainSceneID = project->getMainSceneID();
        if (!mainSceneID.is_nil()) {
            editState.selectedMainScene
                = project->getAssetManager()->loadByID<Core::Scene>(mainSceneID);
        } else {
            editState.selectedMainScene = Core::AssetHandle<Core::Scene>();
        }

        CORVUS_CORE_INFO("Project settings reverted successfully");
    } else {
        CORVUS_CORE_ERROR("Failed to reload project settings");
    }
}

}
