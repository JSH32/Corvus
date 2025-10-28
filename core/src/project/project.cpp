#include "corvus/project/project.hpp"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/log.hpp"
#include <filesystem>
#include <fstream>

namespace Corvus::Core {

template <class Archive>
void ProjectSettings::serialize(Archive& ar) {
    ar(CEREAL_NVP(projectName));

    // Serialize UUID as string
    if constexpr (Archive::is_saving::value) {
        std::string uuidStr = mainSceneID.is_nil() ? "" : boost::uuids::to_string(mainSceneID);
        ar(cereal::make_nvp("mainSceneID", uuidStr));
    } else {
        std::string uuidStr;
        ar(cereal::make_nvp("mainSceneID", uuidStr));
        if (!uuidStr.empty()) {
            mainSceneID = boost::uuids::string_generator()(uuidStr);
        }
    }

    ar(CEREAL_NVP(assetsDirectory));
}

std::unique_ptr<Project> Project::loadOrCreate(const std::string& path, const std::string& name) {
    if (exists(path)) {
        CORVUS_CORE_INFO("Project exists at {}, loading...", path);
        return load(path);
    } else {
        CORVUS_CORE_INFO("Project does not exist at {}, creating...", path);
        return create(path, name);
    }
}

std::unique_ptr<Project> Project::create(const std::string& path, const std::string& name) {
    auto project                  = std::make_unique<Project>();
    project->projectPath          = path;
    project->settings.projectName = name;

    std::filesystem::path assetPath
        = std::filesystem::path(project->projectPath) / project->settings.assetsDirectory;

    // Create project directory structure
    std::filesystem::create_directories(assetPath / "scenes");
    std::filesystem::create_directories(assetPath / "textures");
    std::filesystem::create_directories(assetPath / "models");
    std::filesystem::create_directories(assetPath / "audio");

    // Initialize asset manager pointing to project root
    project->assetManager = std::make_unique<AssetManager>(assetPath, "project");
    project->assetManager->scanAssets();

    // Create default scene
    auto sceneHandle
        = project->assetManager->createAsset<Scene>("scenes/Untitled.scene", "Untitled");
    if (!sceneHandle.isValid()) {
        CORVUS_CORE_ERROR("Failed to create default scene for new project");
        return nullptr;
    }

    project->currentSceneHandle   = sceneHandle;
    project->settings.mainSceneID = sceneHandle.getID();

    project->saveProjectSettings();

    CORVUS_CORE_INFO("Created new project: {} at {}", name, path);
    return project;
}

std::unique_ptr<Project> Project::load(const std::string& path) {
    auto project         = std::make_unique<Project>();
    project->projectPath = path;

    // Load settings
    if (!project->loadProjectSettings()) {
        CORVUS_CORE_ERROR("Failed to load project settings from: {}", path);
        return nullptr;
    }

    std::filesystem::path assetPath
        = std::filesystem::path(project->projectPath) / project->settings.assetsDirectory;

    // Initialize asset manager pointing to project root
    project->assetManager = std::make_unique<AssetManager>(assetPath, "project");
    project->assetManager->scanAssets();

    // Try to load main scene, fallback to a new one if missing
    if (!project->settings.mainSceneID.is_nil()) {
        project->currentSceneHandle
            = project->assetManager->loadByID<Scene>(project->settings.mainSceneID);

        if (!project->currentSceneHandle.isValid()) {
            CORVUS_CORE_WARN("Main scene missing, creating new one");
            project->currentSceneHandle   = project->createNewScene("Untitled");
            project->settings.mainSceneID = project->getCurrentSceneID();
            project->saveProjectSettings();
        }
    } else {
        project->currentSceneHandle = project->createNewScene("Untitled");
    }

    CORVUS_CORE_INFO("Loaded project: {} from {}", project->settings.projectName, path);
    return project;
}

bool Project::saveProjectSettings() {
    try {
        std::string   settingsPath = projectPath + "/project.json";
        std::ofstream file(settingsPath);
        if (!file.is_open()) {
            CORVUS_CORE_ERROR("Failed to open project.json for writing");
            return false;
        }

        cereal::JSONOutputArchive ar(file);
        ar(cereal::make_nvp("project", settings));

        CORVUS_CORE_INFO("Saved project settings to: {}", settingsPath);
        return true;
    } catch (const std::exception& e) {
        CORVUS_CORE_ERROR("Failed to save project settings: {}", e.what());
        return false;
    }
}

bool Project::loadProjectSettings() {
    try {
        std::string settingsPath = projectPath + "/project.json";
        if (!std::filesystem::exists(settingsPath)) {
            CORVUS_CORE_ERROR("project.json not found at: {}", settingsPath);
            return false;
        }

        std::ifstream file(settingsPath);
        if (!file.is_open()) {
            CORVUS_CORE_ERROR("Failed to open project.json for reading");
            return false;
        }

        cereal::JSONInputArchive ar(file);
        ar(cereal::make_nvp("project", settings));

        CORVUS_CORE_INFO("Loaded project settings from: {}", settingsPath);
        return true;
    } catch (const std::exception& e) {
        CORVUS_CORE_ERROR("Failed to load project settings: {}", e.what());
        return false;
    }
}

bool Project::exists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::exists(path + "/project.json");
}

bool Project::saveCurrentScene() {
    if (!currentSceneHandle.isValid()) {
        CORVUS_CORE_ERROR("No scene to save");
        return false;
    }

    try {
        currentSceneHandle.save();
        CORVUS_CORE_INFO("Saved current scene: {}", currentSceneHandle->name);
        return true;
    } catch (const std::exception& e) {
        CORVUS_CORE_ERROR("Failed to save scene: {}", e.what());
        return false;
    }
}

bool Project::loadSceneByID(const UUID& sceneID) {
    auto handle = assetManager->loadByID<Scene>(sceneID);
    if (!handle.isValid()) {
        CORVUS_CORE_ERROR("Failed to load scene by ID: {}", boost::uuids::to_string(sceneID));
        return false;
    }

    currentSceneHandle = handle;
    CORVUS_CORE_INFO("Loaded scene: {}", currentSceneHandle.get()->name);
    return true;
}

void Project::setMainScene(const UUID& sceneID) {
    settings.mainSceneID = sceneID;
    saveProjectSettings();
}

AssetHandle<Scene> Project::createNewScene(const std::string& name) {
    // Create scene in scenes/ directory
    auto handle = assetManager->createAsset<Scene>("scenes/" + name + ".scene", name);
    if (handle.isValid()) {
        currentSceneHandle   = handle;
        settings.mainSceneID = handle.getID();
        saveProjectSettings();
    }
    return handle;
}

std::vector<AssetHandle<Scene>> Project::getAllScenes() {
    return assetManager->getAllOfType<Scene>();
}

AssetHandle<Scene> Project::getCurrentScene() { return currentSceneHandle; }

UUID Project::getCurrentSceneID() const { return currentSceneHandle.getID(); }

void Project::startFileWatcher(int pollIntervalMs) {
    if (assetManager) {
        assetManager->startFileWatcher(pollIntervalMs);
    }
}

void Project::stopFileWatcher() {
    if (assetManager) {
        assetManager->stopFileWatcher();
    }
}

}
