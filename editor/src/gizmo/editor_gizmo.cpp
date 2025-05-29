#include "editor/gizmo/editor_gizmo.hpp"

namespace Linp::Editor {

void EditorGizmo::update(Core::Components::TransformComponent& transform,
    const Vector2&                                             mousePos,
    bool                                                       mousePressed,
    bool                                                       mouseDown,
    bool                                                       mouseInViewport,
    float                                                      viewportWidth,
    float                                                      viewportHeight) {

    if (!gizmoEnabled) {
        gizmoIsActive = false;
        gizmoHovered  = false;
        return;
    }

    // Always sync from transform first
    syncFromTransform(transform);

    // Update hover state
    updateHoverState(mousePos, viewportWidth, viewportHeight);

    // Render gizmo and handle interaction
    bool wasManipulated = renderAndInteract(mousePos, mousePressed, mouseDown,
        mouseInViewport, viewportWidth, viewportHeight);

    gizmoIsActive = wasManipulated;

    // Sync back to transform if gizmo was manipulated
    if (wasManipulated) {
        syncToTransform(transform);
    }
}

void EditorGizmo::updateReadOnly(const Core::Components::TransformComponent& transform,
    const Vector2&                                                           mousePos,
    float                                                                    viewportWidth,
    float                                                                    viewportHeight) {

    if (!gizmoEnabled) {
        gizmoIsActive = false;
        gizmoHovered  = false;
        return;
    }

    // Sync from transform
    syncFromTransform(transform);

    // Update hover state only
    updateHoverState(mousePos, viewportWidth, viewportHeight);

    // Render gizmo without interaction (read-only)
    DrawGizmo3D(
        static_cast<int>(currentMode),
        &gizmoTransform,
        mousePos,
        false, // No mouse pressed
        false, // No mouse down
        viewportWidth,
        viewportHeight);

    // Always inactive in read-only mode
    gizmoIsActive = false;
}

void EditorGizmo::syncFromTransform(const Core::Components::TransformComponent& transform) {
    gizmoTransform.translation = transform.position;
    gizmoTransform.rotation    = transform.rotation;
    gizmoTransform.scale       = transform.scale;
}

void EditorGizmo::syncToTransform(Core::Components::TransformComponent& transform) {
    transform.position = gizmoTransform.translation;
    transform.rotation = gizmoTransform.rotation;
    transform.scale    = gizmoTransform.scale;
}

bool EditorGizmo::renderAndInteract(const Vector2& mousePos,
    bool                                           mousePressed,
    bool                                           mouseDown,
    bool                                           mouseInViewport,
    float                                          viewportWidth,
    float                                          viewportHeight) {

    // Render and check if gizmo was used
    return DrawGizmo3D(
        static_cast<int>(currentMode),
        &gizmoTransform,
        mousePos,
        mousePressed,
        mouseDown && mouseInViewport,
        viewportWidth,
        viewportHeight);
}

void EditorGizmo::updateHoverState(const Vector2& mousePos,
    float                                         viewportWidth,
    float                                         viewportHeight) {

    gizmoHovered = WouldGizmoHandleInput(
        static_cast<int>(currentMode),
        &gizmoTransform,
        mousePos,
        viewportWidth,
        viewportHeight);
}

} // namespace Linp::Editor
