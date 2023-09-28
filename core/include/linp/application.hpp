#pragma once

#include <string>

#include "linp/layerstack.hpp"
#include "linp/log.hpp"
#include <raylib-cpp.hpp>
#include <string_view>

namespace Linp::Core {
// Interface representing desktop system based window
class Application : public raylib::Window {
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

private:
    void onUpdate();

    LayerStack layerStack;
};
}