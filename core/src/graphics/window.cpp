#include "corvus/graphics/glfw_window.hpp"
#include <memory>

namespace Corvus::Graphics {
std::unique_ptr<Window> Window::create(WindowAPI          api,
                                       GraphicsAPI        graphicsAPI,
                                       uint32_t           width,
                                       uint32_t           height,
                                       const std::string& title) {
    switch (api) {
        case WindowAPI::GLFW:
            return std::make_unique<Corvus::Graphics::GLFWWindow>(
                width, height, title, graphicsAPI);
        default:
            return nullptr;
    }
}
}
