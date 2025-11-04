#include "corvus/application.hpp"
#include "FA6FreeSolidFontData.h"
#include "IconsFontAwesome6.h"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/graphics/opengl_context.hpp"
#include "imgui.h"
#include "physfs.h"

namespace Corvus::Core {

Application::Application(unsigned int width, unsigned int height, const std::string& title)
    : width(width), height(height) {
    PHYSFS_init(nullptr);
    PHYSFS_mount("engine.zip", nullptr, 1);

    auto windowAPI   = Graphics::WindowAPI::GLFW;
    auto graphicsAPI = Graphics::GraphicsAPI::OpenGL;

    window = Graphics::Window::create(windowAPI, graphicsAPI, width, height, title);
    if (!window) {
        CORVUS_CORE_ERROR("Failed to create window!");
        return;
    }

    graphicsContext = Graphics::GraphicsContext::create(graphicsAPI);
    if (!graphicsContext || !graphicsContext->initialize(*window)) {
        CORVUS_CORE_ERROR("Failed to initialize graphics context!");
        return;
    }

    inputProducer = std::make_unique<Events::InputProducer>(window.get());

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    setupImgui();

    imguiRenderer = Im::ImGuiRenderer();
    if (!imguiRenderer.initialize(*graphicsContext)) {
        CORVUS_CORE_ERROR("Failed to initialize ImGuiRenderer!");
    }

    inputProducer->bus.attachConsumer(imguiRenderer);
}

Application::~Application() {
    layerStack.clear();
    inputProducer.reset();

    imguiRenderer.shutdown();

    ImGui::DestroyContext();

    if (graphicsContext) {
        graphicsContext->shutdown();
        graphicsContext.reset();
    }

    window.reset();
    PHYSFS_deinit();
}

LayerStack& Application::getLayerStack() { return layerStack; }

void Application::stop() { isRunning = false; }

void Application::run() {
    isRunning = true;

    int fbw, fbh;

    while (isRunning && !window->shouldClose()) {
        window->pollEvents();
        window->getFramebufferSize(fbw, fbh);

        graphicsContext->beginFrame();
        {
            auto cmd = graphicsContext->createCommandBuffer();
            cmd.begin();
            cmd.setViewport(0, 0, fbw, fbh);
            cmd.clear(0.19f, 0.19f, 0.20f, 1.0f);
            cmd.end();
            cmd.submit();
        }

        for (Layer* layer : layerStack)
            layer->onUpdate();

        // ImGui Frame
        ImGuiIO& io                = ImGui::GetIO();
        io.DeltaTime               = (float)window->getDeltaTime();
        io.DisplaySize             = ImVec2((float)fbw, (float)fbh);
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        imguiRenderer.newFrame();

        for (Layer* layer : layerStack)
            layer->onImGuiRender();

        ImGui::Render();
        imguiRenderer.renderDrawData(ImGui::GetDrawData());

        graphicsContext->endFrame();
        window->swapBuffers();
    }
}

void Application::setupImgui() {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGuiStyle& style                            = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
    style.Colors[ImGuiCol_Separator]             = style.Colors[ImGuiCol_Border];
    style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
    style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.29f, 0.30f, 0.31f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_Tab]                   = ImVec4(0.08f, 0.08f, 0.09f, 0.83f);
    style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.33f, 0.34f, 0.36f, 0.83f);
    style.Colors[ImGuiCol_TabActive]             = ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    style.Colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    style.GrabRounding = style.FrameRounding = 2.3f;
    style.TabRounding                        = 0.f;

    ImFontConfig             fontConfig;
    static constexpr ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    fontConfig.OversampleH                = 3;
    fontConfig.OversampleV                = 3;
    fontConfig.PixelSnapH                 = true;

    // We could technically load this as a stack value and forget it, however, that may break when
    // font atlas is updated.
    fontData = StaticResourceFile::create("engine/fonts/DroidSans.ttf")->readAllBytes();
    fontConfig.FontDataOwnedByAtlas = false;

    io.FontDefault = io.Fonts->AddFontFromMemoryTTF(
        (void*)fontData.data(), (int)fontData.size(), 16.f, &fontConfig);

    fontConfig.MergeMode            = true;
    fontConfig.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryCompressedTTF((void*)fa_solid_900_compressed_data,
                                             fa_solid_900_compressed_size,
                                             14.f,
                                             &fontConfig,
                                             iconRanges);
    io.Fonts->Build();
}

} // namespace Corvus::Core
