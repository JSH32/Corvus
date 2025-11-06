#pragma once

#include "corvus/project/project.hpp"
#include "corvus/scene.hpp"
#include "editor/panels/editor_panel.hpp"
#include <string>

namespace Corvus::Editor {
class SceneHierarchyPanel final : public EditorPanel {
public:
    SceneHierarchyPanel(Core::Project& project, Core::Entity& selectedEntity)
        : project(project), selectedEntity(selectedEntity) { }

    void        onUpdate() override;
    std::string title() override { return ICON_FA_LIST_UL " Hierarchy"; }

    Core::Project& project;
    Core::Entity&  selectedEntity;
    bool           isFocused();

private:
    void drawEntity(Core::Entity entity) const;
    bool windowFocused = false;
};
}
