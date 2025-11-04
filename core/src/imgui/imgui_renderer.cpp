#include "corvus/imgui/imgui_renderer.hpp"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/graphics/opengl_context.hpp"
#include "corvus/input/event.hpp"
#include "corvus/input/keycodes.hpp"
#include "corvus/log.hpp"
#include "imgui_internal.h"
#include <vector>

namespace Corvus::Core::Im {

static inline ImGuiKey translateKeyToImGuiKey(Corvus::Core::Input::Key key) {
    using namespace Corvus::Core::Input;
    switch (key) {
        case Key::Tab:
            return ImGuiKey_Tab;
        case Key::Left:
            return ImGuiKey_LeftArrow;
        case Key::Right:
            return ImGuiKey_RightArrow;
        case Key::Up:
            return ImGuiKey_UpArrow;
        case Key::Down:
            return ImGuiKey_DownArrow;
        case Key::PageUp:
            return ImGuiKey_PageUp;
        case Key::PageDown:
            return ImGuiKey_PageDown;
        case Key::Home:
            return ImGuiKey_Home;
        case Key::End:
            return ImGuiKey_End;
        case Key::Insert:
            return ImGuiKey_Insert;
        case Key::Delete:
            return ImGuiKey_Delete;
        case Key::Backspace:
            return ImGuiKey_Backspace;
        case Key::Space:
            return ImGuiKey_Space;
        case Key::Enter:
            return ImGuiKey_Enter;
        case Key::Escape:
            return ImGuiKey_Escape;
        case Key::A:
            return ImGuiKey_A;
        case Key::C:
            return ImGuiKey_C;
        case Key::V:
            return ImGuiKey_V;
        case Key::X:
            return ImGuiKey_X;
        case Key::Y:
            return ImGuiKey_Y;
        case Key::Z:
            return ImGuiKey_Z;
        case Key::F1:
            return ImGuiKey_F1;
        case Key::F2:
            return ImGuiKey_F2;
        case Key::F3:
            return ImGuiKey_F3;
        case Key::F4:
            return ImGuiKey_F4;
        case Key::F5:
            return ImGuiKey_F5;
        case Key::F6:
            return ImGuiKey_F6;
        case Key::F7:
            return ImGuiKey_F7;
        case Key::F8:
            return ImGuiKey_F8;
        case Key::F9:
            return ImGuiKey_F9;
        case Key::F10:
            return ImGuiKey_F10;
        case Key::F11:
            return ImGuiKey_F11;
        case Key::F12:
            return ImGuiKey_F12;
        default:
            return ImGuiKey_None;
    }
}

bool ImGuiRenderer::initialize(Graphics::GraphicsContext& ctx) {
    context = &ctx;

    auto vsBytes = StaticResourceFile::create("engine/shaders/imgui/imgui.vert")->readAllBytes();
    auto fsBytes = StaticResourceFile::create("engine/shaders/imgui/imgui.frag")->readAllBytes();

    std::string vsSource(vsBytes.begin(), vsBytes.end());
    std::string fsSource(fsBytes.begin(), fsBytes.end());
    shader = context->createShader(vsSource, fsSource);

    // Vertex layout
    layout.push<float>(2);   // pos
    layout.push<float>(2);   // uv
    layout.push<uint8_t>(4); // color (packed RGBA)

    // Create VAO + buffers
    vao = context->createVertexArray();
    vbo = context->createVertexBuffer(nullptr, 0);
    ibo = context->createIndexBuffer(nullptr, 0, true);
    vao.addVertexBuffer(vbo, layout);
    vao.setIndexBuffer(ibo);

    // Upload font texture
    ImGuiIO&       io     = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int            w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

    fontTexture = context->createTexture2D(w, h);
    fontTexture.setData(pixels, w * h * 4);
    io.Fonts->TexID = (ImTextureID)(uintptr_t)fontTexture.getNativeHandle();

    CORVUS_CORE_INFO("ImGui initialized (font texture: {}x{})", w, h);
    return true;
}

void ImGuiRenderer::onEvent(const Events::InputEvent& e) {
    ImGuiIO& io = ImGui::GetIO();
    matchEvent(
        e,
        [&](const Events::MouseMoveEvent& evt) { io.AddMousePosEvent((float)evt.x, (float)evt.y); },
        [&](const Events::MouseButtonEvent& evt) {
            io.AddMouseButtonEvent(evt.button, evt.pressed);
        },
        [&](const Events::MouseScrollEvent& evt) {
            io.AddMouseWheelEvent((float)evt.xoffset, (float)evt.yoffset);
        },
        [&](const Events::KeyEvent& evt) {
            ImGuiKey key = translateKeyToImGuiKey(static_cast<Core::Input::Key>(evt.key));
            if (key != ImGuiKey_None)
                io.AddKeyEvent(key, evt.pressed);

            io.AddKeyEvent(ImGuiMod_Ctrl, Input::hasModifier(evt.mods, Input::Modifier::Mod_Ctrl));
            io.AddKeyEvent(ImGuiMod_Shift,
                           Input::hasModifier(evt.mods, Input::Modifier::Mod_Shift));
            io.AddKeyEvent(ImGuiMod_Alt, Input::hasModifier(evt.mods, Input::Modifier::Mod_Alt));
            io.AddKeyEvent(ImGuiMod_Super,
                           Input::hasModifier(evt.mods, Input::Modifier::Mod_Super));
        },
        [&](const Events::WindowResizeEvent& evt) {
            io.DisplaySize = ImVec2((float)evt.width, (float)evt.height);
        },
        [&](const Events::TextInputEvent& evt) {
            char utf8[5] = { 0 };
            ImTextCharToUtf8(utf8, evt.codepoint);
            io.AddInputCharactersUTF8(utf8);
        });
}

void ImGuiRenderer::newFrame() { ImGui::NewFrame(); }

void ImGuiRenderer::renderDrawData(ImDrawData* drawData) {
    if (!drawData) {
        CORVUS_CORE_WARN("ImGui: drawData is null");
        return;
    }

    if (drawData->CmdListsCount == 0) {
        return;
    }

    // Create command buffer for all ImGui rendering
    auto cmd = context->createCommandBuffer();
    cmd.begin();

    cmd.unbindFramebuffer();

    // Setup render state
    cmd.setBlendState(true);
    cmd.setDepthTest(false);
    cmd.setCullFace(false, false);
    cmd.enableScissor(true);

    const uint32_t fbW = (uint32_t)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    const uint32_t fbH = (uint32_t)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    cmd.setViewport(0, 0, fbW, fbH);

    cmd.setShader(shader);

    // Setup orthographic projection matrix
    float L = drawData->DisplayPos.x;
    float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float T = drawData->DisplayPos.y;
    float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

    const float ortho[4][4] = {
        { 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { (R + L) / (L - R), (T + B) / (B - T), 0.0f, 1.0f },
    };
    shader.setUniform(cmd, "u_ProjectionMatrix", &ortho[0][0]);
    shader.setInt(cmd, "u_Texture", 0);

    ImVec2 clip_off   = drawData->DisplayPos;
    ImVec2 clip_scale = drawData->FramebufferScale;

    // Render all command lists
    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cl = drawData->CmdLists[n];

        // Update vertex and index buffers
        // These execute immediately (before command buffer submission)
        vbo.setData(cmd, cl->VtxBuffer.Data, cl->VtxBuffer.Size * sizeof(ImDrawVert));
        ibo.setData(cmd, cl->IdxBuffer.Data, cl->IdxBuffer.Size, true);

        // Bind VAO for this draw list
        cmd.setVertexArray(vao);

        uint32_t idxOffset = 0;

        for (int cmdI = 0; cmdI < cl->CmdBuffer.Size; cmdI++) {
            const ImDrawCmd& pcmd = cl->CmdBuffer[cmdI];

            // Handle user callbacks
            if (pcmd.UserCallback) {
                // Capture callback and data by value to ensure validity
                auto callback = pcmd.UserCallback;
                auto drawList = cl;
                auto drawCmd  = &pcmd;

                // Record callback as a command
                cmd.executeCallback(
                    [callback, drawList, drawCmd]() { callback(drawList, drawCmd); });
                continue;
            }

            // Calculate scissor rectangle
            ImVec2 clip_min((pcmd.ClipRect.x - clip_off.x) * clip_scale.x,
                            (pcmd.ClipRect.y - clip_off.y) * clip_scale.y);
            ImVec2 clip_max((pcmd.ClipRect.z - clip_off.x) * clip_scale.x,
                            (pcmd.ClipRect.w - clip_off.y) * clip_scale.y);

            // Skip if clipped
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            // Set scissor rectangle
            cmd.setScissor((uint32_t)clip_min.x,
                           (uint32_t)(fbH - clip_max.y),
                           (uint32_t)(clip_max.x - clip_min.x),
                           (uint32_t)(clip_max.y - clip_min.y));

            // Bind texture
            uintptr_t                   texHandle     = (uintptr_t)pcmd.GetTexID();
            Corvus::Graphics::Texture2D textureToBind = fontTexture;
            if (texHandle != 0) {
                textureToBind.id = static_cast<uint32_t>(texHandle);
            }

            cmd.bindTexture(0, textureToBind);

            // Draw indexed
            cmd.drawIndexed(pcmd.ElemCount, true, idxOffset);
            idxOffset += pcmd.ElemCount;
        }
    }

    // Disable scissor test
    cmd.enableScissor(false);

    // Submit all recorded commands
    cmd.end();
    cmd.submit();
}

void ImGuiRenderer::shutdown() {
    shader.release();
    vao.release();
    vbo.release();
    ibo.release();
    fontTexture.release();
    context = nullptr;
}

}
