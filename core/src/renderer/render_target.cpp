#include "corvus/renderer/render_target.hpp"

namespace Corvus::Renderer {

RenderTarget::RenderTarget(GraphicsContext& ctx, const RenderTargetSpec& spec)
    : context_(&ctx), spec_(spec) {
    create();
}

void RenderTarget::create() {
    framebuffer_ = context_->createFramebuffer(spec_.width, spec_.height);

    // Create color attachments
    colorTextures_.clear();
    for (uint32_t i = 0; i < spec_.colorAttachments; ++i) {
        auto colorTex = context_->createTexture2D(spec_.width, spec_.height);
        framebuffer_.attachTexture2D(colorTex, i);
        colorTextures_.push_back(colorTex);
    }

    // Create depth attachment if requested
    if (spec_.hasDepth) {
        depthTexture_ = context_->createDepthTexture(spec_.width, spec_.height);
        framebuffer_.attachDepthTexture(depthTexture_);
    }
}

void RenderTarget::bind(CommandBuffer& cmd) const { cmd.bindFramebuffer(framebuffer_); }

void RenderTarget::unbind(CommandBuffer& cmd) const { cmd.unbindFramebuffer(); }

void RenderTarget::clear(CommandBuffer& cmd, const glm::vec4& color, bool clearDepth) const {
    cmd.clear(color.r, color.g, color.b, color.a, clearDepth);
}

const Texture2D& RenderTarget::getColorTexture(uint32_t index) const {
    return colorTextures_[index];
}

const Texture2D& RenderTarget::getDepthTexture() const { return depthTexture_; }

void RenderTarget::resize(uint32_t width, uint32_t height) {
    if (width == spec_.width && height == spec_.height) {
        return;
    }

    release();
    spec_.width  = width;
    spec_.height = height;
    create();
}

void RenderTarget::release() {
    framebuffer_.release();
    for (auto& tex : colorTextures_) {
        tex.release();
    }
    if (spec_.hasDepth) {
        depthTexture_.release();
    }
}

}
