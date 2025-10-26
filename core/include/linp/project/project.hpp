#pragma once

#include "linp/asset/asset_manager.hpp"
#include "linp/scene.hpp"
#include <cereal/archives/json.hpp>
#include <memory>
#include <string>

namespace Linp::Core {

struct ProjectSettings {
    std::string projectName = "Untitled Project";
    UUID        mainSceneID;
    std::string assetsDirectory = "assets";

    template <class Archive>
    void serialize(Archive& ar);
};

class Project {
private:
    std::string                   projectPath;
    ProjectSettings               settings;
    std::unique_ptr<AssetManager> assetManager;
    AssetHandle<Scene>            currentSceneHandle;

    bool saveProjectSettings();
    bool loadProjectSettings();

public:
    Project()  = default;
    ~Project() = default;

    // Static factory methods
    static std::unique_ptr<Project> create(const std::string& path, const std::string& name);
    static std::unique_ptr<Project> load(const std::string& path);

    // Utility to check if project exists at path
    static bool exists(const std::string& path);

    // Load if exists, otherwise create
    static std::unique_ptr<Project> loadOrCreate(const std::string& path, const std::string& name);

    // Scene management
    bool                            saveCurrentScene();
    bool                            loadSceneByID(const UUID& sceneID);
    void                            setMainScene(const UUID& sceneID);
    AssetHandle<Scene>              createNewScene(const std::string& name);
    std::vector<AssetHandle<Scene>> getAllScenes();
    AssetHandle<Scene>              getCurrentScene();
    UUID                            getCurrentSceneID() const;

    // File watching
    void startFileWatcher(int pollIntervalMs = 1000);
    void stopFileWatcher();

    // Getters
    const std::string&  getProjectPath() const { return projectPath; }
    const std::string&  getProjectName() const { return settings.projectName; }
    AssetManager*       getAssetManager() { return assetManager.get(); }
    const AssetManager* getAssetManager() const { return assetManager.get(); }
};

}
