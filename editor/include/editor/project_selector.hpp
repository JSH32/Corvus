#pragma once

#include "cereal/cereal.hpp"
#include "corvus/application.hpp"
#include "corvus/layer.hpp"
#include <optional>
#include <string>
#include <vector>

namespace Corvus::Editor {

class ProjectSelector : public Core::Layer {
public:
    explicit ProjectSelector(Core::Application* app);
    ~ProjectSelector() override = default;

    void onImGuiRender() override;

    // Returns true once a project is selected
    bool hasSelectedProject() const { return selectedPath.has_value(); }

    // Get the chosen project path
    std::optional<std::string> getSelectedProjectPath() const { return selectedPath; }

private:
    struct RecentProject {
        std::string name;
        std::string path;

        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::make_nvp("name", name), cereal::make_nvp("path", path));
        }
    };

    Core::Application*         application;
    std::vector<RecentProject> recentProjects;
    std::optional<std::string> selectedPath;

    void loadRecentProjects();
    void saveRecentProjects();
    void addToRecentProjects(const std::string& name, const std::string& path);

    void renderHeader();
    void renderActionButtons();
    void renderRecentProjects();

    void handleFileDialogs();
    void handleCreateProjectDialog();
    void handleOpenProjectDialog();

    void transitionToEditor();
};

}
