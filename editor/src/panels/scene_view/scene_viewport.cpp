#include "editor/panels/scene_view/scene_viewport.hpp"

#include "corvus/components/entity_info.hpp"
#include "corvus/components/mesh_renderer.hpp"
#include "corvus/components/transform.hpp"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/graphics/opengl_context.hpp"
#include "corvus/log.hpp"
#include "corvus/renderer/raycast.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/string_cast.hpp"
#include <algorithm>
#include <cfloat>

namespace Corvus::Editor {

SceneViewport::SceneViewport(Core::Project& project, Graphics::GraphicsContext& ctx)
    : project(project), ctx(ctx), editorCamera(), editorGizmo(ctx), currentSize({ 1.0f, 1.0f }) {

    // Camera defaults
    editorCamera.setTarget(glm::vec3(0.0f));
    editorCamera.setDistance(10.0f);
    editorCamera.setOrbitAngles(glm::vec2(-0.6f, 0.8f));

    // Set up camera projection
    editorCamera.getCamera().setPerspective(45.0f, 1.0f, 0.1f, 1000.0f);

    auto vsBytes = Core::StaticResourceFile::create("engine/shaders/grid.vert")->readAllBytes();
    auto fsBytes = Core::StaticResourceFile::create("engine/shaders/grid.frag")->readAllBytes();
    std::string vsSrc(vsBytes.begin(), vsBytes.end());
    std::string fsSrc(fsBytes.begin(), fsBytes.end());
    gridShader = ctx.createShader(vsSrc, fsSrc);

    struct GridVertex {
        glm::vec3 pos;
        glm::vec2 uv;
    };

    float      halfSize = 500.0f;
    GridVertex verts[]  = {
        { { -halfSize, 0.f, -halfSize }, { 0, 0 } },
        { { halfSize, 0.f, -halfSize }, { 1, 0 } },
        { { halfSize, 0.f, halfSize }, { 1, 1 } },
        { { -halfSize, 0.f, halfSize }, { 0, 1 } },
    };
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    gridVBO = ctx.createVertexBuffer(verts, sizeof(verts));
    gridIBO = ctx.createIndexBuffer(indices, 6, true);
    gridVAO = ctx.createVertexArray();

    Graphics::VertexBufferLayout layout;
    layout.push<float>(3);
    layout.push<float>(2);
    gridVAO.addVertexBuffer(gridVBO, layout);
    gridVAO.setIndexBuffer(gridIBO);

    editorGizmo.initialize();
    editorGizmo.setMode(EditorGizmo::Mode::All);
    editorGizmo.setOrientation(EditorGizmo::Orientation::Local);
}

SceneViewport::~SceneViewport() {
    editorGizmo.shutdown();
    if (framebuffer.valid())
        framebuffer.release();
    if (colorTexture.valid())
        colorTexture.release();
    if (depthTexture.valid())
        depthTexture.release();
    if (gridShader.valid())
        gridShader.release();
    if (gridVAO.valid())
        gridVAO.release();
    if (gridVBO.valid())
        gridVBO.release();
    if (gridIBO.valid())
        gridIBO.release();
}

void SceneViewport::manageFramebuffer(const ImVec2& size) {
    if (size.x <= 0 || size.y <= 0)
        return;

    int  width  = static_cast<int>(size.x);
    int  height = static_cast<int>(size.y);
    bool recreate
        = !framebuffer.valid() || colorTexture.width != width || colorTexture.height != height;

    if (!recreate)
        return;

    if (framebuffer.valid())
        framebuffer.release();
    if (colorTexture.valid())
        colorTexture.release();
    if (depthTexture.valid())
        depthTexture.release();

    framebuffer  = ctx.createFramebuffer(width, height);
    colorTexture = ctx.createTexture2D(width, height);
    depthTexture = ctx.createDepthTexture(width, height);

    framebuffer.attachTexture2D(colorTexture, 0);
    framebuffer.attachDepthTexture(depthTexture);

    framebuffer.width   = width;
    framebuffer.height  = height;
    colorTexture.width  = width;
    colorTexture.height = height;
    currentSize         = size;

    float aspectRatio = width / static_cast<float>(height);
    editorCamera.getCamera().setPerspective(45.0f, aspectRatio, 0.1f, 1000.0f);
}

void SceneViewport::renderSceneToFramebuffer(Core::Entity*    selectedEntity,
                                             const glm::vec2& mousePos,
                                             bool             mousePressed,
                                             bool             mouseDown,
                                             bool             mouseInViewport) {
    if (!framebuffer.valid())
        return;

    auto& camera = editorCamera.getCamera();
    auto  view   = camera.getViewMatrix();
    auto  proj   = camera.getProjectionMatrix();
    auto  camPos = camera.getPosition();

    // Grid Pass
    {
        Graphics::CommandBuffer cmd = ctx.createCommandBuffer();
        cmd.begin();
        cmd.bindFramebuffer(framebuffer);
        cmd.setViewport(0, 0, (uint32_t)currentSize.x, (uint32_t)currentSize.y);
        cmd.executeCallback([]() { glFrontFace(GL_CCW); });
        cmd.enableScissor(false);
        cmd.clear(64.f / 255.0f, 64.f / 255.0f, 64.f / 255.0f, 1.f, true, true);
        renderGrid(cmd, view, proj, camPos);
        cmd.unbindFramebuffer();
        cmd.end();
        cmd.submit();
    }

    // Scene pass
    project.getCurrentScene()->render(ctx, camera, &framebuffer);

    // Gizmo pass
    if (selectedEntity && *selectedEntity
        && selectedEntity->hasComponent<Core::Components::TransformComponent>()) {

        auto& tr = selectedEntity->getComponent<Core::Components::TransformComponent>();
        Graphics::CommandBuffer cmd = ctx.createCommandBuffer();
        cmd.begin();
        cmd.bindFramebuffer(framebuffer);
        cmd.setViewport(0, 0, (uint32_t)currentSize.x, (uint32_t)currentSize.y);
        editorGizmo.render(cmd,
                           tr,
                           mousePos,
                           mousePressed,
                           mouseDown && mouseInViewport,
                           currentSize.x,
                           currentSize.y,
                           view,
                           proj,
                           camPos);
        cmd.unbindFramebuffer();
        cmd.end();
        cmd.submit();
    }
}

void SceneViewport::renderGrid(Graphics::CommandBuffer& cmd,
                               const glm::mat4&         view,
                               const glm::mat4&         proj,
                               const glm::vec3&         camPos) {
    if (!gridEnabled || !gridShader.valid())
        return;

    glm::mat4 vp = proj * view;
    gridShader.setMat4(cmd, "viewProjection", vp);
    gridShader.setVec3(cmd, "cameraPos", camPos);
    gridShader.setFloat(cmd, "gridSize", 1000.0f);
    cmd.setShader(gridShader);

    cmd.setVertexArray(gridVAO);
    cmd.setDepthTest(false);
    cmd.setDepthMask(false);
    cmd.setBlendState(true);
    cmd.setCullFace(false, false);
    cmd.drawIndexed(6, true);
    cmd.setCullFace(true, false);
    cmd.setDepthTest(true);
    cmd.setDepthMask(true);
}

void SceneViewport::updateCamera(const ImGuiIO& io, bool allowInput) {
    editorCamera.update(io, allowInput);
}

Core::Entity SceneViewport::pickEntity(const glm::vec2& mousePos) {
    if (!framebuffer.valid())
        return {};

    const glm::mat4 view = editorCamera.getViewMatrix();
    const glm::mat4 proj = editorCamera.getProjectionMatrix(currentSize.x / currentSize.y);
    Geometry::Ray   rayWorld
        = Geometry::buildRay(mousePos, { currentSize.x, currentSize.y }, view, proj);

    Core::Entity         best;
    Geometry::RaycastHit bestHit; // distance = max

    for (auto& e : project.getCurrentScene()->getRootOrderedEntities()) {
        if (!e.hasComponent<Core::Components::MeshRendererComponent>()
            || !e.hasComponent<Core::Components::TransformComponent>())
            continue;

        auto& tr = e.getComponent<Core::Components::TransformComponent>();
        auto& mr = e.getComponent<Core::Components::MeshRendererComponent>();

        glm::mat4 model = glm::translate(glm::mat4(1.0f), tr.position) * glm::toMat4(tr.rotation)
            * glm::scale(glm::mat4(1.0f), tr.scale);

        Geometry::RaycastHit hit;

        bool got = false;
        if (mr.hasGeneratedModel && mr.generatedModel)
            got = intersectModel(*mr.generatedModel, model, rayWorld, hit);
        else if (mr.primitiveType == Core::Components::PrimitiveType::Model
                 && mr.modelHandle->valid()) {
            auto modelPtr = mr.modelHandle.get();
            if (modelPtr && modelPtr->valid())
                got = intersectModel(*modelPtr, model, rayWorld, hit);
        }

        if (got && hit.hit && hit.distance < bestHit.distance) {
            best    = e;
            bestHit = hit;
        }
    }

    return best;
}

void SceneViewport::render(const ImVec2&    size,
                           Core::Entity*    selectedEntity,
                           const glm::vec2& mousePos,
                           bool             mousePressed,
                           bool             mouseDown,
                           bool             mouseInViewport) {
    ImVec2 valid = { std::max(1.f, size.x), std::max(1.f, size.y) };
    if (valid.x != currentSize.x || valid.y != currentSize.y)
        manageFramebuffer(valid);

    renderSceneToFramebuffer(selectedEntity, mousePos, mousePressed, mouseDown, mouseInViewport);
}

}
