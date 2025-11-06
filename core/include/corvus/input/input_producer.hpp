#pragma once
#include "corvus/graphics/window.hpp"
#include "corvus/input/event.hpp"
#include "corvus/input/event_producer.hpp"
#include <GLFW/glfw3.h>
#include <array>

namespace Corvus::Core::Events {

class InputProducer {
public:
    explicit InputProducer(Graphics::Window* w) : window(w) {
        // Mouse move
        window->setCursorPosCallback([this](double x, double y) {
            const MouseMoveEvent e { x, y };
            bus.emit(e);
        });

        // Mouse buttons
        window->setMouseButtonCallback([this](int button, int action, int mods) {
            const MouseButtonEvent e { button, mods, action == 1 /* GLFW_PRESS */ };
            bus.emit(e);
        });

        // Scroll
        window->setScrollCallback([this](double xoff, double yoff) {
            const MouseScrollEvent e { xoff, yoff };
            bus.emit(e);
        });

        // Key events
        window->setKeyCallback([this](int key, int scancode, int action, int mods) {
            const KeyEvent e { key, scancode, mods, action == 1 /* GLFW_PRESS */ };
            bus.emit(e);
        });

        // Resize events
        window->setResizeCallback([this](int width, int height) {
            const WindowResizeEvent e { width, height };
            bus.emit(e);
        });

        window->setCharCallback([this](unsigned int codepoint) {
            const TextInputEvent e { codepoint };
            bus.emit(e);
        });

        // Window close
        window->setCloseCallback([this]() {
            const WindowCloseEvent e {};
            bus.emit(e);
        });
    }

    void update() const;

    EventProducer<InputEvent> bus;

private:
    Graphics::Window* window;

    double                             lastMouseX = 0.0, lastMouseY = 0.0;
    std::array<int, GLFW_KEY_LAST + 1> lastKeyState {};
    std::array<int, 3>                 lastButtonState {};
    int                                lastWidth = 0, lastHeight = 0;
};

}
