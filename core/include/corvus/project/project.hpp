#pragma once

#include "corvus/asset/asset_manager.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/scene.hpp"
#include <cereal/archives/json.hpp>
#include <memory>
#include <string>

namespace Corvus::Core {

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

public:
    Project()  = default;
    ~Project() = default;

    // Static factory methods
    static std::unique_ptr<Project>
    create(Graphics::GraphicsContext* ctx, const std::string& path, const std::string& name);
    static std::unique_ptr<Project> load(Graphics::GraphicsContext* ctx, const std::string& path);

    // Utility to check if project exists at path
    static bool exists(const std::string& path);

    // Load if exists, otherwise create
    static std::unique_ptr<Project>
    loadOrCreate(Graphics::GraphicsContext* ctx, const std::string& path, const std::string& name);

    // Scene management
    bool                            saveCurrentScene();
    bool                            loadSceneByID(const UUID& sceneID);
    void                            setMainScene(const UUID& sceneID);
    void                            setProjectName(const std::string& name);
    AssetHandle<Scene>              createNewScene(const std::string& name);
    std::vector<AssetHandle<Scene>> getAllScenes();
    AssetHandle<Scene>              getCurrentScene();
    UUID                            getCurrentSceneID() const;

    bool saveProjectSettings();
    bool loadProjectSettings();

    // File watching
    void startFileWatcher(int pollIntervalMs = 1000);
    void stopFileWatcher();
    bool fileWatcherRunning() const;

    // Getters
    const std::string&  getProjectPath() const { return projectPath; }
    const std::string&  getProjectName() const { return settings.projectName; }
    const UUID&         getMainSceneID() const { return settings.mainSceneID; }
    AssetManager*       getAssetManager() { return assetManager.get(); }
    const AssetManager* getAssetManager() const { return assetManager.get(); }
};

}
