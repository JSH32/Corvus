#pragma once

#include "imgui.h"
#include "raylib-cpp.hpp"

namespace Linp::Editor {

/**
 * @class EditorCamera
 * @brief A 3D orbit camera for editor viewports with mouse controls.
 *
 * Provides orbit, pan, and zoom functionality with configurable constraints.
 * Designed to be embedded in editor panels that need 3D camera control.
 */
class EditorCamera {
public:
    // Camera control constants
    static constexpr float DEFAULT_MIN_DISTANCE     = 1.0f;
    static constexpr float DEFAULT_MAX_DISTANCE     = 100.0f;
    static constexpr float DEFAULT_ZOOM_SPEED       = 1.0f;
    static constexpr float DEFAULT_ORBIT_SPEED      = 0.005f;
    static constexpr float DEFAULT_PAN_SPEED_FACTOR = 0.002f;
    static constexpr float DEFAULT_PITCH_MIN        = -PI / 2.0f + 0.01f;
    static constexpr float DEFAULT_PITCH_MAX        = PI / 2.0f - 0.01f;
    static constexpr float DEFAULT_FOV              = 45.0f;

    /**
     * @brief Constructs an EditorCamera with default settings.
     */
    EditorCamera();

    /**
     * @brief Constructs an EditorCamera with custom parameters.
     */
    EditorCamera(const raylib::Vector3& target, float distance = 10.0f,
        const raylib::Vector2& orbitAngles = { 0.45f, -0.45f });

    /**
     * @brief Updates camera based on input. Call this each frame.
     * @param io ImGui IO for input state
     * @param isInputAllowed Whether camera should respond to input
     * @return True if camera was modified this frame
     */
    bool update(const ImGuiIO& io, bool isInputAllowed = true);

    /**
     * @brief Gets the Raylib camera for rendering.
     */
    const raylib::Camera3D& getCamera() const { return camera; }
    raylib::Camera3D&       getCamera() { return camera; }

    // Target manipulation
    const raylib::Vector3& getTarget() const { return target; }
    void                   setTarget(const raylib::Vector3& newTarget);

    // Distance controls
    float getDistance() const { return distance; }
    void  setDistance(float newDistance);

    // Orbit angle controls (pitch, yaw in radians)
    const raylib::Vector2& getOrbitAngles() const { return orbitAngles; }
    void                   setOrbitAngles(const raylib::Vector2& angles);

    // Configuration
    void setDistanceConstraints(float minDist, float maxDist);
    void setPitchConstraints(float minPitch, float maxPitch);
    void setSpeeds(float zoomSpeed, float orbitSpeed, float panSpeedFactor);

    /**
     * @brief Resets camera to default position and orientation.
     */
    void reset();

    /**
     * @brief Focuses the camera on a point at optimal distance.
     * @param focusPoint Point to focus on
     * @param optimalDistance Desired distance from focus point
     */
    void focusOn(const Vector3& focusPoint, float optimalDistance = 10.0f);

private:
    raylib::Camera3D camera;

    // Camera state
    raylib::Vector3 target;
    raylib::Vector2 orbitAngles; // x: pitch, y: yaw (radians)
    float           distance;

    // Configuration
    float minDistance, maxDistance;
    float pitchMin, pitchMax;
    float zoomSpeed, orbitSpeed, panSpeedFactor;

    /**
     * @brief Recalculates camera position based on current orbit state.
     */
    void updateCameraVectors();

    /**
     * @brief Processes mouse wheel zoom input.
     */
    bool processZoom(float wheelDelta);

    /**
     * @brief Processes right mouse button orbit input.
     */
    bool processOrbit(const raylib::Vector2& mouseDelta);

    /**
     * @brief Processes middle mouse button pan input.
     */
    bool processPan(const raylib::Vector2& mouseDelta);
};

}
