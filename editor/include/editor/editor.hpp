#pragma once

#include "editor/panels/editor_panel.hpp"
#include "corvus/application.hpp"
#include "corvus/layer.hpp"
#include "corvus/project/project.hpp"
#include <memory>
#include <vector>

namespace Corvus::Editor {

class EditorLayer : public Core::Layer {
public:
    EditorLayer(Core::Application* application);
    ~EditorLayer();
    void onImGuiRender() override;
    void recreatePanels();

private:
    std::vector<std::unique_ptr<EditorPanel>> panels;
    std::unique_ptr<Core::Project>            currentProject;
    Core::Entity                              selectedEntity;
    Core::Application*                        application;

    void startDockspace();
    void renderMenuBar();
    void handleFileDialogs();

    void showNewProjectDialog();
    void showOpenProjectDialog();
    void showLoadSceneDialog();

    void openProject(const std::string& path);
    void createNewProject(const std::string& path, const std::string& name);

    char newSceneName[256]  = "New Scene";
    char saveSceneName[256] = "";
};

}
