#pragma once

#include "RenderTexture.hpp"
#include "editor/camera/editor_camera.hpp"
#include "editor/gizmo/editor_gizmo.hpp"
#include "imgui.h"
#include "linp/entity.hpp"
#include "linp/scene.hpp"
#include "raylib.h"

namespace Linp::Editor {

/**
 * @class SceneViewport
 * @brief Core 3D viewport functionality for rendering scenes with camera controls and entity
 * interaction.
 *
 * Handles scene rendering to texture, camera management, entity picking, and gizmo manipulation.
 * Designed to be reusable across different editor UI contexts.
 */
class SceneViewport {
public:
    /**
     * @brief Constructs a SceneViewport.
     * @param scene A reference to the scene to be rendered.
     */
    explicit SceneViewport(Core::Scene& scene);

    /**
     * @brief Destructor. Cleans up the Raylib RenderTexture.
     */
    ~SceneViewport();

    /**
     * @brief Renders the scene to an internal texture with the specified size.
     * @param size Desired viewport size.
     */
    void render(const ImVec2& size);

    /**
     * @brief Updates camera input processing.
     * @param io ImGui IO context for input handling.
     * @param inputAllowed Whether camera input should be processed.
     */
    void updateCamera(const ImGuiIO& io, bool inputAllowed);

    /**
     * @brief Performs entity picking at the specified mouse position.
     * @param mousePos Mouse position relative to the viewport.
     * @return The picked entity, or an invalid entity if nothing was picked.
     */
    Core::Entity pickEntity(const Vector2& mousePos);

    /**
     * @brief Updates the gizmo for the specified entity.
     * @param entity Entity to manipulate with the gizmo.
     * @param mousePos Mouse position relative to the viewport.
     * @param mousePressed Whether the mouse was just pressed.
     * @param mouseDown Whether the mouse is currently down.
     * @param mouseInViewport Whether the mouse is within the viewport bounds.
     * @param viewportWidth Current viewport width.
     * @param viewportHeight Current viewport height.
     */
    void updateGizmo(Core::Entity&  entity,
                     const Vector2& mousePos,
                     bool           mousePressed,
                     bool           mouseDown,
                     bool           mouseInViewport,
                     float          viewportWidth,
                     float          viewportHeight);

    /**
     * @brief Gets the render texture for display in ImGui.
     */
    const RenderTexture& getRenderTexture() const { return sceneTexture; }

    /**
     * @brief Gets the editor camera for external access.
     */
    EditorCamera&       getCamera() { return editorCamera; }
    const EditorCamera& getCamera() const { return editorCamera; }

    /**
     * @brief Gets the editor gizmo for external access.
     */
    EditorGizmo& getGizmo() { return editorGizmo; }

    /**
     * @brief Checks if the viewport has a valid render texture.
     */
    bool isValid() const { return sceneTexture.id != 0; }

    void setGridEnabled(bool enabled) { gridEnabled = enabled; }
    bool isGridEnabled() const { return gridEnabled; }

private:
    // void drawInfiniteGrid(const Camera3D& camera, float baseGridSize = 1.0f);
    void renderGrid(const Camera3D& camera);

    /**
     * @brief Manages the render texture (creation, resizing).
     * @param size Desired size of the render texture.
     */
    void manageRenderTexture(const ImVec2& size);

    /**
     * @brief Renders the scene content to the internal texture.
     */
    void renderSceneToTexture();

    // Core components
    Core::Scene& scene;
    EditorCamera editorCamera;
    EditorGizmo  editorGizmo;

    // Rendering
    raylib::RenderTexture sceneTexture;
    ImVec2                currentSize;

    // Grid rendering
    Shader gridShader;
    Mesh   gridQuad;
    bool   gridEnabled = true;
};

}
