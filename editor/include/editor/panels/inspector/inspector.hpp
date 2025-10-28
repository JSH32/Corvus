#pragma once

#include "corvus/asset/asset_manager.hpp"
#include "corvus/project/project.hpp"
#include "editor/panels/editor_panel.hpp"
#include "editor/panels/inspector/inspector_panel.hpp"
#include "editor/panels/inspector/panels/panels.hpp"
#include "editor/panels/scene_hierarchy.hpp"
#include <memory>

namespace Corvus::Editor {
class InspectorPanel final : public EditorPanel {
public:
    InspectorPanel(Core::Project&       project,
                   Core::AssetManager*  assetManager,
                   SceneHierarchyPanel* sceneHierarchy)
        : project(project), sceneHierarchy(sceneHierarchy), assetManager(assetManager) { }

    void           onUpdate() override;
    std::string    title() { return ICON_FA_CIRCLE_INFO " Inspector"; }
    Core::Project& project;

private:
    template <std::size_t I = 0>
    static void drawAllComponents(Core::Entity entity, Core::AssetManager* assetManager);

    template <typename T>
    static void drawComponentImpl(Core::Entity        entity,
                                  Core::AssetManager* assetManager,
                                  const std::string&  name,
                                  bool                removable,
                                  bool                flat);

    template <std::size_t I = 0>
    static void drawAddComponentMenu(Core::Entity entity);

    SceneHierarchyPanel* sceneHierarchy;
    Core::AssetManager*  assetManager;
};
}
