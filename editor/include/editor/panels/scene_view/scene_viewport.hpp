#pragma once
#include "corvus/entity.hpp"
#include "corvus/graphics/graphics.hpp"
#include "corvus/project/project.hpp"
#include "corvus/scene.hpp"
#include "editor/camera/editor_camera.hpp"
#include "editor/gizmo/editor_gizmo.hpp"
#include "glm/glm.hpp"
#include "imgui.h"

namespace Corvus::Editor {

/**
 * @class SceneViewport
 * @brief Renders the active scene to an off-screen framebuffer, handles camera input,
 *        and manages editor-level gizmos and grid display.
 *
 * This class owns its own framebuffer and camera, rendering into an internal texture
 * that can be displayed in an ImGui panel. It is fully backend-agnostic through
 * the GraphicsContext/CommandBuffer API.
 */
class SceneViewport {
public:
    explicit SceneViewport(Core::Project& project, Graphics::GraphicsContext& ctx);
    ~SceneViewport();

    /**
     * @brief Renders the active scene into the viewport's framebuffer.
     * @param size The size (in pixels) of the viewport region in ImGui.
     * @param selectedEntity current selected entity for gizmo rendering
     * @param mousePos mouse position
     * @param mousePressed is mouse currently being pressed
     * @param mouseDown is the mouse being held down
     * @param mouseInViewport is the mouse within the viewport
     */
    void render(const ImVec2&    size,
                Core::Entity*    selectedEntity,
                const glm::vec2& mousePos,
                bool             mousePressed,
                bool             mouseDown,
                bool             mouseInViewport);

    /**
     * @brief Updates the editor camera based on user input.
     * @param io ImGui IO reference
     * @param inputAllowed Whether input should be processed this frame
     */
    void updateCamera(const ImGuiIO& io, bool inputAllowed);

    /**
     * @brief Performs entity picking from the mouse position.
     * @param mousePos Mouse position relative to the viewport
     * @return The entity under the mouse, or an empty handle if none.
     */
    Core::Entity pickEntity(const glm::vec2& mousePos);

    /**
     * @brief Gets the framebuffer for rendering.
     */
    const Graphics::Framebuffer& getFramebuffer() const { return framebuffer; }

    /**
     * @brief Gets the color texture attached to the framebuffer.
     */
    const Graphics::Texture2D& getColorTexture() const { return colorTexture; }

    /**
     * @brief Returns whether the framebuffer is currently valid.
     */
    bool isValid() const { return framebuffer.valid(); }

    /**
     * @brief Gets the editor camera.
     */
    EditorCamera&       getCamera() { return editorCamera; }
    const EditorCamera& getCamera() const { return editorCamera; }

    /**
     * @brief Gets the editor gizmo.
     */
    EditorGizmo&       getGizmo() { return editorGizmo; }
    const EditorGizmo& getGizmo() const { return editorGizmo; }

    /**
     * @brief Toggles the grid overlay.
     */
    void setGridEnabled(bool enabled);
    bool isGridEnabled() const;

private:
    /**
     * @brief Creates or resizes the internal framebuffer as needed.
     */
    void manageFramebuffer(const ImVec2& size);

    /**
     * @brief Renders the grid, scene, and any active gizmos to the framebuffer.
     */
    void renderSceneToFramebuffer(Core::Entity*    selectedEntity,
                                  const glm::vec2& mousePos,
                                  bool             mousePressed,
                                  bool             mouseDown,
                                  bool             mouseInViewport);

    /**
     * @brief Renders the editor grid overlay.
     */
    void renderGrid(Graphics::CommandBuffer& cmd,
                    const glm::mat4&         view,
                    const glm::mat4&         proj,
                    const glm::vec3&         camPo);

    // Core references
    Core::Project&             project;
    Graphics::GraphicsContext& ctx;

    // Core systems
    EditorCamera editorCamera;
    EditorGizmo  editorGizmo;

    // Rendering resources
    Graphics::Framebuffer framebuffer;
    Graphics::Texture2D   colorTexture;
    Graphics::Texture2D   depthTexture;
    ImVec2                currentSize { 1.0f, 1.0f };

    // Grid
    Graphics::Shader       gridShader;
    Graphics::VertexArray  gridVAO;
    Graphics::VertexBuffer gridVBO;
    Graphics::IndexBuffer  gridIBO;
    bool                   gridEnabled = true;
};

}
