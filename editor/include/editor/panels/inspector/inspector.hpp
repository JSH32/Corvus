#pragma once

#include "editor/panels/editor_panel.hpp"
#include "editor/panels/inspector/inspector_panel.hpp"
#include "editor/panels/inspector/panels/panels.hpp"
#include "editor/panels/scene_hierarchy.hpp"
#include "linp/project/project.hpp"
#include <memory>

namespace Linp::Editor {
class InspectorPanel final : public EditorPanel {
public:
    InspectorPanel(Core::Project& project, SceneHierarchyPanel* sceneHierarchy)
        : project(project), sceneHierarchy(sceneHierarchy) { }

    void           onUpdate() override;
    Core::Project& project;

private:
    template <std::size_t I = 0>
    static void drawAllComponents(Core::Entity entity);

    template <typename T>
    static void
    drawComponentImpl(Core::Entity entity, const std::string& name, bool removable, bool flat);

    template <std::size_t I = 0>
    static void drawAddComponentMenu(Core::Entity entity);

    SceneHierarchyPanel* sceneHierarchy;
};
}
