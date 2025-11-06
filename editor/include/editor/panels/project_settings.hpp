#pragma once

#include "IconsFontAwesome6.h"
#include "corvus/project/project.hpp"
#include "editor/panels/editor_panel.hpp"
#include "imgui.h"
#include <string>

namespace Corvus::Editor {

class ProjectSettingsPanel final : public EditorPanel {
public:
    explicit ProjectSettingsPanel(Core::Project* project);
    ~ProjectSettingsPanel() override = default;

    void        onUpdate() override;
    std::string title() override { return ICON_FA_GEAR " Project Settings"; };

private:
    Core::Project* project;

    // Editing state
    struct EditState {
        char                           projectNameBuffer[256] = "";
        Core::AssetHandle<Core::Scene> selectedMainScene;
        bool                           initialized = false;
    } editState;

    // UI constants
    static constexpr float labelWidth   = 150.0f;
    static constexpr float inputWidth   = 300.0f;
    static constexpr auto  unsavedColor = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);
    static constexpr auto  savedColor   = ImVec4(0.3f, 0.8f, 0.4f, 1.0f);

    // UI drawing methods
    void drawProjectNameSetting();
    void drawMainSceneSetting();
    void drawSaveButton();

    // Helper methods
    void initializeEditState();
    bool hasUnsavedChanges() const;
    void saveSettings();
    void revertChanges();
};

}
