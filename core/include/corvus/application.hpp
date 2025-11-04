#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "corvus/graphics/graphics.hpp"
#include "corvus/graphics/window.hpp"
#include "corvus/imgui/imgui_renderer.hpp"
#include "corvus/input/input_producer.hpp"
#include "corvus/layerstack.hpp"
#include "corvus/log.hpp"

#include <string_view>
#include <vector>

namespace Corvus::Core {
// Interface representing desktop system based window
class Application {
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
    LayerStack& getLayerStack();

    /**
     * @brief Stop the application loop.
     */
    void stop();

    Graphics::GraphicsContext* getGraphics() { return this->graphicsContext.get(); }

private:
    void onUpdate();

    // Load ImGui resources and setup styling for the app to use.
    void setupImgui();

    bool isRunning = false;

    LayerStack layerStack;

    std::vector<char> fontData;

    uint32_t width;
    uint32_t height;
    uint32_t debug;
    uint32_t reset;

    std::unique_ptr<Graphics::Window>          window;
    std::unique_ptr<Graphics::GraphicsContext> graphicsContext;
    std::unique_ptr<Events::InputProducer>     inputProducer;
    Im::ImGuiRenderer                          imguiRenderer;
};
}
