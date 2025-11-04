#include "corvus/renderer/render_queue.hpp"
#include <algorithm>

namespace Corvus::Renderer {

uint32_t RenderCommand::generateSortKey(const Material& material, uint32_t meshId) {
    // Pack shader ID (16 bits) and mesh ID (16 bits) for state sorting
    uint32_t shaderBits = (material.getShaderId() & 0xFFFF) << 16;
    uint32_t meshBits   = meshId & 0xFFFF;
    return shaderBits | meshBits;
}

void RenderQueue::submit(const Renderable& renderable, const glm::vec3& cameraPosition) {
    if (!renderable.isVisible()) {
        return;
    }

    RenderCommand cmd;
    cmd.renderable  = &renderable;
    cmd.modelMatrix = renderable.getTransform().getMatrix();

    // Calculate distance to camera for depth sorting
    glm::vec3 objectPos  = renderable.getTransform().getPosition();
    cmd.distanceToCamera = glm::length(objectPos - cameraPosition);

    // Generate sort key for state sorting
    cmd.sortKey = RenderCommand::generateSortKey(*renderable.getMaterial(),
                                                 renderable.getMesh().getVAO().id);

    commands_.push_back(cmd);
}

void RenderQueue::clear() { commands_.clear(); }

void RenderQueue::sortByState() {
    std::sort(commands_.begin(),
              commands_.end(),
              [](const RenderCommand& a, const RenderCommand& b) { return a.sortKey < b.sortKey; });
}

void RenderQueue::sortByDepth(bool frontToBack) {
    if (frontToBack) {
        std::sort(
            commands_.begin(), commands_.end(), [](const RenderCommand& a, const RenderCommand& b) {
                return a.distanceToCamera < b.distanceToCamera;
            });
    } else {
        std::sort(
            commands_.begin(), commands_.end(), [](const RenderCommand& a, const RenderCommand& b) {
                return a.distanceToCamera > b.distanceToCamera;
            });
    }
}

}
