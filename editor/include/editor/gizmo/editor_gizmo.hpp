#pragma once
#include "editor/gizmo/raygizmo.h"
#include "linp/components/transform.hpp"
#include "raylib.h"

namespace Linp::Editor {

/**
 * @class EditorGizmo
 * @brief Handles 3D gizmo rendering and manipulation for transforms.
 *
 * Updates every frame with a transform reference and provides hover/active state.
 * The gizmo automatically synchronizes with the provided transform.
 */
class EditorGizmo {
public:
    /**
     * @brief Gizmo operation modes
     */
    enum class Mode {
        Translate = GIZMO_TRANSLATE,
        Rotate    = GIZMO_ROTATE,
        Scale     = GIZMO_SCALE,
        All       = GIZMO_TRANSLATE | GIZMO_ROTATE | GIZMO_SCALE
    };

    /**
     * @brief Updates and renders gizmo for the given transform.
     * Call this every frame when you want the gizmo to be active.
     * @param transform The transform to manipulate
     * @param mousePos Mouse position relative to viewport
     * @param mousePressed Whether mouse was pressed this frame
     * @param mouseDown Whether mouse is currently down
     * @param mouseInViewport Whether mouse is within the viewport
     * @param viewportWidth Width of the viewport
     * @param viewportHeight Height of the viewport
     */
    void update(Core::Components::TransformComponent& transform,
                const Vector2&                        mousePos,
                bool                                  mousePressed,
                bool                                  mouseDown,
                bool                                  mouseInViewport,
                float                                 viewportWidth,
                float                                 viewportHeight);

    /**
     * @brief Updates and renders gizmo but doesn't modify any transform.
     * Useful for read-only visualization.
     * @param transform The transform to visualize
     * @param mousePos Mouse position relative to viewport
     * @param viewportWidth Width of the viewport
     * @param viewportHeight Height of the viewport
     */
    void updateReadOnly(const Core::Components::TransformComponent& transform,
                        const Vector2&                              mousePos,
                        float                                       viewportWidth,
                        float                                       viewportHeight);

    /**
     * @brief Gets whether gizmo is currently active (being dragged).
     */
    bool isActive() const { return gizmoIsActive; }

    /**
     * @brief Gets whether gizmo is currently hovered.
     */
    bool isHovered() const { return gizmoHovered; }

    /**
     * @brief Sets the gizmo operation mode.
     */
    void setMode(Mode mode) { currentMode = mode; }

    /**
     * @brief Gets the current gizmo operation mode.
     */
    Mode getMode() const { return currentMode; }

    /**
     * @brief Enables/disables the gizmo.
     */
    void setEnabled(bool enabled) { gizmoEnabled = enabled; }

    /**
     * @brief Gets whether gizmo is enabled.
     */
    bool isEnabled() const { return gizmoEnabled; }

private:
    /**
     * @brief Syncs internal gizmo transform from component.
     */
    void syncFromTransform(const Core::Components::TransformComponent& transform);

    /**
     * @brief Syncs internal gizmo transform back to component.
     */
    void syncToTransform(Core::Components::TransformComponent& transform);

    /**
     * @brief Renders the gizmo and handles interaction.
     * @return True if gizmo was manipulated this frame
     */
    bool renderAndInteract(const Vector2& mousePos,
                           bool           mousePressed,
                           bool           mouseDown,
                           bool           mouseInViewport,
                           float          viewportWidth,
                           float          viewportHeight);

    /**
     * @brief Updates hover state based on mouse position.
     */
    void updateHoverState(const Vector2& mousePos, float viewportWidth, float viewportHeight);

    Transform gizmoTransform = GizmoIdentity();
    Mode      currentMode    = Mode::All;
    bool      gizmoEnabled   = true;
    bool      gizmoIsActive  = false;
    bool      gizmoHovered   = false;
};

}
