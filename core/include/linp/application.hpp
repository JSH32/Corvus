#pragma once

#include <string>

#include "linp/layerstack.hpp"
#include "linp/log.hpp"
#include <raylib-cpp.hpp>
#include <string_view>

namespace Linp::Core {
// Interface representing desktop system based window
class Application : private raylib::Window {
public:
    Application(unsigned int width, unsigned int height, const std::string& title);
    ~Application();

    /**
     * @brief Start the main loop
     */
    void run();

    /**
     * @brief Push a render layer.
     */
    void pushLayer(Layer* layer);

    /**
     * @brief Push an overlay layer.
     */
    void pushOverlay(Layer* layer);

    /**
     * @brief Stop the application loop.
     */
    void stop();

private:
    void onUpdate();

    // Load ImGui resources and setup styling for the app to use.
    void setupImgui();

    bool isRunning = false;

    LayerStack layerStack;
};
}