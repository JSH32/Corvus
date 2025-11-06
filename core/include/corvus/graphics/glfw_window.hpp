#pragma once

#include "corvus/graphics/window.hpp"
#include "corvus/input/keycodes.hpp"
#include <GLFW/glfw3.h>

namespace Corvus::Graphics {

static Core::Input::Key translateGLFWKey(int glfwKey) {
    using namespace Corvus::Core::Input;
    return static_cast<Key>(glfwKey);
}

inline uint8_t translateGLFWMods(const int glfwMods) {
    using namespace Corvus::Core::Input;
    uint8_t result = 0;
    if (glfwMods & GLFW_MOD_SHIFT)
        result |= Mod_Shift;
    if (glfwMods & GLFW_MOD_CONTROL)
        result |= Mod_Ctrl;
    if (glfwMods & GLFW_MOD_ALT)
        result |= Mod_Alt;
    if (glfwMods & GLFW_MOD_SUPER)
        result |= Mod_Super;
    return result;
}

class GLFWWindow final : public Window {
public:
    GLFWWindow(const uint32_t     width,
               const uint32_t     height,
               const std::string& title,
               const GraphicsAPI  graphicsAPI) {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        if (graphicsAPI == GraphicsAPI::OpenGL) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        } else if (graphicsAPI == GraphicsAPI::Vulkan) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }

        window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(window, this);

        glfwSetKeyCallback(
            window,
            [](GLFWwindow* w, const int key, const int scancode, const int action, const int mods) {
                if (const auto self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w));
                    self && self->keyCallback) {
                    const auto keycode   = translateGLFWKey(key);
                    const auto modifiers = translateGLFWMods(mods);
                    self->keyCallback(static_cast<int>(keycode), scancode, action, modifiers);
                }
            });

        glfwSetMouseButtonCallback(
            window, [](GLFWwindow* w, const int button, const int action, const int mods) {
                if (const auto self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w));
                    self && self->mouseButtonCallback) {
                    const auto modifiers = translateGLFWMods(mods);
                    self->mouseButtonCallback(button, action, modifiers);
                }
            });

        glfwSetCursorPosCallback(window, [](GLFWwindow* w, const double x, const double y) {
            if (const auto self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w));
                self && self->cursorPosCallback)
                self->cursorPosCallback(x, y);
        });

        glfwSetScrollCallback(
            window, [](GLFWwindow* w, const double xOffset, const double yOffset) {
                if (const auto self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w));
                    self && self->scrollCallback)
                    self->scrollCallback(xOffset, yOffset);
            });

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, const int wpx, const int hpx) {
            if (const auto self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w));
                self && self->resizeCallback)
                self->resizeCallback(wpx, hpx);
        });

        glfwSetCharCallback(window, [](GLFWwindow* w, const unsigned int codepoint) {
            if (const auto self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w));
                self && self->charCallback)
                self->charCallback(codepoint);
        });

        glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
            if (const auto self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w));
                self && self->closeCallback)
                self->closeCallback();
        });
    }

    ~GLFWWindow() override {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void* getNativeHandle() const override { return window; }
    void  setTitle(const std::string& title) override { glfwSetWindowTitle(window, title.c_str()); }
    void  setSize(const uint32_t w, const uint32_t h) override { glfwSetWindowSize(window, w, h); }
    void  getFramebufferSize(int& w, int& h) const override {
        glfwGetFramebufferSize(window, &w, &h);
    }

    void pollEvents() override { glfwPollEvents(); }
    bool shouldClose() const override { return glfwWindowShouldClose(window); }
    void swapBuffers() override { glfwSwapBuffers(window); }

    void setKeyCallback(KeyCallback cb) override { keyCallback = std::move(cb); }
    void setMouseButtonCallback(MouseButtonCallback cb) override {
        mouseButtonCallback = std::move(cb);
    }
    void setCursorPosCallback(CursorPosCallback cb) override { cursorPosCallback = std::move(cb); }
    void setScrollCallback(ScrollCallback cb) override { scrollCallback = std::move(cb); }
    void setResizeCallback(ResizeCallback cb) override { resizeCallback = std::move(cb); }
    void setCharCallback(CharCallback cb) override { charCallback = std::move(cb); }
    void setCloseCallback(CloseCallback cb) override { closeCallback = std::move(cb); }

    double getTime() const override { return glfwGetTime(); }

    void makeContextCurrent() override { glfwMakeContextCurrent(window); }

    double getDeltaTime() override {
        const double current = glfwGetTime();
        const double delta   = current - lastTime;
        lastTime             = current;
        return delta > 0.0 ? delta : 1.0 / 60.0;
    }

private:
    GLFWwindow*         window = nullptr;
    KeyCallback         keyCallback;
    MouseButtonCallback mouseButtonCallback;
    CursorPosCallback   cursorPosCallback;
    ScrollCallback      scrollCallback;
    ResizeCallback      resizeCallback;
    CharCallback        charCallback;
    CloseCallback       closeCallback;
    double              lastTime = 0.0;
};

}
