#pragma once
#include "corvus/renderer/renderable.hpp"
#include <functional>
#include <vector>

namespace Corvus::Renderer {

struct RenderCommand {
    const Renderable* renderable;
    glm::mat4         modelMatrix;
    float             distanceToCamera; // For depth sorting
    uint32_t          sortKey;          // For state sorting

    // Generate sort key for state-based sorting
    static uint32_t generateSortKey(const Material& material, uint32_t meshId);
};

class RenderQueue {
public:
    void submit(const Renderable& renderable, const glm::vec3& cameraPosition);
    void clear();

    // Sort commands
    void sortByState();                        // Minimize state changes
    void sortByDepth(bool frontToBack = true); // Depth sorting

    const std::vector<RenderCommand>& getCommands() const { return commands_; }

private:
    std::vector<RenderCommand> commands_;
};

}
