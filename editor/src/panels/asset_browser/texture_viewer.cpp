#include "editor/panels/asset_browser/texture_viewer.hpp"
#include "IconsFontAwesome6.h"
#include "imgui_internal.h"
#include "corvus/log.hpp"
#include "rlImGui.h"
#include <format>

namespace Corvus::Editor {

TextureViewer::TextureViewer(const Core::UUID& id, Core::AssetManager* manager)
    : AssetViewer(id, manager, "Texture Viewer") {
    textureHandle = assetManager->loadByID<raylib::Texture>(id);
    if (textureHandle.isValid()) {
        auto meta   = assetManager->getMetadata(id);
        windowTitle = std::format(
            "{} Texture: {}", ICON_FA_IMAGE, meta.path.substr(meta.path.find_last_of('/') + 1));
    }
}

void TextureViewer::handleZoomAndPan() {
    ImGuiIO& io = ImGui::GetIO();

    // Mouse wheel zoom
    if (io.MouseWheel != 0.0f) {
        float zoomDelta = io.MouseWheel * 0.1f;
        zoom            = std::clamp(zoom + zoomDelta, 0.1f, 10.0f);
    }

    // Middle mouse button pan
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        isPanning       = true;
        ImVec2 mousePos = ImGui::GetMousePos();
        lastMousePos    = { mousePos.x, mousePos.y };
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
        isPanning = false;
    }

    if (isPanning && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 currentPos = ImGui::GetMousePos();
        panOffset.x += (currentPos.x - lastMousePos.x) / zoom;
        panOffset.y += (currentPos.y - lastMousePos.y) / zoom;
        lastMousePos = { currentPos.x, currentPos.y };
    }
}

void TextureViewer::renderTexturePreview() {
    auto tex = textureHandle.get();
    if (!tex)
        return;

    ImVec2 availRegion = ImGui::GetContentRegionAvail();

    // Calculate display size
    float displayWidth, displayHeight;
    if (fitToWindow) {
        float aspectRatio = (float)tex->width / (float)tex->height;
        if (availRegion.x / availRegion.y > aspectRatio) {
            displayHeight = availRegion.y - 20;
            displayWidth  = displayHeight * aspectRatio;
        } else {
            displayWidth  = availRegion.x - 20;
            displayHeight = displayWidth / aspectRatio;
        }
        zoom      = displayWidth / tex->width;
        panOffset = { 0.0f, 0.0f }; // Reset pan when fit to window
    } else {
        displayWidth  = tex->width * zoom;
        displayHeight = tex->height * zoom;
    }

    // Constrain pan offset to keep texture mostly visible
    float maxPanX = std::max(0.0f, displayWidth * 0.8f - availRegion.x * 0.5f);
    float maxPanY = std::max(0.0f, displayHeight * 0.8f - availRegion.y * 0.5f);
    panOffset.x   = std::clamp(panOffset.x, -maxPanX, maxPanX);
    panOffset.y   = std::clamp(panOffset.y, -maxPanY, maxPanY);

    // Center the preview
    float offsetX = std::max(0.0f, (availRegion.x - displayWidth) * 0.5f);
    float offsetY = std::max(0.0f, (availRegion.y - displayHeight) * 0.5f);

    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX + panOffset.x,
                               ImGui::GetCursorPosY() + offsetY + panOffset.y));

    ImVec2 cursorBefore = ImGui::GetCursorScreenPos();

    // Draw checkerboard background for alpha channel
    if (showAlpha) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2      p0       = cursorBefore;
        ImVec2      p1       = ImVec2(p0.x + displayWidth, p0.y + displayHeight);

        // Checkerboard pattern
        int checkSize = 16;
        for (int y = 0; y < (int)displayHeight; y += checkSize) {
            for (int x = 0; x < (int)displayWidth; x += checkSize) {
                bool   isEven = ((x / checkSize) + (y / checkSize)) % 2 == 0;
                ImU32  color = isEven ? IM_COL32(200, 200, 200, 255) : IM_COL32(150, 150, 150, 255);
                ImVec2 min   = ImVec2(p0.x + x, p0.y + y);
                ImVec2 max   = ImVec2(std::min(p0.x + x + checkSize, p1.x),
                                    std::min(p0.y + y + checkSize, p1.y));
                drawList->AddRectFilled(min, max, color);
            }
        }
    }

    // Draw texture
    rlImGuiImageSize(tex.get(), displayWidth, displayHeight);

    // Draw grid overlay
    if (showGrid && zoom >= 2.0f) {
        ImDrawList* drawList  = ImGui::GetWindowDrawList();
        ImVec2      p0        = cursorBefore;
        ImU32       gridColor = IM_COL32(100, 100, 100, 100);

        // Vertical lines
        for (int x = 0; x <= tex->width; x++) {
            float screenX = p0.x + x * zoom;
            drawList->AddLine(
                ImVec2(screenX, p0.y), ImVec2(screenX, p0.y + displayHeight), gridColor);
        }

        // Horizontal lines
        for (int y = 0; y <= tex->height; y++) {
            float screenY = p0.y + y * zoom;
            drawList->AddLine(
                ImVec2(p0.x, screenY), ImVec2(p0.x + displayWidth, screenY), gridColor);
        }
    }

    ImVec2 cursorAfter = ImGui::GetCursorScreenPos();
    ImRect previewRect(cursorBefore,
                       ImVec2(cursorBefore.x + displayWidth, cursorBefore.y + displayHeight));

    if (previewRect.Contains(ImGui::GetMousePos())) {
        handleZoomAndPan();
    }
}

void TextureViewer::renderTextureInfo(raylib::Texture* tex) {
    if (!tex)
        return;

    ImGui::SeparatorText(ICON_FA_CIRCLE_INFO " Texture Info");
    ImGui::Spacing();

    // Format info
    const char* formatNames[]
        = { "Unknown",      "Grayscale", "Gray+Alpha", "R5G6B5",       "RGB8",
            "R5G5B5A1",     "RGBA4",     "RGBA8",      "R32",          "R32G32B32",
            "R32G32B32A32", "R16",       "R16G16B16",  "R16G16B16A16", "DXT1 RGB",
            "DXT1 RGBA",    "DXT3 RGBA", "DXT5 RGBA",  "ETC1 RGB",     "ETC2 RGB",
            "ETC2 RGBA",    "PVRT RGB",  "PVRT RGBA",  "ASTC 4x4",     "ASTC 8x8" };

    ImGui::Text("Dimensions:");
    ImGui::SameLine(120);
    ImGui::Text("%d x %d", tex->width, tex->height);

    ImGui::Text("Mipmaps:");
    ImGui::SameLine(120);
    ImGui::Text("%d", tex->mipmaps);

    ImGui::Text("Format:");
    ImGui::SameLine(120);
    int fmt = tex->format;
    if (fmt >= 0 && fmt < IM_ARRAYSIZE(formatNames)) {
        ImGui::Text("%s", formatNames[fmt]);
    } else {
        ImGui::Text("Unknown (%d)", fmt);
    }

    ImGui::Text("GPU ID:");
    ImGui::SameLine(120);
    ImGui::Text("%u", tex->id);

    // Calculate approx memory size
    int bytesPerPixel = 4; // Assume RGBA for simplicity
    int memoryKB      = (tex->width * tex->height * bytesPerPixel) / 1024;
    ImGui::Text("Memory (approx):");
    ImGui::SameLine(120);
    if (memoryKB > 1024) {
        ImGui::Text("%.2f MB", memoryKB / 1024.0f);
    } else {
        ImGui::Text("%d KB", memoryKB);
    }
}

void TextureViewer::render() {
    if (!textureHandle.isValid()) {
        isOpen = false;
        return;
    }

    auto tex = textureHandle.get();
    if (!tex) {
        isOpen = false;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(windowTitle.c_str(), &isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Menu Bar
    if (ImGui::BeginMenuBar()) {
        // Reset view button
        if (ImGui::Button(ICON_FA_ARROWS_TO_DOT " Reset View")) {
            zoom        = 1.0f;
            panOffset   = { 0.0f, 0.0f };
            fitToWindow = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset zoom and pan to default");
        }

        // Fit to window toggle
        ImGui::Checkbox("Fit to Window", &fitToWindow);

        ImGui::EndMenuBar();
    }

    // Two-column layout
    ImGui::Columns(2, "##TextureColumns", true);
    ImGui::SetColumnWidth(0, 650);

    // Left Column: Preview
    {
        ImGui::BeginChild("##PreviewSection", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::SeparatorText(ICON_FA_EYE " Preview");
        ImGui::PopStyleVar();

        ImGui::Spacing();

        // Zoom controls
        ImGui::Text("Zoom: %.1f%%", zoom * 100.0f);
        ImGui::SameLine();

        if (ImGui::Button("-")) {
            zoom = std::clamp(zoom - 0.1f, 0.1f, 10.0f);
        }
        ImGui::SameLine();

        if (ImGui::Button("+")) {
            zoom = std::clamp(zoom + 0.1f, 0.1f, 10.0f);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Preview area with clipping
        ImGui::BeginChild("##PreviewArea", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

        renderTexturePreview();

        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Controls hint
        ImGui::TextDisabled(ICON_FA_COMPUTER_MOUSE " Scroll to zoom, middle-click to pan");

        ImGui::EndChild();
    }

    ImGui::NextColumn();

    // Right Column: Info & Options
    {
        ImGui::BeginChild("##OptionsSection", ImVec2(0, 0), true);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::SeparatorText(ICON_FA_SLIDERS " Display Options");
        ImGui::PopStyleVar();

        ImGui::Spacing();

        ImGui::Checkbox("Show Alpha Channel", &showAlpha);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show checkerboard pattern behind texture");
        }

        ImGui::Checkbox("Show Pixel Grid", &showGrid);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show pixel grid when zoomed in (2x+)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Texture information
        renderTextureInfo(tex.get());

        ImGui::EndChild();
    }

    ImGui::Columns(1);

    ImGui::End();
}

}
