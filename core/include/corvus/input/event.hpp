#pragma once

#include <variant>
namespace Corvus::Core::Events {

struct KeyEvent {
    int  key;
    int  scancode;
    int  mods;
    bool pressed; // true = press, false = release
};

struct MouseButtonEvent {
    int  button;
    int  mods;
    bool pressed; // true = press, false = release
};

struct MouseMoveEvent {
    double x;
    double y;
};

struct MouseScrollEvent {
    double xoffset;
    double yoffset;
};

struct WindowResizeEvent {
    int width;
    int height;
};

struct TextInputEvent {
    unsigned int codepoint;
};

using InputEvent = std::variant<MouseMoveEvent,
                                MouseScrollEvent,
                                MouseButtonEvent,
                                KeyEvent,
                                WindowResizeEvent,
                                TextInputEvent>;

}
