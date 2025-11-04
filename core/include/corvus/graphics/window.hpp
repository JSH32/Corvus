#pragma once
#include "corvus/graphics/graphics.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Corvus::Graphics {

enum class WindowAPI {
    GLFW,
    SDL,
    Headless
};

class Window {
public:
    virtual ~Window() = default;

    using KeyCallback         = std::function<void(int key, int scancode, int action, int mods)>;
    using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
    using CursorPosCallback   = std::function<void(double x, double y)>;
    using ScrollCallback      = std::function<void(double xoffset, double yoffset)>;
    using ResizeCallback      = std::function<void(int width, int height)>;
    using CharCallback        = std::function<void(unsigned int codepoint)>;

    virtual void setKeyCallback(KeyCallback callback)                 = 0;
    virtual void setMouseButtonCallback(MouseButtonCallback callback) = 0;
    virtual void setCursorPosCallback(CursorPosCallback callback)     = 0;
    virtual void setScrollCallback(ScrollCallback cb)                 = 0;
    virtual void setResizeCallback(ResizeCallback cb)                 = 0;
    virtual void setCharCallback(CharCallback cb)                     = 0;

    virtual void pollEvents()        = 0;
    virtual bool shouldClose() const = 0;
    virtual void swapBuffers()       = 0;

    virtual void* getNativeHandle() const                           = 0;
    virtual void  setTitle(const std::string& title)                = 0;
    virtual void  setSize(uint32_t width, uint32_t height)          = 0;
    virtual void  getFramebufferSize(int& width, int& height) const = 0;

    virtual double getTime() const      = 0;
    virtual double getDeltaTime()       = 0;
    virtual void   makeContextCurrent() = 0;

    static std::unique_ptr<Window> create(WindowAPI          windowAPI,
                                          GraphicsAPI        graphicsAPI,
                                          uint32_t           width,
                                          uint32_t           height,
                                          const std::string& title);
};
}
