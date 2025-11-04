#include "editor/panels/scene_view/scene_viewport.hpp"

#include "corvus/components/entity_info.hpp"
#include "corvus/components/mesh_renderer.hpp"
#include "corvus/components/transform.hpp"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/graphics/opengl_context.hpp"
#include "corvus/log.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
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

    float      halfSize  = 500.0f;
    GridVertex verts[]   = { { { -halfSize, 0.f, -halfSize }, { 0, 0 } },
                             { { halfSize, 0.f, -halfSize }, { 1, 0 } },
                             { { halfSize, 0.f, halfSize }, { 1, 1 } },
                             { { -halfSize, 0.f, halfSize }, { 0, 1 } } };
    uint16_t   indices[] = { 0, 1, 2, 0, 2, 3 };

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

    // Update camera aspect ratio
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

    // Get camera for rendering
    auto& camera = editorCamera.getCamera();
    auto  view   = camera.getViewMatrix();
    auto  proj   = camera.getProjectionMatrix();
    auto  camPos = camera.getPosition();

    // Render grid
    {
        Graphics::CommandBuffer cmd = ctx.createCommandBuffer();
        cmd.begin();
        cmd.bindFramebuffer(framebuffer);
        cmd.setViewport(0, 0, (uint32_t)currentSize.x, (uint32_t)currentSize.y);

        cmd.executeCallback([]() { glFrontFace(GL_CCW); });

        cmd.enableScissor(false);
        cmd.clear(64.f / 255.0f, 64.f / 255.0f, 64.0f / 255.0f, 1.f, true, true);

        renderGrid(cmd, view, proj, camPos);

        cmd.unbindFramebuffer();
        cmd.end();
        cmd.submit();
    }

    // Render scene using new camera-based API
    project.getCurrentScene()->render(ctx, camera, &framebuffer);

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

    // Set uniforms through command buffer
    gridShader.setMat4(cmd, "viewProjection", vp);
    gridShader.setVec3(cmd, "cameraPos", camPos);
    gridShader.setFloat(cmd, "gridSize", 1000.0f);
    cmd.setShader(gridShader);

    // Set rendering state and draw
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

    glm::mat4 view = editorCamera.getViewMatrix();
    glm::mat4 proj = editorCamera.getProjectionMatrix(currentSize.x / currentSize.y);
    glm::mat4 inv  = glm::inverse(proj * view);

    glm::vec2 ndc
        = { (2.f * mousePos.x) / currentSize.x - 1.f, 1.f - (2.f * mousePos.y) / currentSize.y };
    glm::vec4 nearP = inv * glm::vec4(ndc, 0.f, 1.f);
    glm::vec4 farP  = inv * glm::vec4(ndc, 1.f, 1.f);
    nearP /= nearP.w;
    farP /= farP.w;

    glm::vec3 ro = glm::vec3(nearP);
    glm::vec3 rd = glm::normalize(glm::vec3(farP - nearP));

    Core::Entity picked;
    float        closest = FLT_MAX;

    for (auto& e : project.getCurrentScene()->getRootOrderedEntities()) {
        if (!e.hasComponent<Core::Components::MeshRendererComponent>()
            || !e.hasComponent<Core::Components::TransformComponent>())
            continue;

        auto&     tr     = e.getComponent<Core::Components::TransformComponent>();
        glm::vec3 pos    = tr.position;
        float     radius = 1.0f;
        glm::vec3 oc     = ro - pos;
        float     a      = glm::dot(rd, rd);
        float     b      = 2.f * glm::dot(oc, rd);
        float     c      = glm::dot(oc, oc) - radius * radius;
        float     disc   = b * b - 4 * a * c;

        if (disc < 0)
            continue;

        float t = (-b - sqrtf(disc)) / (2 * a);
        if (t >= 0 && t < closest) {
            closest = t;
            picked  = e;
        }
    }
    return picked;
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
