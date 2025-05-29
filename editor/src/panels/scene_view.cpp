#include "editor/panels/scene_view.hpp"

#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "linp/components/entity_info.hpp"
#include "linp/components/mesh_renderer.hpp"
#include "linp/components/transform.hpp"
#include "linp/entity.hpp"
#include "linp/log.hpp" // For LINP_ERROR/WARN

namespace Linp::Editor {

/**
 * @brief Constructs a SceneViewPanel.
 */
SceneViewPanel::SceneViewPanel(Core::Scene& scene, SceneHierarchyPanel* sceneHierarchy)
    : scene(scene),
      sceneHierarchyPanel(sceneHierarchy),
      sceneTexture({ 0 }),
      camera(),
      currentViewportSize({ 1.0f, 1.0f }),
      cameraTarget({ 0.0f, 0.0f, 0.0f }),
      cameraOrbitAngles({ 0.45f, -0.45f }),
      cameraDistance(10.0f),
      isWindowFocused(false),
      isWindowHovered(false) {

    // Initialize Camera
    camera.target     = cameraTarget;
    camera.up         = { 0.0f, 1.0f, 0.0f };
    camera.fovy       = CAMERA_DEFAULT_FOV;
    camera.projection = CAMERA_PERSPECTIVE;

    updateCameraVectors(); // Calculate initial position
}

/**
 * @brief Destructor.
 */
SceneViewPanel::~SceneViewPanel() {
    if (sceneTexture.id != 0) {
        UnloadRenderTexture(sceneTexture);
    }
}

/**
 * @brief Updates camera's 3D position.
 */
void SceneViewPanel::updateCameraVectors() {
    Matrix  rotation = MatrixRotateZYX({ cameraOrbitAngles.x, cameraOrbitAngles.y, 0.0f });
    Vector3 orbitArm = Vector3Transform({ 0.0f, 0.0f, cameraDistance }, rotation);
    camera.position  = Vector3Add(cameraTarget, orbitArm);
    camera.target    = cameraTarget;
}

/**
 * @brief Manages the RenderTexture.
 */
void SceneViewPanel::manageRenderTexture(const ImVec2& size) {
    if (size.x <= 0 || size.y <= 0) {
        // LINP_WARN("SceneViewPanel: Attempted to create RenderTexture with non-positive size.");
        return;
    }
    int width  = static_cast<int>(size.x);
    int height = static_cast<int>(size.y);

    if (sceneTexture.id == 0 || sceneTexture.texture.width != width || sceneTexture.texture.height != height) {
        if (sceneTexture.id != 0)
            UnloadRenderTexture(sceneTexture);
        sceneTexture = LoadRenderTexture(width, height);
        if (sceneTexture.id != 0) {
            SetTextureFilter(sceneTexture.texture, TEXTURE_FILTER_BILINEAR);
        } else {
            LINP_ERROR("SceneViewPanel: Failed to load/reload scene view render texture.");
        }
    }
}

/**
 * @brief Handles camera input.
 */
void SceneViewPanel::processCameraInput(const ImGuiIO& io) {
    if (!isWindowFocused && !isWindowHovered)
        return;

    bool cameraChanged = false;

    if (isWindowHovered && io.MouseWheel != 0.0f) {
        cameraDistance -= io.MouseWheel * CAMERA_ZOOM_SPEED;
        cameraDistance = std::clamp(cameraDistance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);
        cameraChanged  = true;
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && isWindowHovered) {
        cameraOrbitAngles.y -= io.MouseDelta.x * CAMERA_ORBIT_SPEED;
        cameraOrbitAngles.x -= io.MouseDelta.y * CAMERA_ORBIT_SPEED;
        cameraOrbitAngles.x = std::clamp(cameraOrbitAngles.x, CAMERA_PITCH_MIN, CAMERA_PITCH_MAX);
        cameraChanged       = true;
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && isWindowHovered) {
        Matrix  invCameraMat = MatrixInvert(GetCameraMatrix(camera));
        float   panSpeed     = CAMERA_PAN_SPEED_FACTOR * cameraDistance;
        Vector3 right        = { invCameraMat.m0, invCameraMat.m1, invCameraMat.m2 };
        Vector3 up           = { invCameraMat.m4, invCameraMat.m5, invCameraMat.m6 };
        cameraTarget         = Vector3Add(cameraTarget, Vector3Scale(right, -io.MouseDelta.x * panSpeed));
        cameraTarget         = Vector3Add(cameraTarget, Vector3Scale(up, io.MouseDelta.y * panSpeed));
        cameraChanged        = true;
    }

    if (cameraChanged)
        updateCameraVectors();
}

/**
 * @brief Renders scene to texture.
 */
void SceneViewPanel::renderSceneToTexture() {
    if (sceneTexture.id == 0 || currentViewportSize.x <= 0 || currentViewportSize.y <= 0)
        return;

    BeginTextureMode(sceneTexture);
    ClearBackground(SKYBLUE);
    BeginMode3D(camera);
    DrawGrid(100, 1.0f);
    auto meshView = scene.registry.view<Core::Components::TransformComponent, Core::Components::MeshRendererComponent>();
    for (auto entityHandle : meshView) {
        Core::Entity entity { entityHandle, &scene };
        if (entity.hasComponent<Core::Components::EntityInfoComponent>() && !entity.getComponent<Core::Components::EntityInfoComponent>().enabled) {
            continue;
        }
        auto& transformComp = meshView.get<Core::Components::TransformComponent>(entityHandle);
        auto& meshRenderer  = meshView.get<Core::Components::MeshRendererComponent>(entityHandle);
        if (meshRenderer.mesh && meshRenderer.material) {
            DrawMesh(*meshRenderer.mesh, *meshRenderer.material, transformComp.getMatrix());
        }
    }
    EndMode3D();
    EndTextureMode();
}

/**
 * @brief Handles entity picking.
 */
void SceneViewPanel::processEntityPicking(const Vector2& mousePosInTexture, int textureWidth, int textureHeight) {
    if (sceneTexture.id == 0)
        return;

    Ray          mouseRay           = GetScreenToWorldRayEx(mousePosInTexture, camera, textureWidth, textureHeight);
    Core::Entity pickedEntity       = {};
    float        closestHitDistance = FLT_MAX;

    auto rootOrderedEntities = scene.getRootOrderedEntities();
    for (auto it = rootOrderedEntities.rbegin(); it != rootOrderedEntities.rend(); ++it) {
        Core::Entity& entity = *it;
        if (!entity.hasComponent<Core::Components::EntityInfoComponent>() || !entity.getComponent<Core::Components::EntityInfoComponent>().enabled) {
            continue;
        }
        if (entity.hasComponent<Core::Components::MeshRendererComponent>() && entity.hasComponent<Core::Components::TransformComponent>()) {
            const auto& meshRenderer  = entity.getComponent<Core::Components::MeshRendererComponent>();
            const auto& transformComp = entity.getComponent<Core::Components::TransformComponent>();
            if (meshRenderer.mesh) {
                RayCollision collision = GetRayCollisionMesh(mouseRay, *meshRenderer.mesh, transformComp.getMatrix());
                if (collision.hit && collision.distance < closestHitDistance) {
                    closestHitDistance = collision.distance;
                    pickedEntity       = entity;
                }
            }
        }
    }

    if (sceneHierarchyPanel) {
        // If a valid entity was picked, select it. Otherwise, if nothing was picked (empty space click),
        if (pickedEntity.isValid()) {
            sceneHierarchyPanel->selectedEntity = pickedEntity;
        } else {
            sceneHierarchyPanel->selectedEntity = Core::Entity(); // Empty handle
        }
    }
}

/**
 * @brief Updates and renders the panel.
 */
void SceneViewPanel::onUpdate() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin(ICON_FA_CUBES " Scene View")) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }
    ImGui::PopStyleVar();

    isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    isWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    ImVec2 newViewportSize = ImGui::GetContentRegionAvail();
    if (newViewportSize.x < 1.0f)
        newViewportSize.x = 1.0f;
    if (newViewportSize.y < 1.0f)
        newViewportSize.y = 1.0f;
    if (newViewportSize.x != currentViewportSize.x || newViewportSize.y != currentViewportSize.y) {
        currentViewportSize = newViewportSize;
    }
    manageRenderTexture(currentViewportSize);

    processCameraInput(ImGui::GetIO());
    renderSceneToTexture();

    // Display texture and handle interaction (previously in displayTextureAndHandleInteraction())
    if (sceneTexture.id == 0 || currentViewportSize.x <= 0 || currentViewportSize.y <= 0) {
        ImGui::TextDisabled("Scene View not available (texture error or zero size).");
    } else {
        ImGui::Image(
            (ImTextureID)(intptr_t)sceneTexture.texture.id,
            currentViewportSize,
            ImVec2(0, 1),
            ImVec2(1, 0));

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2  imageTopLeft     = ImGui::GetItemRectMin();
            ImVec2  mousePosAbsolute = ImGui::GetMousePos();
            Vector2 mousePosRelative = {
                mousePosAbsolute.x - imageTopLeft.x,
                mousePosAbsolute.y - imageTopLeft.y
            };

            if (mousePosRelative.x >= 0 && mousePosRelative.x < sceneTexture.texture.width && mousePosRelative.y >= 0 && mousePosRelative.y < sceneTexture.texture.height) {
                processEntityPicking(mousePosRelative, sceneTexture.texture.width, sceneTexture.texture.height);
            }
        }
    }

    ImGui::End();
}

} // namespace Linp::Editor
