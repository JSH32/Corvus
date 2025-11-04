#include "corvus/renderer/renderer.hpp"

namespace Corvus::Renderer {

Renderer::Renderer(GraphicsContext& context)
    : context_(context), commandBuffer_(context.createCommandBuffer()) { }

void Renderer::beginScene(const Camera& camera, RenderTarget* target) {
    currentCamera_ = &camera;
    currentTarget_ = target;
    queue_.clear();
    stats_.reset();

    commandBuffer_.begin();

    if (currentTarget_) {
        currentTarget_->bind(commandBuffer_);
    } else {
        commandBuffer_.unbindFramebuffer();
    }
}

void Renderer::submit(const Renderable& renderable) {
    if (!currentCamera_) {
        return;
    }

    queue_.submit(renderable, currentCamera_->getPosition());
}

void Renderer::endScene() {
    if (!currentCamera_) {
        return;
    }

    // Sort queue for optimal rendering
    queue_.sortByState(); // Minimize state changes

    // Render all commands
    for (const auto& cmd : queue_.getCommands()) {
        renderCommand(commandBuffer_, cmd, *currentCamera_);
    }

    commandBuffer_.end();
    commandBuffer_.submit();

    currentCamera_ = nullptr;
    currentTarget_ = nullptr;
}

void Renderer::renderQueue(const RenderQueue& queue, const Camera& camera, RenderTarget* target) {
    stats_.reset();

    auto cmd = context_.createCommandBuffer();
    cmd.begin();

    if (target) {
        target->bind(cmd);
    } else {
        cmd.unbindFramebuffer();
    }

    for (const auto& renderCmd : queue.getCommands()) {
        renderCommand(cmd, renderCmd, camera);
    }

    cmd.end();
    cmd.submit();
}

void Renderer::clear(const glm::vec4& color, bool clearDepth) {
    auto cmd = context_.createCommandBuffer();
    cmd.begin();

    if (currentTarget_) {
        currentTarget_->bind(cmd);
    } else {
        cmd.unbindFramebuffer();
    }

    cmd.clear(color.r, color.g, color.b, color.a, clearDepth);
    cmd.end();
    cmd.submit();
}

void Renderer::renderCommand(CommandBuffer&       cmd,
                             const RenderCommand& command,
                             const Camera&        camera) {
    const Renderable& renderable = *command.renderable;
    Material&         material   = *renderable.getMaterial();
    const Mesh&       mesh       = renderable.getMesh();

    // Bind material (shader, textures, render state, uniforms)
    material.bind(cmd);

    // Set standard uniforms
    glm::mat4 mvp = camera.getViewProjectionMatrix() * command.modelMatrix;
    material.getShader().setMat4(cmd, "u_MVP", mvp);
    material.getShader().setMat4(cmd, "u_Model", command.modelMatrix);
    material.getShader().setMat4(cmd, "u_View", camera.getViewMatrix());
    material.getShader().setMat4(cmd, "u_Projection", camera.getProjectionMatrix());

    // Draw mesh
    mesh.draw(cmd);

    // Update stats
    stats_.drawCalls++;
    stats_.triangles += mesh.getIndexCount() / 3;
    stats_.vertices += mesh.getIndexCount();
}

}
