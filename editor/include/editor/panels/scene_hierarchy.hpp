#pragma once

#include "editor/panels/editor_panel.hpp"
#include "corvus/project/project.hpp"
#include "corvus/scene.hpp"

namespace Corvus::Editor {
class SceneHierarchyPanel final : public EditorPanel {
public:
    SceneHierarchyPanel(Core::Project& project, Core::Entity& selectedEntity)
        : project(project), selectedEntity(selectedEntity) { }

    void onUpdate() override;

    Core::Project& project;
    Core::Entity&  selectedEntity;
    bool           isFocused();

private:
    void drawEntity(Core::Entity entity) const;
    bool windowFocused;
};
}
