#pragma once

#include <memory>
#include <string>

#include "corvus/graphics/graphics.hpp"
#include "corvus/graphics/window.hpp"
#include "corvus/imgui/imgui_renderer.hpp"
#include "corvus/input/event.hpp"
#include "corvus/input/event_consumer.hpp"
#include "corvus/input/input_producer.hpp"
#include "corvus/layerstack.hpp"
#include <vector>

namespace Corvus::Core {
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

    Graphics::GraphicsContext* getGraphics() const { return this->graphicsContext.get(); }

private:
    // Load ImGui resources and setup styling for the app to use.
    void setupImgui();

    struct WindowCloseListener final : Events::EventConsumer<Events::InputEvent> {
        explicit WindowCloseListener(Application& app) : app(app) { }

    protected:
        void onEvent(const Events::InputEvent& e) override {
            matchEvent(
                e, [this](const Events::WindowCloseEvent&) { app.stop(); }, [](auto const&) {});
        }

    private:
        Application& app;
    };

    bool isRunning = false;

    LayerStack layerStack;

    std::vector<char> fontData;

    uint32_t width;
    uint32_t height;

    std::unique_ptr<Graphics::Window>          window;
    std::unique_ptr<Graphics::GraphicsContext> graphicsContext;
    std::unique_ptr<Events::InputProducer>     inputProducer;
    Im::ImGuiRenderer                          imguiRenderer;
    std::unique_ptr<WindowCloseListener>       closeConsumer;
};
}
