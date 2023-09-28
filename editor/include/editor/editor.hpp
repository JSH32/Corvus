#pragma once

#include "editor/panels/editor_panel.hpp"
#include "linp/application.hpp"
#include "linp/layer.hpp"
#include "linp/scene.hpp"
#include <memory>
#include <vector>


namespace Linp::Editor {
class EditorLayer : public Core::Layer {
public:
    EditorLayer(Core::Application* application);

    void onImGuiRender() override;

private:
    std::vector<std::unique_ptr<EditorPanel>> panels;
    Core::Scene scene;
    Core::Entity selectedEntity;
    Core::Application* application;

    void startDockspace();
};
}