#pragma once
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/camera.hpp"
#include "corvus/renderer/render_queue.hpp"
#include "corvus/renderer/render_target.hpp"
#include <memory>

namespace Corvus::Renderer {

using Graphics::CommandBuffer;
using Graphics::GraphicsContext;

struct RendererStats {
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    uint32_t vertices  = 0;

    void reset() {
        drawCalls = 0;
        triangles = 0;
        vertices  = 0;
    }
};

class Renderer {
public:
    explicit Renderer(GraphicsContext& context);

    // Scene rendering
    void beginScene(const Camera& camera, RenderTarget* target = nullptr);
    void submit(const Renderable& renderable);
    void endScene();

    // Direct queue rendering
    void
    renderQueue(const RenderQueue& queue, const Camera& camera, RenderTarget* target = nullptr);

    // Clear
    void clear(const glm::vec4& color = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), bool clearDepth = true);

    // Stats
    const RendererStats& getStats() const { return stats_; }
    void                 resetStats() { stats_.reset(); }

    // Context access
    GraphicsContext& getContext() { return context_; }

private:
    void renderCommand(CommandBuffer& cmd, const RenderCommand& command, const Camera& camera);

    GraphicsContext& context_;
    CommandBuffer    commandBuffer_;
    RenderQueue      queue_;
    const Camera*    currentCamera_ = nullptr;
    RenderTarget*    currentTarget_ = nullptr;
    RendererStats    stats_;
};

}
