#pragma once

#include "corvus/graphics/graphics.hpp"
#include "corvus/input/event.hpp"
#include "corvus/input/event_consumer.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

namespace Corvus::Core::Im {

class ImGuiRenderer : public Events::EventConsumer<Events::InputEvent> {
public:
    ImGuiRenderer()  = default;
    ~ImGuiRenderer() = default;

    bool initialize(Graphics::GraphicsContext& ctx);
    void newFrame();
    void renderDrawData(ImDrawData* drawData);
    void shutdown();

protected:
    void onEvent(const Events::InputEvent& e) override;

private:
    Graphics::GraphicsContext* context = nullptr;

    Graphics::Shader             shader;
    Graphics::VertexArray        vao;
    Graphics::VertexBuffer       vbo;
    Graphics::IndexBuffer        ibo;
    Graphics::Texture2D          fontTexture;
    Graphics::VertexBufferLayout layout;
};

}
