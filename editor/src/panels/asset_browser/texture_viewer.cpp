#include "editor/panels/asset_browser/texture_viewer.hpp"
#include "IconsFontAwesome6.h"
#include "corvus/log.hpp"
#include "editor/imguiutils.hpp"
#include "imgui_internal.h"
#include <format>

namespace Corvus::Editor {

TextureViewer::TextureViewer(const Core::UUID&          id,
                             Core::AssetManager*        manager,
                             Graphics::GraphicsContext& context)
    : AssetViewer(id, manager, "Texture Viewer"), context_(&context) {
    textureHandle = assetManager->loadByID<Graphics::Texture2D>(id);
    initPreview();
}

TextureViewer::~TextureViewer() { cleanupPreview(); }

void TextureViewer::initPreview() {
    if (previewInitialized)
        return;

    previewFramebuffer = context_->createFramebuffer(previewResolution, previewResolution);
    previewColorTex    = context_->createTexture2D(previewResolution, previewResolution);
    previewDepthTex    = context_->createDepthTexture(previewResolution, previewResolution);

    previewFramebuffer.attachTexture2D(previewColorTex, 0);
    previewFramebuffer.attachDepthTexture(previewDepthTex);

    previewInitialized = true;
}

void TextureViewer::cleanupPreview() {
    if (!previewInitialized)
        return;

    previewColorTex.release();
    previewDepthTex.release();
    previewFramebuffer.release();
    previewInitialized = false;
}

void TextureViewer::handleZoomAndPan() {
    ImGuiIO& io = ImGui::GetIO();

    // Zoom
    if (io.MouseWheel != 0.0f) {
        float delta = io.MouseWheel * 0.1f;
        zoom        = std::clamp(zoom + delta, 0.1f, 10.0f);
    }

    // Pan
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        isPanning    = true;
        lastMousePos = ImGui::GetMousePos();
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
        isPanning = false;

    if (isPanning && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 cur = ImGui::GetMousePos();
        panOffset.x += (cur.x - lastMousePos.x) / zoom;
        panOffset.y += (cur.y - lastMousePos.y) / zoom;
        lastMousePos = cur;
    }
}

void TextureViewer::renderTexturePreview() {
    auto tex = textureHandle.get();
    if (!tex || !previewInitialized)
        return;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float  displayW, displayH;

    if (fitToWindow) {
        float aspect = (float)tex->width / (float)tex->height;
        if (avail.x / avail.y > aspect) {
            displayH = avail.y - 20;
            displayW = displayH * aspect;
        } else {
            displayW = avail.x - 20;
            displayH = displayW / aspect;
        }
        zoom      = displayW / tex->width;
        panOffset = { 0, 0 };
    } else {
        displayW = tex->width * zoom;
        displayH = tex->height * zoom;
    }

    float offsetX = std::max(0.0f, (avail.x - displayW) * 0.5f);
    float offsetY = std::max(0.0f, (avail.y - displayH) * 0.5f);
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX + panOffset.x,
                               ImGui::GetCursorPosY() + offsetY + panOffset.y));

    ImVec2 cursorBefore = ImGui::GetCursorScreenPos();

    // Checkerboard background for alpha
    if (showAlpha) {
        ImDrawList* draw      = ImGui::GetWindowDrawList();
        int         checkSize = 16;
        for (int y = 0; y < (int)std::ceil(displayH); y += checkSize) {
            for (int x = 0; x < (int)std::ceil(displayW); x += checkSize) {
                float x1   = cursorBefore.x + x;
                float y1   = cursorBefore.y + y;
                float x2   = std::min(cursorBefore.x + displayW, x1 + checkSize);
                float y2   = std::min(cursorBefore.y + displayH, y1 + checkSize);
                bool  even = ((x / checkSize) + (y / checkSize)) % 2 == 0;
                ImU32 col  = even ? IM_COL32(200, 200, 200, 255) : IM_COL32(150, 150, 150, 255);
                draw->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), col);
            }
        }
    }

    // Render the texture
    ImGui::RenderTexture(*tex, ImVec2(displayW, displayH), true);

    // Grid overlay
    if (showGrid && zoom >= 2.0f) {
        ImDrawList* draw      = ImGui::GetWindowDrawList();
        ImU32       gridColor = IM_COL32(100, 100, 100, 100);
        for (int x = 0; x <= tex->width; x++) {
            float sx = cursorBefore.x + x * zoom;
            draw->AddLine(
                ImVec2(sx, cursorBefore.y), ImVec2(sx, cursorBefore.y + displayH), gridColor);
        }
        for (int y = 0; y <= tex->height; y++) {
            float sy = cursorBefore.y + y * zoom;
            draw->AddLine(
                ImVec2(cursorBefore.x, sy), ImVec2(cursorBefore.x + displayW, sy), gridColor);
        }
    }

    ImRect previewRect(cursorBefore, ImVec2(cursorBefore.x + displayW, cursorBefore.y + displayH));
    if (previewRect.Contains(ImGui::GetMousePos()))
        handleZoomAndPan();
}

void TextureViewer::renderTextureInfo(Graphics::Texture2D* tex) {
    if (!tex)
        return;

    ImGui::SeparatorText(ICON_FA_CIRCLE_INFO " Texture Info");
    ImGui::Spacing();

    ImGui::Text("Resolution:");
    ImGui::SameLine(120);
    ImGui::Text("%ux%u", tex->width, tex->height);

    ImGui::Text("GPU ID:");
    ImGui::SameLine(120);
    ImGui::Text("%u", tex->id);

    int bytesPerPixel = 4;
    int memKB         = (tex->width * tex->height * bytesPerPixel) / 1024;
    ImGui::Text("Approx Memory:");
    ImGui::SameLine(120);
    if (memKB > 1024)
        ImGui::Text("%.2f MB", memKB / 1024.0f);
    else
        ImGui::Text("%d KB", memKB);
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

    std::string windowTitle = "";
    if (textureHandle.isValid()) {
        auto meta   = assetManager->getMetadata(textureHandle.getID());
        windowTitle = std::format(
            "{} Texture: {}", ICON_FA_IMAGE, meta.path.substr(meta.path.find_last_of('/') + 1));
    }

    ImGui::SetNextWindowSize(ImVec2(800, 650), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(windowTitle.c_str(), &isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button(ICON_FA_ARROWS_TO_DOT " Reset")) {
            zoom        = 1.0f;
            panOffset   = { 0, 0 };
            fitToWindow = false;
        }
        ImGui::Checkbox("Fit to Window", &fitToWindow);
        ImGui::EndMenuBar();
    }

    ImGui::Columns(2, "##TextureColumns", true);
    ImGui::SetColumnWidth(0, 600);

    // --- Left column: preview ---
    {
        ImGui::BeginChild("##Preview", ImVec2(0, 0), true);
        ImGui::SeparatorText(ICON_FA_EYE " Preview");
        ImGui::Text("Zoom: %.1f%%", zoom * 100.0f);
        ImGui::SameLine();
        if (ImGui::Button("-"))
            zoom = std::clamp(zoom - 0.1f, 0.1f, 10.0f);
        ImGui::SameLine();
        if (ImGui::Button("+"))
            zoom = std::clamp(zoom + 0.1f, 0.1f, 10.0f);
        ImGui::Spacing();
        ImGui::Separator();

        renderTexturePreview();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled(ICON_FA_COMPUTER_MOUSE " Scroll to zoom, middle-drag to pan");
        ImGui::EndChild();
    }

    ImGui::NextColumn();

    // Right column: info
    {
        ImGui::BeginChild("##Info", ImVec2(0, 0), true);
        ImGui::SeparatorText(ICON_FA_SLIDERS " Display Options");
        ImGui::Checkbox("Show Alpha", &showAlpha);
        ImGui::Checkbox("Show Grid", &showGrid);
        ImGui::Spacing();
        ImGui::Separator();
        renderTextureInfo(tex.get());
        ImGui::EndChild();
    }

    ImGui::Columns(1);
    ImGui::End();
}

}
