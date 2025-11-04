#pragma once

#include "corvus/application.hpp"
#include "corvus/layer.hpp"
#include "corvus/project/project.hpp"
#include "editor/panels/editor_panel.hpp"
#include <memory>
#include <vector>

namespace Corvus::Editor {

class EditorLayer : public Core::Layer {
public:
    EditorLayer(Core::Application* application, std::unique_ptr<Core::Project> project);

    ~EditorLayer();
    void onImGuiRender() override;
    void recreatePanels();

    template <typename T>
    T* getPanel() {
        for (auto& panelInstance : panels) {
            if (auto* panel = dynamic_cast<T*>(panelInstance.panel.get())) {
                return panel;
            }
        }
        return nullptr;
    }

    Core::Application* getApplication() { return application; }

private:
    struct PanelDefinition {
        const char*                                               displayName;
        bool                                                      visibleOnBoot;
        std::function<std::unique_ptr<EditorPanel>(EditorLayer*)> factory;
    };

    // Runtime panel instance
    struct PanelInstance {
        std::unique_ptr<EditorPanel> panel;
        bool                         visible;
    };

    // Panel registry, look in editor.cpp to add panels
    static const std::vector<PanelDefinition> PANEL_REGISTRY;

    std::vector<PanelInstance>     panels;
    std::unique_ptr<Core::Project> currentProject;
    Core::Entity                   selectedEntity;
    Core::Application*             application;

    void startDockspace();
    void renderMenuBar();

    void returnToProjectSelector();

    void openProject(const std::string& path);
    void createNewProject(const std::string& path, const std::string& name);

    char newSceneName[256]  = "New Scene";
    char saveSceneName[256] = "";
};

}
