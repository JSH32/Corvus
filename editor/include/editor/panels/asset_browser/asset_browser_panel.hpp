#pragma once

#include "IconsFontAwesome6.h"
#include "corvus/asset/asset_manager.hpp"
#include "corvus/project/project.hpp"
#include "editor/editor.hpp"
#include "editor/panels/asset_browser/asset_viewer.hpp"
#include "editor/panels/editor_panel.hpp"

#include <imgui.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Corvus::Editor {

class AssetBrowserPanel : public EditorPanel {
public:
    explicit AssetBrowserPanel(Core::AssetManager* manager, Core::Project* project);
    ~AssetBrowserPanel();

    void        onUpdate() override;
    std::string title() { return ICON_FA_FOLDER_OPEN " Asset Browser"; };

private:
    Core::AssetManager* assetManager;
    Core::Project*      project;

    // State
    std::string               currentDir;
    std::optional<Core::UUID> selectedAsset;

    // Layout constants
    static constexpr float tileW   = 110.0f;
    static constexpr float tileH   = 120.0f;
    static constexpr float iconBox = 80.0f;
    static constexpr float padding = 12.0f;

    // Popup state
    struct PopupState {
        std::string renameBuffer;
        std::string moveBuffer;
        std::string copyBuffer;
        std::string newDirBuffer;

        bool openRenamePopup    = false;
        bool renamingFolder     = false;
        bool openMovePopup      = false;
        bool openCopyPopup      = false;
        bool openDeletePopup    = false;
        bool openNewDirPopup    = false;
        bool openDeleteDirPopup = false;
    } popupState;

    // Asset type icons mapping
    const std::unordered_map<Core::AssetType, const char*> typeIcons
        = { { Core::AssetType::Scene, ICON_FA_FILM },  { Core::AssetType::Texture, ICON_FA_IMAGE },
            { Core::AssetType::Audio, ICON_FA_MUSIC }, { Core::AssetType::Shader, ICON_FA_CODE },
            { Core::AssetType::Font, ICON_FA_FONT },   { Core::AssetType::Model, ICON_FA_CUBE },
            { Core::AssetType::Unknown, ICON_FA_FILE } };

    // Asset viewers
    std::vector<std::unique_ptr<AssetViewer>> openViewers;

    // Main UI drawing methods
    void                     drawToolbar();
    void                     drawBreadcrumbPath();
    void                     drawAssetGrid();
    void                     drawDirectory(const std::string& dir);
    void                     drawAsset(const Core::AssetMetadata& meta);
    void                     handleContextMenus();
    void                     drawPopups();
    std::vector<std::string> splitPath(const std::string& path) const;

    // Navigation
    void navigateUp();

    // Tile and icon rendering
    void drawTile(ImVec2 start, bool hovered, bool selected, bool isDropTarget);
    void drawIcon(ImVec2 tileStart, const char* icon, const std::string& label);

    // Drag and drop
    void setupDirectoryDragDrop(const std::string& dir, const std::string& name);
    void setupAssetDragDrop(const Core::AssetMetadata& meta,
                            const char*                icon,
                            const std::string&         filename);
    bool handleDirectoryDropTarget(const std::string& dir);
    void handleBreadcrumbDropTarget(const std::string& targetPath);
    void handleBackgroundDropTarget();
    void handleAssetDrop(const ImGuiPayload* payload, const std::string& targetDir);
    void handleDirectoryDrop(const ImGuiPayload* payload, const std::string& targetDir);

    // Context menus
    void handleDirectoryContextMenu(const std::string& dir, const std::string& name);
    void handleAssetContextMenu(const Core::AssetMetadata& meta, const std::string& filename);
    void drawCreateAssetMenu();

    // Event handlers
    void handleAssetDoubleClick(const Core::AssetMetadata& meta);

    // Asset operations
    void        duplicateAsset(const Core::AssetMetadata& meta, const std::string& filename);
    void        createNewAsset(const std::string& typeName, Core::AssetType type);
    std::string findUniqueAssetName(const std::string& baseName) const;
    bool        assetNameExists(const std::string& nameWithoutExt) const;

    // Popup drawing
    void drawRenamePopup();
    void drawMovePopup();
    void drawCopyPopup();
    void drawDeleteAssetPopup();
    void drawNewFolderPopup();
    void drawDeleteFolderPopup();
    void drawCancelButton();

    // Popup operations
    void performRename();
    void performCopy(const Core::UUID& assetId);
    void renameFolder(const std::string& oldPath, const std::string& newName);
    void renameAsset(const Core::UUID& assetId, const std::string& newName);

    // Asset viewer management
    void updateViewers();
    void openAssetViewer(const Core::UUID& assetID, Core::AssetType type);

    // Utility helpers
    std::string ellipsizeToWidth(const std::string& text, float maxWidth) const;
    std::string extractFilename(const std::string& path) const;
    std::string extractParentPath(const std::string& path) const;
    std::string buildPath(const std::string& parent, const std::string& child) const;
    const char* getAssetIcon(Core::AssetType type) const;
};

}
