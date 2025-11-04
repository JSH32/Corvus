#pragma once
#include "corvus/components/transform.hpp"
#include "corvus/graphics/graphics.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>

namespace Corvus::Editor {

/**
 * @class EditorGizmo
 * @brief Handles 3D gizmo rendering and manipulation for transforms.
 */
class EditorGizmo {
public:
    explicit EditorGizmo(Graphics::GraphicsContext& context);
    ~EditorGizmo();

    void initialize();
    void shutdown();

    enum class Mode : uint8_t {
        Translate = 1 << 0,
        Rotate    = 1 << 1,
        Scale     = 1 << 2,
        All       = Translate | Rotate | Scale
    };

    enum class Orientation : uint8_t {
        Global,
        Local,
        View
    };

    enum ActiveAxis : uint8_t {
        None = 0,
        X    = 1 << 0,
        Y    = 1 << 1,
        Z    = 1 << 2,
        XY   = X | Y,
        XZ   = X | Z,
        YZ   = Y | Z,
        XYZ  = X | Y | Z
    };

    enum class Action : uint8_t {
        None,
        Translate,
        Rotate,
        Scale
    };

    bool render(Graphics::CommandBuffer&              cmd,
                Core::Components::TransformComponent& transform,
                const glm::vec2&                      mousePos,
                bool                                  mousePressed,
                bool                                  mouseDown,
                float                                 viewportW,
                float                                 viewportH,
                const glm::mat4&                      view,
                const glm::mat4&                      proj,
                const glm::vec3&                      camPos);

    bool isActive() const { return currentAction != Action::None; }
    bool isHovered() const { return hoveredAxis != None; }

    void setMode(Mode mode) { currentMode = mode; }
    Mode getMode() const { return currentMode; }

    void        setOrientation(Orientation orient) { orientation = orient; }
    Orientation getOrientation() const { return orientation; }

    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }

    void  setSize(float size) { baseGizmoSize = glm::max(0.0f, size); }
    float getSize() const { return baseGizmoSize; }

    void  setLineWidth(float width) { lineWidth = glm::max(0.0f, width); }
    float getLineWidth() const { return lineWidth; }

    void
    setColors(const glm::vec4& x, const glm::vec4& y, const glm::vec4& z, const glm::vec4& center);
    void setGlobalAxes(const glm::vec3& right, const glm::vec3& up, const glm::vec3& forward);

private:
    struct GizmoVertex {
        glm::vec3 pos;
        glm::vec4 color;
    };

    void syncFromTransform(const Core::Components::TransformComponent& transform);
    void syncToTransform(Core::Components::TransformComponent& transform);

    void computeAxisOrientation(const glm::mat4& view, const glm::vec3& camPos);

    // Geometry building functions (append to vectors)
    void buildTranslateArrow(std::vector<GizmoVertex>& triVerts,
                             std::vector<uint16_t>&    triIndices,
                             std::vector<GizmoVertex>& lineVerts,
                             std::vector<uint16_t>&    lineIndices,
                             int                       axis);
    void buildScaleCube(std::vector<GizmoVertex>& triVerts,
                        std::vector<uint16_t>&    triIndices,
                        std::vector<GizmoVertex>& lineVerts,
                        std::vector<uint16_t>&    lineIndices,
                        int                       axis);
    void buildRotateCircle(std::vector<GizmoVertex>& lineVerts,
                           std::vector<uint16_t>&    lineIndices,
                           int                       axis);
    void buildPlane(std::vector<GizmoVertex>& triVerts,
                    std::vector<uint16_t>&    triIndices,
                    std::vector<GizmoVertex>& lineVerts,
                    std::vector<uint16_t>&    lineIndices,
                    int                       lockedAxis);
    void buildCenter(std::vector<GizmoVertex>& lineVerts, std::vector<uint16_t>& lineIndices);

    // Collision detection
    bool checkOrientedBoundingBox(const glm::vec3& rayOrigin,
                                  const glm::vec3& rayDir,
                                  const glm::vec3& obbCenter,
                                  const glm::vec3& obbHalfSize);
    bool checkAxis(int axis, const glm::vec3& rayOrigin, const glm::vec3& rayDir, Mode type);
    bool checkPlane(int lockedAxis, const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    bool checkCircle(int axis, const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    bool checkCenter(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    bool checkRayQuad(const glm::vec3& rayOrigin,
                      const glm::vec3& rayDir,
                      const glm::vec3& a,
                      const glm::vec3& b,
                      const glm::vec3& c,
                      const glm::vec3& d);
    bool checkRaySphere(const glm::vec3& rayOrigin,
                        const glm::vec3& rayDir,
                        const glm::vec3& center,
                        float            radius);

    // Input handling
    void handleInput(const glm::vec2& mousePos, bool mousePressed, bool mouseDown);
    void beginTransform(Action           action,
                        uint8_t          axis,
                        const glm::vec3& rayOrigin,
                        const glm::vec3& rayDir);
    void applyTransform(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    void endTransform();

    // Ray utilities
    void      computeRay(const glm::vec2& mousePos,
                         const glm::mat4& invViewProj,
                         float            w,
                         float            h,
                         glm::vec3&       outOrigin,
                         glm::vec3&       outDir);
    glm::vec3 getWorldMouse(const glm::vec2& mousePos);

    // Helper utilities
    bool      isAxisActive(int axis) const;
    glm::vec3 projectOntoAxis(const glm::vec3& vec, const glm::vec3& axis);

    Graphics::GraphicsContext& ctx;

    Graphics::Shader       shader;
    Graphics::VertexArray  vao;
    Graphics::VertexBuffer vbo;
    Graphics::IndexBuffer  ibo;

    glm::vec3 position { 0.0f };
    glm::quat rotation { 1, 0, 0, 0 };
    glm::vec3 scale { 1.0f };

    glm::vec3 startPosition { 0.0f };
    glm::quat startRotation { 1, 0, 0, 0 };
    glm::vec3 startScale { 1.0f };

    glm::vec3 globalAxes[3]  = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    glm::vec3 currentAxes[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    glm::vec4 axisColors[3]
        = { { 0.9f, 0.28f, 0.36f, 1 }, { 0.51f, 0.8f, 0.22f, 1 }, { 0.27f, 0.54f, 0.95f, 1 } };
    glm::vec4 centerColor { 1, 1, 1, 0.78f };

    glm::vec3 cameraRight { 1, 0, 0 };
    glm::vec3 cameraUp { 0, 1, 0 };
    glm::vec3 cameraForward { 0, 0, 1 };

    Mode        currentMode     = Mode::All;
    Orientation orientation     = Orientation::Global;
    bool        enabled         = true;
    float       baseGizmoSize   = 1.5f;
    float       actualGizmoSize = 1.5f;
    float       lineWidth       = 2.5f;

    float arrowLengthFactor  = 0.15f;
    float arrowWidthFactor   = 0.1f;
    float planeOffsetFactor  = 0.3f;
    float planeSizeFactor    = 0.15f;
    float circleRadiusFactor = 0.1f;

    Action    currentAction = Action::None;
    uint8_t   activeAxis    = None;
    uint8_t   hoveredAxis   = None;
    glm::vec3 dragStartWorld { 0.0f };

    glm::mat4 lastViewProj;
    glm::vec2 lastMousePos;
    float     viewportW = 0.0f;
    float     viewportH = 0.0f;
    glm::vec3 cameraPosition { 0.0f };

    Core::Components::TransformComponent* activeTransform = nullptr;
};

}
