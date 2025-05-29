#include "editor/panels/scene_view/scene_viewport.hpp"

#include "ShaderUnmanaged.hpp"
#include "linp/components/entity_info.hpp"
#include "linp/components/mesh_renderer.hpp"
#include "linp/components/transform.hpp"
#include "linp/files/static_resource_file.hpp"
#include "linp/log.hpp"
#include "raylib.h"
#include <algorithm>

namespace Linp::Editor {

SceneViewport::SceneViewport(Core::Scene& scene)
    : scene(scene),
      editorCamera(),
      editorGizmo(),
      sceneTexture({ 0 }),
      currentSize({ 1.0f, 1.0f }) {

    // Set up camera with good defaults for scene editing
    editorCamera.setTarget({ 0.0f, 0.0f, 0.0f });
    editorCamera.setDistance(10.0f);
    editorCamera.setOrbitAngles({ 0.45f, -0.45f });

    auto        vertBytes = Core::StaticResourceFile::create("engine/shaders/grid.vert")->readAllBytes();
    auto        fragBytes = Core::StaticResourceFile::create("engine/shaders/grid.frag")->readAllBytes();
    std::string vertShader(vertBytes.begin(), vertBytes.end());
    std::string fragShader(fragBytes.begin(), fragBytes.end());

    gridShader = LoadShaderFromMemory(vertShader.c_str(), fragShader.c_str());
    gridQuad   = GenMeshPlane(1000.0f, 1000.0f, 1, 1);
}

SceneViewport::~SceneViewport() {
    if (sceneTexture.id != 0) {
        UnloadRenderTexture(sceneTexture);
    }

    UnloadShader(this->gridShader);
    UnloadMesh(this->gridQuad);
}

void SceneViewport::manageRenderTexture(const ImVec2& size) {
    if (size.x <= 0 || size.y <= 0) {
        return; // Invalid size
    }

    int width  = static_cast<int>(size.x);
    int height = static_cast<int>(size.y);

    // Only recreate if size changed or texture doesn't exist
    bool needsRecreation = (sceneTexture.id == 0) || (sceneTexture.texture.width != width) || (sceneTexture.texture.height != height);

    if (needsRecreation) {
        if (sceneTexture.id != 0) {
            UnloadRenderTexture(sceneTexture);
        }

        sceneTexture = LoadRenderTexture(width, height);
        if (sceneTexture.id != 0) {
            SetTextureFilter(sceneTexture.texture, TEXTURE_FILTER_BILINEAR);
            currentSize = size;
        } else {
            LINP_ERROR("SceneViewport: Failed to create render texture.");
        }
    }
}

void SceneViewport::renderSceneToTexture() {
    if (sceneTexture.id == 0 || currentSize.x <= 0 || currentSize.y <= 0) {
        return;
    }

    BeginTextureMode(sceneTexture);
    ClearBackground({ 64, 64, 64, 255 });

    // Use the EditorCamera's raylib camera
    BeginMode3D(editorCamera.getCamera());

    // Draw grid
    renderGrid(editorCamera.getCamera());

    // Render entities
    scene.render(sceneTexture);

    EndMode3D();
    EndTextureMode();
}

void SceneViewport::render(const ImVec2& size) {
    // Ensure valid size
    ImVec2 validSize = {
        std::max(1.0f, size.x),
        std::max(1.0f, size.y)
    };

    // Update viewport size if changed
    if (validSize.x != currentSize.x || validSize.y != currentSize.y) {
        manageRenderTexture(validSize);
    }

    // Render scene to texture
    renderSceneToTexture();
}

void SceneViewport::updateCamera(const ImGuiIO& io, bool inputAllowed) {
    editorCamera.update(io, inputAllowed);
}

Core::Entity SceneViewport::pickEntity(const Vector2& mousePos) {
    if (sceneTexture.id == 0) {
        return {};
    }

    // Cast ray from mouse position into 3D world using EditorCamera
    Ray mouseRay = GetScreenToWorldRayEx(mousePos, editorCamera.getCamera(),
        sceneTexture.texture.width,
        sceneTexture.texture.height);

    Core::Entity closestEntity   = {};
    float        closestDistance = FLT_MAX;

    // Check all entities for collision (render order matters for picking)
    auto rootOrderedEntities = scene.getRootOrderedEntities();
    for (auto it = rootOrderedEntities.rbegin(); it != rootOrderedEntities.rend(); ++it) {
        Core::Entity& entity = *it;

        // Skip disabled entities
        if (!entity.hasComponent<Core::Components::EntityInfoComponent>() || !entity.getComponent<Core::Components::EntityInfoComponent>().enabled) {
            continue;
        }

        // Only check entities with transform
        if (!entity.hasComponent<Core::Components::MeshRendererComponent>() || !entity.hasComponent<Core::Components::TransformComponent>()) {
            continue;
        }

        const auto& meshRenderer  = entity.getComponent<Core::Components::MeshRendererComponent>();
        const auto& transformComp = entity.getComponent<Core::Components::TransformComponent>();

        if (meshRenderer.mesh) {
            RayCollision collision = GetRayCollisionMesh(mouseRay, *meshRenderer.mesh, transformComp.getMatrix());
            if (collision.hit && collision.distance < closestDistance) {
                closestDistance = collision.distance;
                closestEntity   = entity;
            }
        }
    }

    return closestEntity;
}

void SceneViewport::updateGizmo(Core::Entity& entity, const Vector2& mousePos, bool mousePressed,
    bool mouseDown, bool mouseInViewport, float viewportWidth, float viewportHeight) {
    if (!entity || !entity.hasComponent<Core::Components::TransformComponent>()) {
        return;
    }

    auto& transform = entity.getComponent<Core::Components::TransformComponent>();

    // Render gizmo during scene rendering
    if (sceneTexture.id != 0) {
        BeginTextureMode(sceneTexture);
        BeginMode3D(editorCamera.getCamera());

        editorGizmo.update(transform, mousePos, mousePressed, mouseDown, mouseInViewport, viewportWidth, viewportHeight);

        EndMode3D();
        EndTextureMode();
    }
}

void SceneViewport::renderGrid(const Camera3D& camera) {
    if (!gridEnabled)
        return;

    // Set shader uniforms
    Matrix viewProjection = MatrixMultiply(GetCameraMatrix(camera),
        MatrixPerspective(camera.fovy * DEG2RAD,
            (float)sceneTexture.texture.width / sceneTexture.texture.height,
            0.1f, 1000.0f));

    SetShaderValueMatrix(gridShader, GetShaderLocation(gridShader, "viewProjection"), viewProjection);
    SetShaderValue(gridShader, GetShaderLocation(gridShader, "cameraPos"), &camera.position, SHADER_UNIFORM_VEC3);

    float gridSize = 1000.0f;
    SetShaderValue(gridShader, GetShaderLocation(gridShader, "gridSize"), &gridSize, SHADER_UNIFORM_FLOAT);

    Material gridMaterial = LoadMaterialDefault();
    gridMaterial.shader   = gridShader;

    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();

    // Draw the grid
    BeginShaderMode(gridShader);
    DrawMesh(gridQuad, gridMaterial, MatrixIdentity());
    EndShaderMode();

    rlEnableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
}
}
