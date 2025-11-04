#pragma once

#include "corvus/renderer/camera.hpp"
#include "glm/glm.hpp"
#include "imgui.h"

namespace Corvus::Editor {

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
    static constexpr float DEFAULT_PITCH_MIN        = -glm::half_pi<float>() + 0.01f;
    static constexpr float DEFAULT_PITCH_MAX        = glm::half_pi<float>() - 0.01f;
    static constexpr float DEFAULT_FOV              = 45.0f;

    /**
     * @brief Constructs an EditorCamera with default settings.
     */
    EditorCamera();

    /**
     * @brief Constructs an EditorCamera with custom parameters.
     */
    EditorCamera(const glm::vec3& target,
                 float            distance    = 10.0f,
                 const glm::vec2& orbitAngles = { 0.45f, -0.45f });

    /**
     * @brief Updates camera based on input. Call this each frame.
     * @param io ImGui IO for input state
     * @param isInputAllowed Whether camera should respond to input
     * @return True if camera was modified this frame
     */
    bool update(const ImGuiIO& io, bool isInputAllowed = true);

    /**
     * @brief Gets the underlying Renderer::Camera.
     */
    const Renderer::Camera& getCamera() const { return camera; }
    Renderer::Camera&       getCamera() { return camera; }

    // Matrix interface (for backward compatibility)
    glm::mat4 getViewMatrix() const { return camera.getViewMatrix(); }
    glm::mat4 getProjectionMatrix(float aspectRatio) const { return camera.getProjectionMatrix(); }
    glm::vec3 getPosition() const { return camera.getPosition(); }

    // Target manipulation
    const glm::vec3& getTarget() const { return target; }
    void             setTarget(const glm::vec3& newTarget);

    // Distance controls
    float getDistance() const { return distance; }
    void  setDistance(float newDistance);

    // Orbit angle controls (pitch, yaw in radians)
    const glm::vec2& getOrbitAngles() const { return orbitAngles; }
    void             setOrbitAngles(const glm::vec2& angles);

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
    void focusOn(const glm::vec3& focusPoint, float optimalDistance = 10.0f);

    /**
     * @brief Gets the forward direction vector of the camera.
     */
    glm::vec3 getForward() const;

    /**
     * @brief Gets the right direction vector of the camera.
     */
    glm::vec3 getRight() const;

    /**
     * @brief Gets the up direction vector of the camera.
     */
    glm::vec3 getUp() const;

private:
    Renderer::Camera camera;

    // Camera state
    glm::vec3 target;
    glm::vec2 orbitAngles; // x: pitch, y: yaw (radians)
    float     distance;

    // Configuration
    float minDistance, maxDistance;
    float pitchMin, pitchMax;
    float zoomSpeed, orbitSpeed, panSpeedFactor;
    float flySpeed = 5.0f;

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
    bool processOrbit(const glm::vec2& mouseDelta);

    /**
     * @brief Processes middle mouse button pan input.
     */
    bool processPan(const glm::vec2& mouseDelta);

    /**
     * @brief Processes fly mode input (right mouse + WASD).
     */
    bool processFlyMode(const ImGuiIO& io, const glm::vec2& mouseDelta);
};

}
