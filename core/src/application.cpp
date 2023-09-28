#include "linp/application.hpp"
#include "rlImGui.h"

namespace Linp::Core {
Application::Application(unsigned int width, unsigned int height, const std::string& title)
    : raylib::Window(width, height, title) {
    rlImGuiSetup(true);
}

Application::~Application() {
    rlImGuiShutdown();
}

void Application::pushLayer(Layer* layer) {
    layerStack.pushLayer(layer);
    layer->onAttach();
}

void Application::pushOverlay(Layer* layer) {
    layerStack.pushOverlay(layer);
    layer->onAttach();
}

void Application::run() {
    SetTargetFPS(60);

    while (!this->ShouldClose()) {
        BeginDrawing();

        this->ClearBackground(RAYWHITE);

        for (Layer* layer : layerStack)
            layer->onUpdate();

        rlImGuiBegin();

        for (Layer* layer : layerStack)
            layer->onImGuiRender();

        rlImGuiEnd();

        EndDrawing();
    }
}
}