#pragma once

#include "editor/panels/editor_panel.hpp"
#include "linp/scene.hpp"

namespace Linp::Editor {
class SceneHierarchyPanel final : public EditorPanel {
public:
    SceneHierarchyPanel(Core::Scene& scene, Core::Entity& selectedEntity)
        : scene(scene), selectedEntity(selectedEntity) { }

    void onUpdate() override;

    Core::Scene&  scene;
    Core::Entity& selectedEntity;

private:
    void drawEntity(Core::Entity entity) const;
};
}
