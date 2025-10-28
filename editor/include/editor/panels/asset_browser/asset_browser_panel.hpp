#pragma once

#include "IconsFontAwesome6.h"
#include "editor/editor.hpp"
#include "editor/panels/asset_browser/asset_viewer.hpp"
#include "editor/panels/editor_panel.hpp"
#include "linp/asset/asset_manager.hpp"
#include "linp/project/project.hpp"
#include <imgui.h>
#include <optional>
#include <string>
#include <unordered_map>

namespace Linp::Editor {

class AssetBrowserPanel : public EditorPanel {
public:
    explicit AssetBrowserPanel(Core::AssetManager* manager, Core::Project* project);
    ~AssetBrowserPanel();
    void        onUpdate() override;
    std::string title();

private:
    Core::AssetManager* assetManager;
    Core::Project*      project;

    std::string               currentDir = ".";
    std::optional<Core::UUID> selectedAsset;

    void handleAssetDoubleClick(const Core::AssetMetadata& meta);

    // popups
    std::string renameBuffer; // new name (no path)
    std::string moveBuffer;   // full dest path
    std::string copyBuffer;   // full dest path for copy
    std::string newDirBuffer;

    bool openRenamePopup    = false;
    bool renamingFolder     = false;
    bool openMovePopup      = false;
    bool openCopyPopup      = false;
    bool openDeletePopup    = false;
    bool openNewDirPopup    = false;
    bool openDeleteDirPopup = false;

    // icons
    std::unordered_map<Core::AssetType, const char*> typeIcons
        = { { Core::AssetType::Scene, ICON_FA_FILM },  { Core::AssetType::Texture, ICON_FA_IMAGE },
            { Core::AssetType::Audio, ICON_FA_MUSIC }, { Core::AssetType::Shader, ICON_FA_CODE },
            { Core::AssetType::Font, ICON_FA_FONT },   { Core::AssetType::Model, ICON_FA_CUBE },
            { Core::AssetType::Unknown, ICON_FA_FILE } };

    // layout
    float tileW   = 110.0f;
    float tileH   = 120.0f;
    float iconBox = 80.0f;
    float padding = 12.0f;

    // ui helpers
    void drawToolbar();
    void drawDirectory(const std::string& dir);
    void drawAsset(const Core::AssetMetadata& meta);
    void handleContextMenus();
    void drawPopups();

    // text ellipsis helper
    std::string ellipsizeToWidth(const std::string& text, float maxWidth) const;

    // For asset viewer panels
    std::vector<std::unique_ptr<AssetViewer>> openViewers;

    void updateViewers();
    void openAssetViewer(const Core::UUID& assetID, Core::AssetType type);
};

}
