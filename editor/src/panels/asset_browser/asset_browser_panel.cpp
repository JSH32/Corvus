#include "editor/panels/asset_browser/asset_browser_panel.hpp"
#include "boost/uuid.hpp"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/log.hpp"
#include "corvus/scene.hpp"
#include "editor/panels/asset_browser/material_viewer.hpp"
#include "editor/panels/asset_browser/model_viewer.hpp"
#include "editor/panels/asset_browser/texture_viewer.hpp"
#include "imgui_internal.h"

#include <algorithm>
#include <format>

namespace Corvus::Editor {

AssetBrowserPanel::AssetBrowserPanel(Core::AssetManager* manager, Core::Project* project)
    : assetManager(manager), project(project), currentDir("") {
    popupState.renameBuffer.resize(256);
    popupState.moveBuffer.resize(512);
    popupState.copyBuffer.resize(512);
    popupState.newDirBuffer.resize(256);
}

AssetBrowserPanel::~AssetBrowserPanel() { openViewers.clear(); }

void AssetBrowserPanel::onUpdate() {
    ImGui::Begin(title().c_str());

    drawToolbar();
    drawAssetGrid();
    handleBackgroundDropTarget();
    handleContextMenus();
    drawPopups();
    updateViewers();

    ImGui::End();
}

void AssetBrowserPanel::drawAssetGrid() {
    float avail = ImGui::GetContentRegionAvail().x;
    int   cols  = std::max(1, static_cast<int>(avail / (tileW + padding)));
    ImGui::Columns(cols, nullptr, false);

    // Draw directories
    for (const auto& dir : assetManager->getDirectories(currentDir)) {
        drawDirectory(dir);
        ImGui::NextColumn();
    }

    // Draw assets
    for (const auto& meta : assetManager->getAssetsInDirectory(currentDir)) {
        drawAsset(meta);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
}

void AssetBrowserPanel::drawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

    if (ImGui::Button(ICON_FA_ARROW_LEFT " Up")) {
        navigateUp();
    }

    ImGui::SameLine();
    drawBreadcrumbPath();

    ImGui::PopStyleVar();
    ImGui::Separator();
}

std::vector<std::string> AssetBrowserPanel::splitPath(const std::string& path) const {
    if (path.empty()) {
        return {};
    }

    std::vector<std::string> segments;
    std::string              accumulated;

    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/') {
            if (!accumulated.empty()) {
                segments.push_back(accumulated);
            }
        } else {
            accumulated += path[i];
        }
    }

    if (!accumulated.empty()) {
        segments.push_back(path);
    }

    return segments;
}

void AssetBrowserPanel::drawBreadcrumbPath() {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));

    // Root segment
    bool isRoot = currentDir.empty();
    if (ImGui::Button("(root)##breadcrumb_root")) {
        currentDir = "";
    }
    handleBreadcrumbDropTarget("");

    if (isRoot) {
        ImGui::PopStyleColor(3);
        return;
    }

    // Build path segments
    std::vector<std::string> segments = splitPath(currentDir);

    // Draw each segment
    for (size_t i = 0; i < segments.size(); ++i) {
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();

        std::string segmentName = extractFilename(segments[i]);
        std::string buttonLabel = segmentName + "##breadcrumb_" + std::to_string(i);

        // Highlight current directory
        bool isCurrent = (i == segments.size() - 1);
        if (isCurrent) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        }

        if (ImGui::Button(buttonLabel.c_str())) {
            currentDir = segments[i];
        }

        if (isCurrent) {
            ImGui::PopStyleColor();
        }

        handleBreadcrumbDropTarget(segments[i]);
    }

    ImGui::PopStyleColor(3);
}

void AssetBrowserPanel::handleBreadcrumbDropTarget(const std::string& targetPath) {
    if (!ImGui::BeginDragDropTarget()) {
        return;
    }

    // Accept asset drops
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ASSET")) {
        handleAssetDrop(payload, targetPath);
    }

    // Accept directory drops
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_DIR")) {
        handleDirectoryDrop(payload, targetPath);
    }

    ImGui::EndDragDropTarget();
}

void AssetBrowserPanel::navigateUp() {
    if (!currentDir.empty()) {
        auto slash = currentDir.find_last_of('/');
        currentDir = (slash == std::string::npos) ? "" : currentDir.substr(0, slash);
    }
}

void AssetBrowserPanel::drawDirectory(const std::string& dir) {
    std::string name = extractFilename(dir);

    ImGui::PushID(dir.c_str());
    ImGui::BeginGroup();

    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##tile", ImVec2(tileW, tileH));

    bool hovered = ImGui::IsItemHovered();
    bool dbl     = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hovered;

    setupDirectoryDragDrop(dir, name);
    bool isDropTarget = handleDirectoryDropTarget(dir);

    drawTile(start, hovered, false, isDropTarget);
    drawIcon(start, ICON_FA_FOLDER, name);

    if (dbl) {
        currentDir = dir;
    }

    handleDirectoryContextMenu(dir, name);

    ImGui::EndGroup();
    ImGui::PopID();
}

void AssetBrowserPanel::drawAsset(const Core::AssetMetadata& meta) {
    const char* icon     = getAssetIcon(meta.type);
    std::string filename = extractFilename(meta.path);

    ImGui::PushID(boost::uuids::to_string(meta.id).c_str());
    ImGui::BeginGroup();

    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##tile", ImVec2(tileW, tileH));

    bool hovered = ImGui::IsItemHovered();
    bool dbl     = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hovered;
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    setupAssetDragDrop(meta, icon, filename);

    bool selected = (selectedAsset && *selectedAsset == meta.id);
    drawTile(start, hovered, selected, false);
    drawIcon(start, icon, filename);

    if (clicked) {
        selectedAsset = meta.id;
    }

    if (dbl) {
        handleAssetDoubleClick(meta);
    }

    handleAssetContextMenu(meta, filename);

    ImGui::EndGroup();
    ImGui::PopID();
}

void AssetBrowserPanel::drawTile(ImVec2 start, bool hovered, bool selected, bool isDropTarget) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Determine background color
    ImU32 bg;
    if (isDropTarget) {
        ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        col.w *= 0.7f;
        bg = ImGui::ColorConvertFloat4ToU32(col);
    } else if (selected) {
        bg = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    } else if (hovered) {
        bg = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
    } else {
        bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    }

    dl->AddRectFilled(start, ImVec2(start.x + tileW, start.y + tileH), bg, 3.0f);

    // Draw border if hovered or drop target
    if (hovered || isDropTarget) {
        ImU32 borderCol = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        dl->AddRect(start, ImVec2(start.x + tileW, start.y + tileH), borderCol, 3.0f, 0, 2.0f);
    }
}

void AssetBrowserPanel::drawIcon(ImVec2 tileStart, const char* icon, const std::string& label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Icon background box
    ImVec2 iconMin(tileStart.x + (tileW - iconBox) * 0.5f, tileStart.y + 8);
    ImVec2 iconMax(iconMin.x + iconBox, iconMin.y + iconBox);

    ImU32 iconBg = ImGui::GetColorU32(ImGuiCol_ChildBg);
    dl->AddRectFilled(iconMin, iconMax, iconBg, 3.0f);

    // Icon glyph
    ImVec2 glyphSize = ImGui::CalcTextSize(icon);
    ImVec2 glyphPos(iconMin.x + (iconBox - glyphSize.x) * 0.5f,
                    iconMin.y + (iconBox - glyphSize.y) * 0.5f);

    ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
    dl->AddText(glyphPos, textCol, icon);

    // Label text
    std::string fit      = ellipsizeToWidth(label, tileW - 10.0f);
    ImVec2      textSize = ImGui::CalcTextSize(fit.c_str());
    ImVec2      textPos(tileStart.x + (tileW - textSize.x) * 0.5f, iconMax.y + 6.0f);
    dl->AddText(textPos, textCol, fit.c_str());
}

void AssetBrowserPanel::setupDirectoryDragDrop(const std::string& dir, const std::string& name) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("ASSET_BROWSER_DIR", dir.c_str(), dir.size() + 1);
        ImGui::Text(ICON_FA_FOLDER " %s", name.c_str());
        ImGui::EndDragDropSource();
    }
}

void AssetBrowserPanel::setupAssetDragDrop(const Core::AssetMetadata& meta,
                                           const char*                icon,
                                           const std::string&         filename) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        std::string idStr = boost::uuids::to_string(meta.id);
        ImGui::SetDragDropPayload("ASSET_BROWSER_ASSET", idStr.c_str(), idStr.size() + 1);
        ImGui::Text("%s %s", icon, filename.c_str());
        ImGui::EndDragDropSource();
    }
}

bool AssetBrowserPanel::handleDirectoryDropTarget(const std::string& dir) {
    if (!ImGui::BeginDragDropTarget()) {
        return false;
    }

    // Accept asset drops
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ASSET")) {
        handleAssetDrop(payload, dir);
    }

    // Accept directory drops
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_DIR")) {
        handleDirectoryDrop(payload, dir);
    }

    ImGui::EndDragDropTarget();
    return true;
}

void AssetBrowserPanel::handleBackgroundDropTarget() {
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

    ImGui::SetCursorPos(contentMin);
    ImGui::InvisibleButton("##background_drop_target",
                           ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y));

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ASSET")) {
            handleAssetDrop(payload, currentDir);
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_DIR")) {
            handleDirectoryDrop(payload, currentDir);
        }

        ImGui::EndDragDropTarget();
    }
}

void AssetBrowserPanel::handleAssetDrop(const ImGuiPayload* payload, const std::string& targetDir) {
    std::string assetIdStr(static_cast<const char*>(payload->Data));

    try {
        Core::UUID  assetId  = boost::uuids::string_generator()(assetIdStr);
        auto        meta     = assetManager->getMetadata(assetId);
        std::string filename = extractFilename(meta.path);
        std::string newPath  = buildPath(targetDir, filename);

        if (meta.path != newPath) {
            assetManager->moveAsset(assetId, newPath);
        }
    } catch (const std::exception& e) {
        CORVUS_CORE_ERROR("Failed to move asset: {}", e.what());
    }
}

void AssetBrowserPanel::handleDirectoryDrop(const ImGuiPayload* payload,
                                            const std::string&  targetDir) {
    std::string srcDir(static_cast<const char*>(payload->Data));

    // Prevent moving into itself or its children
    if (srcDir == targetDir || targetDir.find(srcDir + "/") == 0) {
        return;
    }

    std::string folderName = extractFilename(srcDir);
    std::string newPath    = buildPath(targetDir, folderName);

    if (srcDir != newPath) {
        try {
            assetManager->renameDirectory(srcDir, newPath);
        } catch (const std::exception& e) {
            CORVUS_CORE_ERROR("Failed to move directory: {}", e.what());
        }
    }
}

void AssetBrowserPanel::handleDirectoryContextMenu(const std::string& dir,
                                                   const std::string& name) {
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("DirContextMenu");
    }

    if (ImGui::BeginPopup("DirContextMenu")) {
        if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
            popupState.renameBuffer    = name;
            popupState.moveBuffer      = dir;
            popupState.renamingFolder  = true;
            selectedAsset              = std::nullopt;
            popupState.openRenamePopup = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem(ICON_FA_TRASH " Delete Folder")) {
            popupState.moveBuffer         = dir;
            popupState.openDeleteDirPopup = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::handleAssetContextMenu(const Core::AssetMetadata& meta,
                                               const std::string&         filename) {
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("AssetContextMenu");
    }

    if (ImGui::BeginPopup("AssetContextMenu")) {
        if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
            popupState.renameBuffer    = filename;
            selectedAsset              = meta.id;
            popupState.renamingFolder  = false;
            popupState.openRenamePopup = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem(ICON_FA_ARROW_RIGHT " Move…")) {
            popupState.moveBuffer    = meta.path;
            selectedAsset            = meta.id;
            popupState.openMovePopup = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem(ICON_FA_COPY " Duplicate")) {
            duplicateAsset(meta, filename);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem(ICON_FA_CLONE " Copy As…")) {
            popupState.copyBuffer    = meta.path;
            selectedAsset            = meta.id;
            popupState.openCopyPopup = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
            selectedAsset              = meta.id;
            popupState.openDeletePopup = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::duplicateAsset(const Core::AssetMetadata& meta,
                                       const std::string&         filename) {
    size_t      dotPos  = filename.find_last_of('.');
    std::string base    = filename.substr(0, dotPos);
    std::string ext     = filename.substr(dotPos);
    std::string parent  = extractParentPath(meta.path);
    std::string newPath = buildPath(parent, base + " Copy" + ext);

    assetManager->copyAsset(meta.id, newPath);
}

void AssetBrowserPanel::handleAssetDoubleClick(const Core::AssetMetadata& meta) {
    switch (meta.type) {
        case Core::AssetType::Scene:
            project->loadSceneByID(meta.id);
            break;

        case Core::AssetType::Texture:
        case Core::AssetType::Material:
        case Core::AssetType::Model:
            openAssetViewer(meta.id, meta.type);
            break;

        default:
            CORVUS_CORE_INFO(
                "Double-clicked asset: {} (type: {})", meta.path, static_cast<int>(meta.type));
            selectedAsset = meta.id;
            break;
    }
}

void AssetBrowserPanel::handleContextMenus() {
    if (ImGui::BeginPopupContextWindow("##rootctx",
                                       ImGuiPopupFlags_MouseButtonRight
                                           | ImGuiPopupFlags_NoOpenOverExistingPopup)) {
        if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS " New Folder")) {
            popupState.newDirBuffer.clear();
            popupState.openNewDirPopup = true;
        }

        drawCreateAssetMenu();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::drawCreateAssetMenu() {
    auto creatable = assetManager->getCreatableAssetTypes();
    if (creatable.empty()) {
        return;
    }

    ImGui::SeparatorText("Create");

    for (const auto& [name, type] : creatable) {
        std::string label = std::format("{} New {}", ICON_FA_FILE, name);

        if (ImGui::MenuItem(label.c_str())) {
            createNewAsset(name, type);
        }
    }
}

void AssetBrowserPanel::createNewAsset(const std::string& typeName, Core::AssetType type) {
    std::string baseName     = "New " + typeName;
    std::string uniqueName   = findUniqueAssetName(baseName);
    std::string relativePath = buildPath(currentDir, uniqueName);

    assetManager->createAssetByType(type, relativePath, uniqueName);
}

std::string AssetBrowserPanel::findUniqueAssetName(const std::string& baseName) const {
    std::string candidate = baseName;
    int         counter   = 1;

    while (assetNameExists(candidate)) {
        candidate = baseName + " " + std::to_string(counter);
        counter++;
    }

    return candidate;
}

bool AssetBrowserPanel::assetNameExists(const std::string& nameWithoutExt) const {
    for (const auto& meta : assetManager->getAssetsInDirectory(currentDir)) {
        std::string filename = extractFilename(meta.path);
        size_t      dotPos   = filename.find_last_of('.');
        std::string existingName
            = (dotPos != std::string::npos) ? filename.substr(0, dotPos) : filename;

        if (existingName == nameWithoutExt) {
            return true;
        }
    }
    return false;
}

void AssetBrowserPanel::drawPopups() {
    drawRenamePopup();
    drawMovePopup();
    drawCopyPopup();
    drawDeleteAssetPopup();
    drawNewFolderPopup();
    drawDeleteFolderPopup();
}

void AssetBrowserPanel::drawRenamePopup() {
    if (popupState.openRenamePopup) {
        ImGui::OpenPopup("Rename");
        popupState.openRenamePopup = false;
    }

    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText(
            "New Name", popupState.renameBuffer.data(), popupState.renameBuffer.capacity() + 1);

        if (ImGui::Button("OK", ImVec2(90, 0))) {
            performRename();
            ImGui::CloseCurrentPopup();
        }

        drawCancelButton();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::performRename() {
    std::string newName(popupState.renameBuffer.data());

    if (popupState.renamingFolder) {
        renameFolder(popupState.moveBuffer, newName);
        popupState.renamingFolder = false;
    } else if (selectedAsset) {
        renameAsset(*selectedAsset, newName);
    }
}

void AssetBrowserPanel::renameFolder(const std::string& oldPath, const std::string& newName) {
    std::string parent  = extractParentPath(oldPath);
    std::string newPath = buildPath(parent, newName);
    assetManager->renameDirectory(oldPath, newPath);
}

void AssetBrowserPanel::renameAsset(const Core::UUID& assetId, const std::string& newName) {
    auto        meta    = assetManager->getMetadata(assetId);
    std::string parent  = extractParentPath(meta.path);
    std::string ext     = meta.path.substr(meta.path.find_last_of('.'));
    std::string newPath = buildPath(parent, newName + ext);

    assetManager->moveAsset(assetId, newPath);
}

void AssetBrowserPanel::drawMovePopup() {
    if (popupState.openMovePopup) {
        ImGui::OpenPopup("Move");
        popupState.openMovePopup = false;
    }

    if (ImGui::BeginPopupModal("Move", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText(
            "Destination Path", popupState.moveBuffer.data(), popupState.moveBuffer.capacity() + 1);

        if (ImGui::Button("Move", ImVec2(90, 0))) {
            if (selectedAsset) {
                std::string destPath(popupState.moveBuffer.data());
                assetManager->moveAsset(*selectedAsset, destPath);
            }
            ImGui::CloseCurrentPopup();
        }

        drawCancelButton();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::drawCopyPopup() {
    if (popupState.openCopyPopup) {
        ImGui::OpenPopup("Copy Asset");
        popupState.openCopyPopup = false;
    }

    if (ImGui::BeginPopupModal("Copy Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText(
            "New Path", popupState.copyBuffer.data(), popupState.copyBuffer.capacity() + 1);

        if (ImGui::Button("Copy", ImVec2(90, 0))) {
            if (selectedAsset) {
                performCopy(*selectedAsset);
            }
            ImGui::CloseCurrentPopup();
        }

        drawCancelButton();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::performCopy(const Core::UUID& assetId) {
    auto        meta = assetManager->getMetadata(assetId);
    std::string ext  = meta.path.substr(meta.path.find_last_of('.'));
    std::string newPath(popupState.copyBuffer.data());

    // Add extension if missing
    if (newPath.find(ext) == std::string::npos) {
        newPath += ext;
    }

    assetManager->copyAsset(assetId, newPath);
}

void AssetBrowserPanel::drawDeleteAssetPopup() {
    if (popupState.openDeletePopup) {
        ImGui::OpenPopup("Delete Asset");
        popupState.openDeletePopup = false;
    }

    if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete this asset?");

        if (ImGui::Button("Delete", ImVec2(90, 0))) {
            if (selectedAsset) {
                assetManager->deleteAsset(*selectedAsset);
            }
            ImGui::CloseCurrentPopup();
        }

        drawCancelButton();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::drawNewFolderPopup() {
    if (popupState.openNewDirPopup) {
        ImGui::OpenPopup("New Folder");
        popupState.openNewDirPopup = false;
    }

    if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText(
            "Folder Name", popupState.newDirBuffer.data(), popupState.newDirBuffer.capacity() + 1);

        if (ImGui::Button("Create", ImVec2(90, 0))) {
            std::string folderName(popupState.newDirBuffer.data());
            std::string folderPath = buildPath(currentDir, folderName);
            assetManager->createDirectory(folderPath);
            ImGui::CloseCurrentPopup();
        }

        drawCancelButton();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::drawDeleteFolderPopup() {
    if (popupState.openDeleteDirPopup) {
        ImGui::OpenPopup("Delete Folder");
        popupState.openDeleteDirPopup = false;
    }

    if (ImGui::BeginPopupModal("Delete Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete this folder and its contents?");

        if (ImGui::Button("Delete", ImVec2(90, 0))) {
            assetManager->deleteDirectory(popupState.moveBuffer);
            ImGui::CloseCurrentPopup();
        }

        drawCancelButton();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::drawCancelButton() {
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(90, 0))) {
        ImGui::CloseCurrentPopup();
    }
}

std::string AssetBrowserPanel::ellipsizeToWidth(const std::string& text, float maxWidth) const {
    if (text.empty()) {
        return text;
    }

    ImVec2 size = ImGui::CalcTextSize(text.c_str());
    if (size.x <= maxWidth) {
        return text;
    }

    const char* dots   = "...";
    ImVec2      dotsSz = ImGui::CalcTextSize(dots);

    int lo = 0;
    int hi = static_cast<int>(text.size());

    while (lo < hi) {
        int         mid = (lo + hi) / 2;
        std::string tmp = text.substr(0, mid) + dots;

        if (ImGui::CalcTextSize(tmp.c_str()).x <= maxWidth) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (lo <= 1) {
        return dots;
    }

    return text.substr(0, lo - 1) + dots;
}

void AssetBrowserPanel::updateViewers() {
    // Render all open viewers
    for (auto& viewer : openViewers) {
        viewer->render();
    }

    // Remove closed viewers
    openViewers.erase(std::remove_if(openViewers.begin(),
                                     openViewers.end(),
                                     [](const auto& viewer) { return viewer->shouldClose(); }),
                      openViewers.end());
}

void AssetBrowserPanel::openAssetViewer(const Core::UUID& assetID, Core::AssetType type) {
    // Check if already open
    for (const auto& viewer : openViewers) {
        if (viewer->getAssetID() == assetID) {
            return;
        }
    }

    // Create appropriate viewer based on type
    switch (type) {
        case Core::AssetType::Material:
            openViewers.push_back(std::make_unique<MaterialViewer>(assetID, assetManager));
            break;

        case Core::AssetType::Texture:
            openViewers.push_back(std::make_unique<TextureViewer>(assetID, assetManager));
            break;

        case Core::AssetType::Model:
            openViewers.push_back(std::make_unique<ModelViewer>(assetID, assetManager));
            break;

        default:
            CORVUS_CORE_WARN("No viewer available for asset type: {}", static_cast<int>(type));
            break;
    }
}

// Helper utility methods
std::string AssetBrowserPanel::extractFilename(const std::string& path) const {
    size_t lastSlash = path.find_last_of('/');
    return (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
}

std::string AssetBrowserPanel::extractParentPath(const std::string& path) const {
    size_t lastSlash = path.find_last_of('/');
    return (lastSlash != std::string::npos) ? path.substr(0, lastSlash) : "";
}

std::string AssetBrowserPanel::buildPath(const std::string& parent,
                                         const std::string& child) const {
    return parent.empty() ? child : parent + "/" + child;
}

const char* AssetBrowserPanel::getAssetIcon(Core::AssetType type) const {
    auto it = typeIcons.find(type);
    return (it != typeIcons.end()) ? it->second : ICON_FA_FILE;
}

}
