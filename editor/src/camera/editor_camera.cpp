#include "editor/camera/editor_camera.hpp"
#include "Matrix.hpp"
#include "Vector2.hpp"
#include "Vector3.hpp"
#include <algorithm>
#include <cmath>

namespace Linp::Editor {

EditorCamera::EditorCamera() : EditorCamera({ 0.0f, 0.0f, 0.0f }, 10.0f, { 0.45f, -0.45f }) { }

EditorCamera::EditorCamera(const raylib::Vector3& target,
                           float                  distance,
                           const raylib::Vector2& orbitAngles)
    : target(target), orbitAngles(orbitAngles), distance(distance),
      minDistance(DEFAULT_MIN_DISTANCE), maxDistance(DEFAULT_MAX_DISTANCE),
      pitchMin(DEFAULT_PITCH_MIN), pitchMax(DEFAULT_PITCH_MAX), zoomSpeed(DEFAULT_ZOOM_SPEED),
      orbitSpeed(DEFAULT_ORBIT_SPEED), panSpeedFactor(DEFAULT_PAN_SPEED_FACTOR) {

    // Initialize camera with sensible defaults
    camera.target     = this->target;
    camera.up         = { 0.0f, 1.0f, 0.0f };
    camera.fovy       = DEFAULT_FOV;
    camera.projection = CAMERA_PERSPECTIVE;

    updateCameraVectors();
}

bool EditorCamera::update(const ImGuiIO& io, bool isInputAllowed) {
    if (!isInputAllowed) {
        return false;
    }

    bool cameraChanged = false;

    // Process zoom (mouse wheel)
    if (io.MouseWheel != 0.0f) {
        cameraChanged |= processZoom(io.MouseWheel);
    }

    raylib::Vector2 mouseDelta(io.MouseDelta.x, io.MouseDelta.y);

    // Process orbit (right mouse button)
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        cameraChanged |= processOrbit(mouseDelta);
    }

    // Process pan (middle mouse button)
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        cameraChanged |= processPan(mouseDelta);
    }

    if (cameraChanged) {
        updateCameraVectors();
    }

    return cameraChanged;
}

void EditorCamera::setTarget(const raylib::Vector3& newTarget) {
    target = newTarget;
    updateCameraVectors();
}

void EditorCamera::setDistance(float newDistance) {
    distance = std::clamp(newDistance, minDistance, maxDistance);
    updateCameraVectors();
}

void EditorCamera::setOrbitAngles(const raylib::Vector2& angles) {
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

void EditorCamera::setSpeeds(float zoomSpd, float orbitSpd, float panSpeedFac) {
    zoomSpeed      = zoomSpd;
    orbitSpeed     = orbitSpd;
    panSpeedFactor = panSpeedFac;
}

void EditorCamera::reset() {
    target      = raylib::Vector3(0.0f, 0.0f, 0.0f);
    distance    = 10.0f;
    orbitAngles = raylib::Vector2(0.45f, -0.45f);
    updateCameraVectors();
}

void EditorCamera::focusOn(const Vector3& focusPoint, float optimalDistance) {
    target   = focusPoint;
    distance = std::clamp(optimalDistance, minDistance, maxDistance);
    updateCameraVectors();
}

void EditorCamera::updateCameraVectors() {
    // Calculate camera position using spherical coordinates
    Matrix  rotation = MatrixRotateZYX({ orbitAngles.x, orbitAngles.y, 0.0f });
    Vector3 orbitArm = Vector3Transform({ 0.0f, 0.0f, distance }, rotation);
    camera.position  = Vector3Add(target, orbitArm);
    camera.target    = target;
}

bool EditorCamera::processZoom(float wheelDelta) {
    float oldDistance = distance;
    distance -= wheelDelta * zoomSpeed;
    distance = std::clamp(distance, minDistance, maxDistance);
    return distance != oldDistance;
}

bool EditorCamera::processOrbit(const raylib::Vector2& mouseDelta) {
    Vector2 oldAngles = orbitAngles;
    orbitAngles.y -= mouseDelta.x * orbitSpeed;
    orbitAngles.x -= mouseDelta.y * orbitSpeed;
    orbitAngles.x = std::clamp(orbitAngles.x, pitchMin, pitchMax);
    return (orbitAngles.x != oldAngles.x || orbitAngles.y != oldAngles.y);
}

bool EditorCamera::processPan(const raylib::Vector2& mouseDelta) {
    Vector3 oldTarget = target;

    Matrix  invCameraMat = MatrixInvert(GetCameraMatrix(camera));
    float   panSpeed     = panSpeedFactor * distance;
    Vector3 right        = { invCameraMat.m0, invCameraMat.m1, invCameraMat.m2 };
    Vector3 up           = { invCameraMat.m4, invCameraMat.m5, invCameraMat.m6 };

    target = Vector3Add(target, Vector3Scale(right, -mouseDelta.x * panSpeed));
    target = Vector3Add(target, Vector3Scale(up, mouseDelta.y * panSpeed));

    return !Vector3Equals(target, oldTarget);
}

}
