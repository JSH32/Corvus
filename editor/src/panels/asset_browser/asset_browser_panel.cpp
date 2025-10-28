#include "editor/panels/asset_browser/asset_browser_panel.hpp"
#include "editor/panels/asset_browser/material_viewer.hpp"
#include "editor/panels/asset_browser/texture_viewer.hpp"
#include "imgui_internal.h"
#include "linp/asset/asset_handle.hpp"
#include "linp/log.hpp"
#include "linp/scene.hpp"
#include <algorithm>
#include <format>

namespace Linp::Editor {

AssetBrowserPanel::AssetBrowserPanel(Core::AssetManager* manager, Core::Project* project)
    : assetManager(manager), project(project), renameBuffer(256, '\0'), moveBuffer(512, '\0'),
      copyBuffer(512, '\0'), newDirBuffer(256, '\0'), currentDir("") { }

AssetBrowserPanel::~AssetBrowserPanel() { openViewers.clear(); }

std::string AssetBrowserPanel::title() {
    return std::format("{} Asset Browser", ICON_FA_FOLDER_OPEN);
}

void AssetBrowserPanel::onUpdate() {
    ImGui::Begin(title().c_str());
    drawToolbar();

    float avail = ImGui::GetContentRegionAvail().x;
    int   cols  = std::max(1, int(avail / (tileW + padding)));
    ImGui::Columns(cols, nullptr, false);

    // Directories
    for (const auto& dir : assetManager->getDirectories(currentDir)) {
        drawDirectory(dir);
        ImGui::NextColumn();
    }

    // Assets
    for (const auto& meta : assetManager->getAssetsInDirectory(currentDir)) {
        drawAsset(meta);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);

    handleContextMenus();
    drawPopups();
    updateViewers();

    ImGui::End();
}

void AssetBrowserPanel::drawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 6, 6 });

    if (ImGui::Button(ICON_FA_ARROW_LEFT " Up")) {
        if (!currentDir.empty()) {
            auto slash = currentDir.find_last_of('/');
            if (slash == std::string::npos) {
                currentDir = "";
            } else {
                currentDir = currentDir.substr(0, slash);
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(currentDir.empty() ? "(root)" : currentDir.c_str());

    ImGui::PopStyleVar();
    ImGui::Separator();
}

void AssetBrowserPanel::drawDirectory(const std::string& dir) {
    std::string name      = dir;
    auto        lastSlash = name.find_last_of('/');
    if (lastSlash != std::string::npos) {
        name = name.substr(lastSlash + 1);
    }

    ImGui::PushID(dir.c_str());
    ImGui::BeginGroup();

    ImVec2 start = ImGui::GetCursorScreenPos();
    ImVec2 size { tileW, tileH };
    ImGui::InvisibleButton("##tile", size);
    bool hovered = ImGui::IsItemHovered();
    bool dbl     = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hovered;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Use ImGui theme colors
    ImVec4 frameCol = hovered ? ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered)
                              : ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    ImU32  bg       = ImGui::ColorConvertFloat4ToU32(frameCol);

    // Reduce rounding from 8.0f to 3.0f
    dl->AddRectFilled(start, { start.x + tileW, start.y + tileH }, bg, 3.0f);

    ImVec2 iconMin { start.x + (tileW - iconBox) * 0.5f, start.y + 8 };
    ImVec2 iconMax { iconMin.x + iconBox, iconMin.y + iconBox };

    // Use ChildBg color for icon background
    ImU32 iconBg = ImGui::GetColorU32(ImGuiCol_ChildBg);
    dl->AddRectFilled(iconMin, iconMax, iconBg, 3.0f);

    ImVec2 glyphSize = ImGui::CalcTextSize(ICON_FA_FOLDER);
    ImVec2 glyphPos { iconMin.x + (iconBox - glyphSize.x) * 0.5f,
                      iconMin.y + (iconBox - glyphSize.y) * 0.5f };

    // Use Text color
    ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
    dl->AddText(glyphPos, textCol, ICON_FA_FOLDER);

    std::string fit      = ellipsizeToWidth(name, tileW - 10.0f);
    ImVec2      textSize = ImGui::CalcTextSize(fit.c_str());
    ImVec2      textPos { start.x + (tileW - textSize.x) * 0.5f, iconMax.y + 6.0f };
    dl->AddText(textPos, textCol, fit.c_str());

    if (hovered) {
        // Use ButtonActive color for border
        ImU32 borderCol = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        dl->AddRect(start, { start.x + tileW, start.y + tileH }, borderCol, 3.0f, 0, 2.0f);
    }

    if (dbl) {
        currentDir = dir;
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("DirContextMenu");

    if (ImGui::BeginPopup("DirContextMenu")) {
        if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
            renameBuffer    = name;         // Just the folder name
            moveBuffer      = dir;          // Full path for reference
            renamingFolder  = true;         // Flag to indicate we're renaming a folder
            selectedAsset   = std::nullopt; // Clear asset selection
            openRenamePopup = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete Folder")) {
            moveBuffer         = dir;
            openDeleteDirPopup = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndGroup();
    ImGui::PopID();
}

void AssetBrowserPanel::drawAsset(const Core::AssetMetadata& meta) {
    const char* icon     = typeIcons.contains(meta.type) ? typeIcons.at(meta.type) : ICON_FA_FILE;
    std::string filename = meta.path.substr(meta.path.find_last_of('/') + 1);

    ImGui::PushID(boost::uuids::to_string(meta.id).c_str());
    ImGui::BeginGroup();

    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##tile", { tileW, tileH });
    bool hovered = ImGui::IsItemHovered();
    bool dbl     = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hovered;
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    ImDrawList* dl       = ImGui::GetWindowDrawList();
    bool        selected = (selectedAsset && *selectedAsset == meta.id);

    // Use ImGui theme colors
    ImU32 bg;
    if (selected) {
        bg = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    } else if (hovered) {
        bg = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
    } else {
        bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    }

    // Reduce rounding from 8.0f to 3.0f
    dl->AddRectFilled(start, { start.x + tileW, start.y + tileH }, bg, 3.0f);

    ImVec2 iconMin { start.x + (tileW - iconBox) * 0.5f, start.y + 8 };
    ImVec2 iconMax { iconMin.x + iconBox, iconMin.y + iconBox };

    // Use ChildBg color for icon background
    ImU32 iconBg = ImGui::GetColorU32(ImGuiCol_ChildBg);
    dl->AddRectFilled(iconMin, iconMax, iconBg, 3.0f);

    ImVec2 gsz = ImGui::CalcTextSize(icon);
    ImVec2 gpos { iconMin.x + (iconBox - gsz.x) * 0.5f, iconMin.y + (iconBox - gsz.y) * 0.5f };

    // Use Text color
    ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
    dl->AddText(gpos, textCol, icon);

    std::string fit = ellipsizeToWidth(filename, tileW - 10.0f);
    ImVec2      ts  = ImGui::CalcTextSize(fit.c_str());
    ImVec2      tpos { start.x + (tileW - ts.x) * 0.5f, iconMax.y + 6.0f };
    dl->AddText(tpos, textCol, fit.c_str());

    if (hovered) {
        // Use ButtonActive color for border
        ImU32 borderCol = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        dl->AddRect(start, { start.x + tileW, start.y + tileH }, borderCol, 3.0f, 0, 2.0f);
    }

    if (clicked)
        selectedAsset = meta.id;

    if (dbl) {
        handleAssetDoubleClick(meta);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("AssetContextMenu");

    if (ImGui::BeginPopup("AssetContextMenu")) {
        if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
            renameBuffer    = filename;
            selectedAsset   = meta.id;
            renamingFolder  = false; // Clear folder flag
            openRenamePopup = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem(ICON_FA_ARROW_RIGHT " Move…")) {
            moveBuffer    = meta.path;
            selectedAsset = meta.id;
            openMovePopup = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem(ICON_FA_COPY " Duplicate")) {
            std::string base    = filename.substr(0, filename.find_last_of('.'));
            std::string ext     = filename.substr(filename.find_last_of('.'));
            std::string parent  = meta.path.substr(0, meta.path.find_last_of('/'));
            std::string newPath = parent + "/" + base + " Copy" + ext;
            assetManager->copyAsset(meta.id, newPath);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem(ICON_FA_CLONE " Copy As…")) {
            copyBuffer    = meta.path;
            selectedAsset = meta.id;
            openCopyPopup = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
            selectedAsset   = meta.id;
            openDeletePopup = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndGroup();
    ImGui::PopID();
}

void AssetBrowserPanel::handleAssetDoubleClick(const Core::AssetMetadata& meta) {
    switch (meta.type) {
        case Core::AssetType::Scene:
            project->loadSceneByID(meta.id);
            break;
        case Core::AssetType::Texture:
        case Core::AssetType::Material:
            openAssetViewer(meta.id, meta.type);
            break;

        default:
            LINP_CORE_INFO(
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
            newDirBuffer.assign("");
            openNewDirPopup = true;
        }

        auto creatable = assetManager->getCreatableAssetTypes();
        if (!creatable.empty()) {
            ImGui::SeparatorText("Create");
            for (auto& [name, type] : creatable) {
                std::string label = std::format("{} New {}", ICON_FA_FILE, name);
                if (ImGui::MenuItem(label.c_str())) {
                    std::string safe = "New " + name;
                    std::string rel  = currentDir.empty() ? safe : currentDir + "/" + safe;

                    assetManager->createAssetByType(type, rel, safe);
                }
            }
        }
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::drawPopups() {
    auto cancelBtn = [] {
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 90, 0 }))
            ImGui::CloseCurrentPopup();
    };

    // Rename Popup
    if (openRenamePopup) {
        ImGui::OpenPopup("Rename");
        openRenamePopup = false;
    }
    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("New Name", renameBuffer.data(), renameBuffer.capacity() + 1);
        if (ImGui::Button("OK", { 90, 0 })) {
            std::string newName(renameBuffer.data());

            if (renamingFolder) {
                // Renaming a folder
                std::string oldPath = moveBuffer;
                std::string parent  = oldPath.find_last_of('/') != std::string::npos
                     ? oldPath.substr(0, oldPath.find_last_of('/'))
                     : "";
                std::string newPath = parent.empty() ? newName : parent + "/" + newName;

                assetManager->renameDirectory(oldPath, newPath);
                renamingFolder = false;

            } else if (selectedAsset) {
                // Renaming a file
                auto        meta    = assetManager->getMetadata(*selectedAsset);
                std::string parent  = meta.path.find_last_of('/') != std::string::npos
                     ? meta.path.substr(0, meta.path.find_last_of('/'))
                     : "";
                std::string ext     = meta.path.substr(meta.path.find_last_of('.'));
                std::string newPath = parent.empty() ? newName + ext : parent + "/" + newName + ext;

                assetManager->moveAsset(*selectedAsset, newPath);
            }
            ImGui::CloseCurrentPopup();
        }
        cancelBtn();
        ImGui::EndPopup();
    }

    // Move Popup
    if (openMovePopup) {
        ImGui::OpenPopup("Move");
        openMovePopup = false;
    }
    if (ImGui::BeginPopupModal("Move", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Destination Path", moveBuffer.data(), moveBuffer.capacity() + 1);
        if (ImGui::Button("Move", { 90, 0 })) {
            if (selectedAsset)
                assetManager->moveAsset(*selectedAsset, moveBuffer);
            ImGui::CloseCurrentPopup();
        }
        cancelBtn();
        ImGui::EndPopup();
    }

    // Copy Popup
    if (openCopyPopup) {
        ImGui::OpenPopup("Copy Asset");
        openCopyPopup = false;
    }
    if (ImGui::BeginPopupModal("Copy Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("New Path", copyBuffer.data(), copyBuffer.capacity() + 1);
        if (ImGui::Button("Copy", { 90, 0 })) {
            if (selectedAsset)
                assetManager->copyAsset(*selectedAsset, copyBuffer);
            ImGui::CloseCurrentPopup();
        }
        cancelBtn();
        ImGui::EndPopup();
    }

    // Delete Asset Popup
    if (openDeletePopup) {
        ImGui::OpenPopup("Delete Asset");
        openDeletePopup = false;
    }
    if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete this asset?");
        if (ImGui::Button("Delete", { 90, 0 })) {
            if (selectedAsset)
                assetManager->deleteAsset(*selectedAsset);
            ImGui::CloseCurrentPopup();
        }
        cancelBtn();
        ImGui::EndPopup();
    }

    // New Folder Popup
    if (openNewDirPopup) {
        ImGui::OpenPopup("New Folder");
        openNewDirPopup = false;
    }
    if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Folder Name", newDirBuffer.data(), newDirBuffer.capacity() + 1);
        if (ImGui::Button("Create", { 90, 0 })) {
            std::string full = currentDir.empty()
                ? std::string(newDirBuffer.data())
                : currentDir + "/" + std::string(newDirBuffer.data());
            assetManager->createDirectory(full);
            ImGui::CloseCurrentPopup();
        }
        cancelBtn();
        ImGui::EndPopup();
    }

    // Delete Folder Popup
    if (openDeleteDirPopup) {
        ImGui::OpenPopup("Delete Folder");
        openDeleteDirPopup = false;
    }
    if (ImGui::BeginPopupModal("Delete Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete this folder and its contents?");
        if (ImGui::Button("Delete", { 90, 0 })) {
            assetManager->deleteDirectory(moveBuffer);
            ImGui::CloseCurrentPopup();
        }
        cancelBtn();
        ImGui::EndPopup();
    }
}

std::string AssetBrowserPanel::ellipsizeToWidth(const std::string& text, float maxWidth) const {
    if (text.empty())
        return text;
    ImVec2 size = ImGui::CalcTextSize(text.c_str());
    if (size.x <= maxWidth)
        return text;
    std::string out    = text;
    const char* dots   = "...";
    ImVec2      dotsSz = ImGui::CalcTextSize(dots);
    int         lo = 0, hi = (int)out.size();
    while (lo < hi) {
        int         mid = (lo + hi) / 2;
        std::string tmp = out.substr(0, mid) + dots;
        if (ImGui::CalcTextSize(tmp.c_str()).x <= maxWidth)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo <= 1)
        return dots;
    return out.substr(0, lo - 1) + dots;
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
            return; // Already open
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
        // Add more viewer types here
        default:
            LINP_CORE_WARN("No viewer available for asset type: {}", static_cast<int>(type));
            break;
    }
}

}
