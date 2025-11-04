#include "editor/camera/editor_camera.hpp"
#include "imgui_internal.h"
#include <algorithm>

namespace Corvus::Editor {

EditorCamera::EditorCamera()
    : target(0.0f, 0.0f, 0.0f), orbitAngles(0.45f, -0.45f), distance(10.0f),
      minDistance(DEFAULT_MIN_DISTANCE), maxDistance(DEFAULT_MAX_DISTANCE),
      pitchMin(DEFAULT_PITCH_MIN), pitchMax(DEFAULT_PITCH_MAX), zoomSpeed(DEFAULT_ZOOM_SPEED),
      orbitSpeed(DEFAULT_ORBIT_SPEED), panSpeedFactor(DEFAULT_PAN_SPEED_FACTOR) {

    updateCameraVectors();
}

EditorCamera::EditorCamera(const glm::vec3& target, float distance, const glm::vec2& orbitAngles)
    : target(target), orbitAngles(orbitAngles), distance(distance),
      minDistance(DEFAULT_MIN_DISTANCE), maxDistance(DEFAULT_MAX_DISTANCE),
      pitchMin(DEFAULT_PITCH_MIN), pitchMax(DEFAULT_PITCH_MAX), zoomSpeed(DEFAULT_ZOOM_SPEED),
      orbitSpeed(DEFAULT_ORBIT_SPEED), panSpeedFactor(DEFAULT_PAN_SPEED_FACTOR) {

    updateCameraVectors();
}

void EditorCamera::updateCameraVectors() {
    // Calculate camera position from orbit angles and distance
    float cosPitch = std::cos(orbitAngles.x);
    float sinPitch = std::sin(orbitAngles.x);
    float cosYaw   = std::cos(orbitAngles.y);
    float sinYaw   = std::sin(orbitAngles.y);

    glm::vec3 offset(
        cosPitch * sinYaw * distance, sinPitch * distance, cosPitch * cosYaw * distance);

    glm::vec3 position = target + offset;

    // Update the renderer camera
    camera.setPosition(position);
    camera.lookAt(target);
}

glm::vec3 EditorCamera::getForward() const { return glm::normalize(target - camera.getPosition()); }

glm::vec3 EditorCamera::getRight() const {
    glm::vec3 forward = getForward();
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    return glm::normalize(glm::cross(forward, worldUp));
}

glm::vec3 EditorCamera::getUp() const {
    glm::vec3 forward = getForward();
    glm::vec3 right   = getRight();
    return glm::normalize(glm::cross(right, forward));
}

void EditorCamera::setTarget(const glm::vec3& newTarget) {
    target = newTarget;
    updateCameraVectors();
}

void EditorCamera::setDistance(float newDistance) {
    distance = std::clamp(newDistance, minDistance, maxDistance);
    updateCameraVectors();
}

void EditorCamera::setOrbitAngles(const glm::vec2& angles) {
    orbitAngles.x = std::clamp(angles.x, pitchMin, pitchMax);
    orbitAngles.y = angles.y;
    updateCameraVectors();
}

void EditorCamera::setDistanceConstraints(float minDist, float maxDist) {
    minDistance = minDist;
    maxDistance = maxDist;
    distance    = std::clamp(distance, minDistance, maxDistance);
}

void EditorCamera::setPitchConstraints(float minPitch, float maxPitch) {
    pitchMin      = minPitch;
    pitchMax      = maxPitch;
    orbitAngles.x = std::clamp(orbitAngles.x, pitchMin, pitchMax);
}

void EditorCamera::setSpeeds(float zoom, float orbit, float panFactor) {
    zoomSpeed      = zoom;
    orbitSpeed     = orbit;
    panSpeedFactor = panFactor;
}

void EditorCamera::reset() {
    target      = glm::vec3(0.0f);
    orbitAngles = glm::vec2(0.45f, -0.45f);
    distance    = 10.0f;
    updateCameraVectors();
}

void EditorCamera::focusOn(const glm::vec3& focusPoint, float optimalDistance) {
    target   = focusPoint;
    distance = optimalDistance;
    updateCameraVectors();
}

bool EditorCamera::processZoom(float wheelDelta) {
    if (std::abs(wheelDelta) < 0.01f)
        return false;

    distance -= wheelDelta * zoomSpeed;
    distance = std::clamp(distance, minDistance, maxDistance);
    updateCameraVectors();
    return true;
}

bool EditorCamera::processOrbit(const glm::vec2& mouseDelta) {
    if (glm::length(mouseDelta) < 0.01f)
        return false;

    orbitAngles.y += mouseDelta.x * orbitSpeed;
    orbitAngles.x += mouseDelta.y * orbitSpeed;
    orbitAngles.x = std::clamp(orbitAngles.x, pitchMin, pitchMax);

    updateCameraVectors();
    return true;
}

bool EditorCamera::processPan(const glm::vec2& mouseDelta) {
    if (glm::length(mouseDelta) < 0.01f)
        return false;

    glm::vec3 right = getRight();
    glm::vec3 up    = getUp();

    float panSpeed = distance * panSpeedFactor;
    target -= right * mouseDelta.x * panSpeed;
    target += up * mouseDelta.y * panSpeed;

    updateCameraVectors();
    return true;
}

bool EditorCamera::processFlyMode(const ImGuiIO& io, const glm::vec2& mouseDelta) {
    bool modified = false;

    // Mouse look
    if (glm::length(mouseDelta) > 0.01f) {
        orbitAngles.y += mouseDelta.x * orbitSpeed;
        orbitAngles.x += mouseDelta.y * orbitSpeed;
        orbitAngles.x = std::clamp(orbitAngles.x, pitchMin, pitchMax);
        modified      = true;
    }

    // WASD movement
    glm::vec3 forward = getForward();
    glm::vec3 right   = getRight();
    glm::vec3 movement(0.0f);

    if (ImGui::IsKeyDown(ImGuiKey_W))
        movement += forward;
    if (ImGui::IsKeyDown(ImGuiKey_S))
        movement -= forward;
    if (ImGui::IsKeyDown(ImGuiKey_D))
        movement += right;
    if (ImGui::IsKeyDown(ImGuiKey_A))
        movement -= right;
    if (ImGui::IsKeyDown(ImGuiKey_E))
        movement.y += 1.0f;
    if (ImGui::IsKeyDown(ImGuiKey_Q))
        movement.y -= 1.0f;

    if (glm::length(movement) > 0.01f) {
        movement = glm::normalize(movement);
        target += movement * flySpeed * io.DeltaTime;
        modified = true;
    }

    if (modified) {
        updateCameraVectors();
    }

    return modified;
}

bool EditorCamera::update(const ImGuiIO& io, bool isInputAllowed) {
    if (!isInputAllowed)
        return false;

    bool modified = false;

    // Zoom with mouse wheel
    if (std::abs(io.MouseWheel) > 0.01f) {
        modified |= processZoom(io.MouseWheel);
    }

    glm::vec2 mouseDelta(io.MouseDelta.x, io.MouseDelta.y);

    // Right mouse button - orbit or fly mode
    if (io.MouseDown[1]) {
        if (io.KeyShift) {
            // Fly mode with shift
            modified |= processFlyMode(io, mouseDelta);
        } else {
            // Standard orbit
            modified |= processOrbit(mouseDelta);
        }
    }

    // Middle mouse button - pan
    if (io.MouseDown[2]) {
        modified |= processPan(mouseDelta);
    }

    return modified;
}

}
