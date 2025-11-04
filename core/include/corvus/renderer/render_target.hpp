#pragma once
#include "corvus/graphics/graphics.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace Corvus::Renderer {

using Graphics::CommandBuffer;
using Graphics::Framebuffer;
using Graphics::GraphicsContext;
using Graphics::Texture2D;

struct RenderTargetSpec {
    uint32_t width;
    uint32_t height;
    uint32_t colorAttachments = 1;
    bool     hasDepth         = true;
};

class RenderTarget {
public:
    RenderTarget(GraphicsContext& ctx, const RenderTargetSpec& spec);

    void bind(CommandBuffer& cmd) const;
    void unbind(CommandBuffer& cmd) const;
    void
    clear(CommandBuffer& cmd, const glm::vec4& color = glm::vec4(0), bool clearDepth = true) const;

    const Texture2D& getColorTexture(uint32_t index = 0) const;
    const Texture2D& getDepthTexture() const;

    uint32_t getWidth() const { return spec_.width; }
    uint32_t getHeight() const { return spec_.height; }

    void resize(uint32_t width, uint32_t height);
    void release();

private:
    void create();

    GraphicsContext*       context_;
    RenderTargetSpec       spec_;
    Framebuffer            framebuffer_;
    std::vector<Texture2D> colorTextures_;
    Texture2D              depthTexture_;
};

}
