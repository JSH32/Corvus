#include "editor/gizmo/editor_gizmo.hpp"

#include "corvus/application.hpp"
#include "corvus/files/static_resource_file.hpp"
#include "corvus/log.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"
#include "glm/gtx/quaternion.hpp"
#include <algorithm>

namespace Corvus::Editor {

EditorGizmo::EditorGizmo(Graphics::GraphicsContext& context) : ctx(context) { }

EditorGizmo::~EditorGizmo() { shutdown(); }

void EditorGizmo::initialize() {
    using namespace Core;

    auto vsBytes = StaticResourceFile::create("engine/shaders/gizmo/gizmo.vert")->readAllBytes();
    auto fsBytes = StaticResourceFile::create("engine/shaders/gizmo/gizmo.frag")->readAllBytes();
    const std::string vsSrc(vsBytes.begin(), vsBytes.end());
    const std::string fsSrc(fsBytes.begin(), fsBytes.end());
    shader = ctx.createShader(vsSrc, fsSrc);

    // Pre-allocate large buffers for dynamic geometry
    const std::vector<GizmoVertex> emptyVertices(10000);
    const std::vector<uint16_t>    emptyIndices(30000);

    vbo = ctx.createVertexBuffer(emptyVertices.data(), emptyVertices.size() * sizeof(GizmoVertex));
    ibo = ctx.createIndexBuffer(emptyIndices.data(), emptyIndices.size(), true);
    vao = ctx.createVertexArray();

    Graphics::VertexBufferLayout layout;
    layout.push<float>(3); // position
    layout.push<float>(4); // color
    vao.addVertexBuffer(vbo, layout);
    vao.setIndexBuffer(ibo);
}

void EditorGizmo::shutdown() {
    if (shader.valid())
        shader.release();
    if (vao.valid())
        vao.release();
    if (vbo.valid())
        vbo.release();
    if (ibo.valid())
        ibo.release();
}

void EditorGizmo::setColors(const glm::vec4& x,
                            const glm::vec4& y,
                            const glm::vec4& z,
                            const glm::vec4& center) {
    axisColors[0] = x;
    axisColors[1] = y;
    axisColors[2] = z;
    centerColor   = center;
}

void EditorGizmo::setGlobalAxes(const glm::vec3& right,
                                const glm::vec3& up,
                                const glm::vec3& forward) {
    globalAxes[0] = glm::normalize(right);
    globalAxes[1] = glm::normalize(up);
    globalAxes[2] = glm::normalize(forward);
}

void EditorGizmo::computeAxisOrientation(const glm::mat4& view, const glm::vec3& camPos) {
    cameraRight   = glm::vec3(view[0][0], view[1][0], view[2][0]);
    cameraUp      = glm::vec3(view[0][1], view[1][1], view[2][1]);
    cameraForward = glm::normalize(position - camPos);

    if (Mode activeMode = currentMode;
        static_cast<uint8_t>(activeMode) & static_cast<uint8_t>(Mode::Scale)) {
        orientation = Orientation::Local;
    }

    if (orientation == Orientation::View) {
        currentAxes[0] = cameraRight;
        currentAxes[1] = cameraUp;
        currentAxes[2] = cameraForward;
    } else {
        currentAxes[0] = globalAxes[0];
        currentAxes[1] = globalAxes[1];
        currentAxes[2] = globalAxes[2];

        if (orientation == Orientation::Local) {
            for (auto& currentAxe : currentAxes) {
                currentAxe = glm::normalize(rotation * currentAxe);
            }
        }
    }
}

void EditorGizmo::computeRay(const glm::vec2& mousePos,
                             const glm::mat4& invViewProj,
                             float            w,
                             float            h,
                             glm::vec3&       outOrigin,
                             glm::vec3&       outDir) {
    const glm::vec2 ndc = { 2.0f * mousePos.x / w - 1.0f, 1.0f - 2.0f * mousePos.y / h };

    // Near and far points for direction
    glm::vec4 nearP = invViewProj * glm::vec4(ndc, 0.0f, 1.0f);
    glm::vec4 farP  = invViewProj * glm::vec4(ndc, 1.0f, 1.0f);

    // Camera plane pointer position
    glm::vec4 camPlaneP = invViewProj * glm::vec4(ndc, -1.0f, 1.0f);

    nearP /= nearP.w;
    farP /= farP.w;
    camPlaneP /= camPlaneP.w;

    outOrigin = glm::vec3(camPlaneP);
    outDir    = glm::normalize(glm::vec3(farP - nearP));
}
glm::vec3 EditorGizmo::getWorldMouse(const glm::vec2& mousePos) const {
    const float dist = glm::distance(cameraPosition, position);

    glm::vec3 rayOrigin, rayDir;
    computeRay(mousePos, glm::inverse(lastViewProj), viewportW, viewportH, rayOrigin, rayDir);

    return rayOrigin + rayDir * dist;
}

bool EditorGizmo::checkRaySphere(const glm::vec3& rayOrigin,
                                 const glm::vec3& rayDir,
                                 const glm::vec3& center,
                                 float            radius) {
    const glm::vec3 oc           = rayOrigin - center;
    const float     a            = glm::dot(rayDir, rayDir);
    const float     b            = 2.0f * glm::dot(oc, rayDir);
    const float     c            = glm::dot(oc, oc) - radius * radius;
    const float     discriminant = b * b - 4 * a * c;

    if (discriminant < 0)
        return false;

    const float t = (-b - sqrt(discriminant)) / (2.0f * a);
    return t >= 0;
}

bool EditorGizmo::checkRayQuad(const glm::vec3& rayOrigin,
                               const glm::vec3& rayDir,
                               const glm::vec3& a,
                               const glm::vec3& b,
                               const glm::vec3& c,
                               const glm::vec3& d) {
    auto checkTriangle
        = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) -> bool {
        const glm::vec3 e1  = v1 - v0;
        const glm::vec3 e2  = v2 - v0;
        const glm::vec3 h   = glm::cross(rayDir, e2);
        const float     det = glm::dot(e1, h);

        if (abs(det) < 1e-8f)
            return false;

        const float     invDet = 1.0f / det;
        const glm::vec3 s      = rayOrigin - v0;
        const float     u      = invDet * glm::dot(s, h);

        if (u < 0.0f || u > 1.0f)
            return false;

        const glm::vec3 q = glm::cross(s, e1);

        if (const float v = invDet * glm::dot(rayDir, q); v < 0.0f || u + v > 1.0f)
            return false;

        const float t = invDet * glm::dot(e2, q);
        return t > 1e-8f;
    };

    return checkTriangle(a, b, c) || checkTriangle(a, c, d);
}

bool EditorGizmo::checkOrientedBoundingBox(const glm::vec3& rayOrigin,
                                           const glm::vec3& rayDir,
                                           const glm::vec3& obbCenter,
                                           const glm::vec3& obbHalfSize) const {
    glm::vec3 oLocal = rayOrigin - obbCenter;

    glm::vec3 localRayOrigin;
    localRayOrigin.x = glm::dot(oLocal, currentAxes[0]);
    localRayOrigin.y = glm::dot(oLocal, currentAxes[1]);
    localRayOrigin.z = glm::dot(oLocal, currentAxes[2]);

    glm::vec3 localRayDir;
    localRayDir.x = glm::dot(rayDir, currentAxes[0]);
    localRayDir.y = glm::dot(rayDir, currentAxes[1]);
    localRayDir.z = glm::dot(rayDir, currentAxes[2]);

    // Avoid division by zero
    glm::vec3 invDir;
    invDir.x = (fabs(localRayDir.x) > 1e-8f) ? 1.0f / localRayDir.x : 1e8f;
    invDir.y = (fabs(localRayDir.y) > 1e-8f) ? 1.0f / localRayDir.y : 1e8f;
    invDir.z = (fabs(localRayDir.z) > 1e-8f) ? 1.0f / localRayDir.z : 1e8f;

    glm::vec3 t0 = (-obbHalfSize - localRayOrigin) * invDir;
    glm::vec3 t1 = (obbHalfSize - localRayOrigin) * invDir;

    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);

    float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float tFar  = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

    return tNear <= tFar && tFar >= 0;
}

bool EditorGizmo::checkAxis(const int        axis,
                            const glm::vec3& rayOrigin,
                            const glm::vec3& rayDir,
                            const Mode       type) {
    // The OBB should span the ENTIRE arrow from base to tip (giggity)
    float lengthHalf = actualGizmoSize * 0.5f; // Half the full length

    // Width/height of the arrow shaft
    const float widthHalf = actualGizmoSize * arrowWidthFactor * 0.5f;

    glm::vec3 halfDim;
    halfDim[axis]           = lengthHalf;
    halfDim[(axis + 1) % 3] = widthHalf * 2.0f; // Make 2x wider for easier selection
    halfDim[(axis + 2) % 3] = widthHalf * 2.0f;

    const bool hasBothScaleTranslate
        = (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Translate))
        && (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Scale));

    if (type == Mode::Scale && hasBothScaleTranslate) {
        halfDim[axis] *= 0.5f;
        lengthHalf *= 0.5f;
    }

    const glm::vec3 obbCenter = position + currentAxes[axis] * lengthHalf;

    return checkOrientedBoundingBox(rayOrigin, rayDir, obbCenter, halfDim);
}

bool EditorGizmo::checkPlane(const int        lockedAxis,
                             const glm::vec3& rayOrigin,
                             const glm::vec3& rayDir) const {
    const glm::vec3 dir1 = currentAxes[(lockedAxis + 1) % 3];
    const glm::vec3 dir2 = currentAxes[(lockedAxis + 2) % 3];

    const float offset = planeOffsetFactor * actualGizmoSize;
    const float size   = planeSizeFactor * actualGizmoSize;

    const glm::vec3 a = position + dir1 * offset + dir2 * offset;
    const glm::vec3 b = a + dir1 * size;
    const glm::vec3 c = b + dir2 * size;
    const glm::vec3 d = a + dir2 * size;

    return checkRayQuad(rayOrigin, rayDir, a, b, c, d);
}

bool EditorGizmo::checkCircle(const int        axis,
                              const glm::vec3& rayOrigin,
                              const glm::vec3& rayDir) const {
    const glm::vec3 dir1 = currentAxes[(axis + 1) % 3];
    const glm::vec3 dir2 = currentAxes[(axis + 2) % 3];

    const float   circleRadius = actualGizmoSize;
    constexpr int angleStep    = 10;
    const float   sphereRadius
        = circleRadius * sin(glm::radians(static_cast<float>(angleStep) / 2.0f));

    for (int i = 0; i < 360; i += angleStep) {
        const float angle = glm::radians(static_cast<float>(i));
        glm::vec3   p     = position + dir1 * glm::sin(angle) * circleRadius
            + dir2 * glm::cos(angle) * circleRadius;

        if (checkRaySphere(rayOrigin, rayDir, p, sphereRadius)) {
            return true;
        }
    }

    return false;
}

bool EditorGizmo::checkCenter(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const {
    return checkRaySphere(rayOrigin, rayDir, position, actualGizmoSize * circleRadiusFactor);
}

glm::vec3 EditorGizmo::projectOntoAxis(const glm::vec3& vec, const glm::vec3& axis) {
    return axis * glm::dot(vec, axis);
}

bool EditorGizmo::isAxisActive(const int axis) const {
    return (axis == 0 && (activeAxis & X)) || (axis == 1 && (activeAxis & Y))
        || (axis == 2 && (activeAxis & Z));
}

void EditorGizmo::beginTransform(const Action action, const unsigned char axis) {
    currentAction = action;
    activeAxis    = axis;

    startPosition = position;
    startRotation = rotation;
    startScale    = scale;

    dragStartWorld = getWorldMouse(lastMousePos);
}

void EditorGizmo::applyTransform() {
    const glm::vec3 currentWorldMouse = getWorldMouse(lastMousePos);
    const glm::vec3 delta             = currentWorldMouse - dragStartWorld;

    switch (currentAction) {
        case Action::Translate: {
            position = startPosition;

            if (activeAxis == XYZ) {
                position += projectOntoAxis(delta, cameraRight);
                position += projectOntoAxis(delta, cameraUp);
            } else {
                if (activeAxis & X)
                    position += projectOntoAxis(delta, currentAxes[0]);
                if (activeAxis & Y)
                    position += projectOntoAxis(delta, currentAxes[1]);
                if (activeAxis & Z)
                    position += projectOntoAxis(delta, currentAxes[2]);
            }
            break;
        }

        case Action::Scale: {
            scale = startScale;

            if (activeAxis == XYZ) {
                const float scaleDelta = glm::dot(delta, globalAxes[0]);
                scale += glm::vec3(scaleDelta);
            } else {
                if (activeAxis & X)
                    scale += projectOntoAxis(delta, globalAxes[0]);
                if (activeAxis & Y)
                    scale += projectOntoAxis(delta, globalAxes[1]);
                if (activeAxis & Z)
                    scale += projectOntoAxis(delta, globalAxes[2]);
            }

            scale = glm::max(scale, glm::vec3(0.001f));
            break;
        }

        case Action::Rotate: {
            rotation = startRotation;

            const float deltaAngle = glm::clamp(glm::dot(delta, cameraRight + cameraUp),
                                                -2.0f * glm::pi<float>(),
                                                2.0f * glm::pi<float>());

            if (activeAxis & X)
                rotation = glm::angleAxis(deltaAngle, currentAxes[0]) * rotation;
            if (activeAxis & Y)
                rotation = glm::angleAxis(deltaAngle, currentAxes[1]) * rotation;
            if (activeAxis & Z)
                rotation = glm::angleAxis(deltaAngle, currentAxes[2]) * rotation;

            startRotation  = rotation;
            dragStartWorld = currentWorldMouse;
            break;
        }

        default:
            break;
    }
}

void EditorGizmo::endTransform() {
    currentAction   = Action::None;
    activeAxis      = None;
    activeTransform = nullptr;
}

void EditorGizmo::handleInput(const glm::vec2& mousePos,
                              const bool       mousePressed,
                              const bool       mouseDown) {
    if (currentAction != Action::None) {
        if (!mouseDown) {
            endTransform();
        } else {
            glm::vec3 rayOrigin, rayDir;
            computeRay(
                mousePos, glm::inverse(lastViewProj), viewportW, viewportH, rayOrigin, rayDir);
            applyTransform();
        }
    } else {
        hoveredAxis = None;

        if (mousePressed) {
            glm::vec3 rayOrigin, rayDir;
            computeRay(
                mousePos, glm::inverse(lastViewProj), viewportW, viewportH, rayOrigin, rayDir);

            int  hit    = -1;
            auto action = Action::None;

            for (int k = 0; hit == -1 && k < 2; ++k) {
                Mode         gizmoMode   = k == 0 ? Mode::Scale : Mode::Translate;
                const Action gizmoAction = k == 0 ? Action::Scale : Action::Translate;

                if (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(gizmoMode)) {
                    if (checkCenter(rayOrigin, rayDir)) {
                        action = gizmoAction;
                        hit    = 6;
                        break;
                    }

                    for (int i = 0; i < 3; ++i) {
                        if (checkAxis(i, rayOrigin, rayDir, gizmoMode)) {
                            action = gizmoAction;
                            hit    = i;
                            break;
                        }
                        if (checkPlane(i, rayOrigin, rayDir)) {
                            const bool hasBoth = (static_cast<uint8_t>(currentMode)
                                                  & static_cast<uint8_t>(Mode::Scale))
                                && (static_cast<uint8_t>(currentMode)
                                    & static_cast<uint8_t>(Mode::Translate));
                            action = hasBoth ? Action::Translate : gizmoAction;
                            hit    = 3 + i;
                            break;
                        }
                    }
                }
            }

            if (hit == -1
                && static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Rotate)) {
                for (int i = 0; i < 3; ++i) {
                    if (checkCircle(i, rayOrigin, rayDir)) {
                        action = Action::Rotate;
                        hit    = i;
                        break;
                    }
                }
            }

            if (hit >= 0) {
                uint8_t axis = None;
                switch (hit) {
                    case 0:
                        axis = X;
                        break;
                    case 1:
                        axis = Y;
                        break;
                    case 2:
                        axis = Z;
                        break;
                    case 3:
                        axis = YZ;
                        break;
                    case 4:
                        axis = XZ;
                        break;
                    case 5:
                        axis = XY;
                        break;
                    case 6:
                        axis = XYZ;
                        break;
                    default:;
                }

                beginTransform(action, axis);
            }
        }
    }
}

void EditorGizmo::buildTranslateArrow(std::vector<GizmoVertex>& triVerts,
                                      std::vector<uint16_t>&    triIndices,
                                      std::vector<GizmoVertex>& lineVerts,
                                      std::vector<uint16_t>&    lineIndices,
                                      int                       axis) {
    if (currentAction != Action::None
        && (!isAxisActive(axis) || currentAction != Action::Translate)) {
        return;
    }

    glm::vec4 color  = axisColors[axis];
    glm::vec3 endPos = position + currentAxes[axis] * actualGizmoSize * (1.0f - arrowLengthFactor);

    // Line from origin to arrow base (LINES)
    if (!(static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Scale))) {
        uint16_t lineBaseIdx = lineVerts.size();
        lineVerts.push_back({ position, color });
        lineVerts.push_back({ endPos, color });
        lineIndices.push_back(lineBaseIdx + 0);
        lineIndices.push_back(lineBaseIdx + 1);
    }

    // Arrow head (cone as pyramid) - TRIANGLES
    uint16_t triBaseIdx = triVerts.size();

    float arrowLength = actualGizmoSize * arrowLengthFactor;
    float arrowWidth  = actualGizmoSize * arrowWidthFactor;

    glm::vec3 dim1 = currentAxes[(axis + 1) % 3] * arrowWidth;
    glm::vec3 dim2 = currentAxes[(axis + 2) % 3] * arrowWidth;
    glm::vec3 n    = currentAxes[axis];

    glm::vec3 v = endPos + n * arrowLength;
    glm::vec3 a = endPos - dim1 * 0.5f - dim2 * 0.5f;
    glm::vec3 b = a + dim1;
    glm::vec3 c = b + dim2;
    glm::vec3 d = a + dim2;

    triVerts.push_back({ a, color });
    triVerts.push_back({ b, color });
    triVerts.push_back({ c, color });
    triVerts.push_back({ d, color });
    triVerts.push_back({ v, color });

    // Base (2 triangles)
    triIndices.push_back(triBaseIdx + 0);
    triIndices.push_back(triBaseIdx + 1);
    triIndices.push_back(triBaseIdx + 2);
    triIndices.push_back(triBaseIdx + 0);
    triIndices.push_back(triBaseIdx + 2);
    triIndices.push_back(triBaseIdx + 3);

    // 4 sides
    triIndices.push_back(triBaseIdx + 0);
    triIndices.push_back(triBaseIdx + 4);
    triIndices.push_back(triBaseIdx + 1);

    triIndices.push_back(triBaseIdx + 1);
    triIndices.push_back(triBaseIdx + 4);
    triIndices.push_back(triBaseIdx + 2);

    triIndices.push_back(triBaseIdx + 2);
    triIndices.push_back(triBaseIdx + 4);
    triIndices.push_back(triBaseIdx + 3);

    triIndices.push_back(triBaseIdx + 3);
    triIndices.push_back(triBaseIdx + 4);
    triIndices.push_back(triBaseIdx + 0);
}

void EditorGizmo::buildScaleCube(std::vector<GizmoVertex>& triVerts,
                                 std::vector<uint16_t>&    triIndices,
                                 std::vector<GizmoVertex>& lineVerts,
                                 std::vector<uint16_t>&    lineIndices,
                                 int                       axis) {
    if (currentAction != Action::None && (!isAxisActive(axis) || currentAction != Action::Scale)) {
        return;
    }

    bool hasBoth = (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Translate))
        && (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Scale));

    float gizmoSize = hasBoth ? actualGizmoSize * 0.5f : actualGizmoSize;

    glm::vec4 color  = axisColors[axis];
    glm::vec3 endPos = position + currentAxes[axis] * gizmoSize * (1.0f - arrowWidthFactor);

    // Line from origin to cube (LINES)
    uint16_t lineBaseIdx = lineVerts.size();
    lineVerts.push_back({ position, color });
    lineVerts.push_back({ endPos, color });
    lineIndices.push_back(lineBaseIdx + 0);
    lineIndices.push_back(lineBaseIdx + 1);

    // Cube at end (TRIANGLES)
    uint16_t triBaseIdx = triVerts.size();

    float     boxSize = actualGizmoSize * arrowWidthFactor;
    glm::vec3 dim1    = currentAxes[(axis + 1) % 3] * boxSize;
    glm::vec3 dim2    = currentAxes[(axis + 2) % 3] * boxSize;
    glm::vec3 depth   = currentAxes[axis] * boxSize;

    glm::vec3 a = endPos - dim1 * 0.5f - dim2 * 0.5f;
    glm::vec3 b = a + dim1;
    glm::vec3 c = b + dim2;
    glm::vec3 d = a + dim2;
    glm::vec3 e = a + depth;
    glm::vec3 f = b + depth;
    glm::vec3 g = c + depth;
    glm::vec3 h = d + depth;

    triVerts.push_back({ a, color });
    triVerts.push_back({ b, color });
    triVerts.push_back({ c, color });
    triVerts.push_back({ d, color });
    triVerts.push_back({ e, color });
    triVerts.push_back({ f, color });
    triVerts.push_back({ g, color });
    triVerts.push_back({ h, color });

    // 6 faces of cube
    auto addQuad = [&](const uint16_t i0, const uint16_t i1, const uint16_t i2, const uint16_t i3) {
        triIndices.push_back(triBaseIdx + i0);
        triIndices.push_back(triBaseIdx + i1);
        triIndices.push_back(triBaseIdx + i2);
        triIndices.push_back(triBaseIdx + i0);
        triIndices.push_back(triBaseIdx + i2);
        triIndices.push_back(triBaseIdx + i3);
    };

    addQuad(0, 1, 2, 3);
    addQuad(4, 5, 6, 7);
    addQuad(0, 4, 5, 1);
    addQuad(1, 5, 6, 2);
    addQuad(2, 6, 7, 3);
    addQuad(3, 7, 4, 0);
}

void EditorGizmo::buildRotateCircle(std::vector<GizmoVertex>& lineVertices,
                                    std::vector<uint16_t>&    lineIndices,
                                    const int                 axis) const {
    if (currentAction != Action::None && (!isAxisActive(axis) || currentAction != Action::Rotate)) {
        return;
    }

    const uint16_t baseIdx = lineVertices.size();

    const glm::vec4 color = axisColors[axis];
    const glm::vec3 dir1  = currentAxes[(axis + 1) % 3];
    const glm::vec3 dir2  = currentAxes[(axis + 2) % 3];

    const float   radius    = actualGizmoSize;
    constexpr int angleStep = 10;

    for (int i = 0; i < 360; i += angleStep) {
        const float angle1 = glm::radians(static_cast<float>(i));
        const float angle2 = glm::radians(static_cast<float>(i + angleStep));

        const glm::vec3 p1
            = position + dir1 * glm::sin(angle1) * radius + dir2 * glm::cos(angle1) * radius;
        const glm::vec3 p2
            = position + dir1 * glm::sin(angle2) * radius + dir2 * glm::cos(angle2) * radius;

        lineVertices.push_back({ p1, color });
        lineVertices.push_back({ p2, color });

        uint16_t idx = baseIdx + (i / angleStep) * 2;
        lineIndices.push_back(idx);
        lineIndices.push_back(idx + 1);
    }
}

void EditorGizmo::buildPlane(std::vector<GizmoVertex>& triVertices,
                             std::vector<uint16_t>&    triIndices,
                             std::vector<GizmoVertex>& lineVertices,
                             std::vector<uint16_t>&    lineIndices,
                             int                       lockedAxis) const {
    if (currentAction != Action::None) {
        return;
    }

    glm::vec3 dir1  = currentAxes[(lockedAxis + 1) % 3];
    glm::vec3 dir2  = currentAxes[(lockedAxis + 2) % 3];
    glm::vec4 color = axisColors[lockedAxis];
    color.a *= 0.5f;

    float offset = planeOffsetFactor * actualGizmoSize;
    float size   = planeSizeFactor * actualGizmoSize;

    glm::vec3 a = position + dir1 * offset + dir2 * offset;
    glm::vec3 b = a + dir1 * size;
    glm::vec3 c = b + dir2 * size;
    glm::vec3 d = a + dir2 * size;

    // Filled quad (TRIANGLES)
    uint16_t triBaseIdx = triVertices.size();
    triVertices.push_back({ a, color });
    triVertices.push_back({ b, color });
    triVertices.push_back({ c, color });
    triVertices.push_back({ d, color });

    triIndices.push_back(triBaseIdx + 0);
    triIndices.push_back(triBaseIdx + 1);
    triIndices.push_back(triBaseIdx + 2);
    triIndices.push_back(triBaseIdx + 0);
    triIndices.push_back(triBaseIdx + 2);
    triIndices.push_back(triBaseIdx + 3);

    // Outline (LINES)
    color.a              = 1.0f;
    uint16_t lineBaseIdx = lineVertices.size();

    lineVertices.push_back({ a, color });
    lineVertices.push_back({ b, color });
    lineVertices.push_back({ c, color });
    lineVertices.push_back({ d, color });

    lineIndices.push_back(lineBaseIdx + 0);
    lineIndices.push_back(lineBaseIdx + 1);
    lineIndices.push_back(lineBaseIdx + 1);
    lineIndices.push_back(lineBaseIdx + 2);
    lineIndices.push_back(lineBaseIdx + 2);
    lineIndices.push_back(lineBaseIdx + 3);
    lineIndices.push_back(lineBaseIdx + 3);
    lineIndices.push_back(lineBaseIdx + 0);
}

void EditorGizmo::buildCenter(std::vector<GizmoVertex>& lineVertices,
                              std::vector<uint16_t>&    lineIndices) const {
    const uint16_t baseIdx = lineVertices.size();

    const float   radius    = actualGizmoSize * circleRadiusFactor;
    constexpr int angleStep = 15;

    for (int i = 0; i < 360; i += angleStep) {
        const float angle1 = glm::radians(static_cast<float>(i));
        const float angle2 = glm::radians(static_cast<float>(i + angleStep));

        const glm::vec3 p1 = position + cameraRight * glm::sin(angle1) * radius
            + cameraUp * glm::cos(angle1) * radius;
        const glm::vec3 p2 = position + cameraRight * glm::sin(angle2) * radius
            + cameraUp * glm::cos(angle2) * radius;

        lineVertices.push_back({ p1, centerColor });
        lineVertices.push_back({ p2, centerColor });

        uint16_t idx = baseIdx + (i / angleStep) * 2;
        lineIndices.push_back(idx);
        lineIndices.push_back(idx + 1);
    }
}

void EditorGizmo::syncFromTransform(const Core::Components::TransformComponent& transform) {
    position = transform.position;
    rotation = transform.rotation;
    scale    = transform.scale;
}

void EditorGizmo::syncToTransform(Core::Components::TransformComponent& transform) const {
    transform.position = position;
    transform.rotation = rotation;
    transform.scale    = scale;
}

// In editor_gizmo.cpp
bool EditorGizmo::render(Graphics::CommandBuffer&              cmd,
                         Core::Components::TransformComponent& transform,
                         const glm::vec2&                      mousePos,
                         const bool                            mousePressed,
                         const bool                            mouseDown,
                         const float                           vw,
                         const float                           vh,
                         const glm::mat4&                      view,
                         const glm::mat4&                      proj,
                         const glm::vec3&                      camPos) {
    if (!enabled)
        return false;

    viewportW      = vw;
    viewportH      = vh;
    lastMousePos   = mousePos;
    cameraPosition = camPos;
    lastViewProj   = proj * view;

    syncFromTransform(transform);

    actualGizmoSize = baseGizmoSize * glm::distance(camPos, position) * 0.1f;
    computeAxisOrientation(view, camPos);

    std::vector<GizmoVertex> triangleVertices;
    std::vector<uint16_t>    triangleIndices;
    std::vector<GizmoVertex> lineVertices;
    std::vector<uint16_t>    lineIndices;

    for (int i = 0; i < 3; i++) {
        if (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Translate)) {
            buildTranslateArrow(triangleVertices, triangleIndices, lineVertices, lineIndices, i);
        }
        if (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Scale)) {
            buildScaleCube(triangleVertices, triangleIndices, lineVertices, lineIndices, i);
        }
        if ((static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Scale))
            || (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Translate))) {
            buildPlane(triangleVertices, triangleIndices, lineVertices, lineIndices, i);
        }
        if (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Rotate)) {
            buildRotateCircle(lineVertices, lineIndices, i);
        }
    }

    if ((static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Scale))
        || (static_cast<uint8_t>(currentMode) & static_cast<uint8_t>(Mode::Translate))) {
        buildCenter(lineVertices, lineIndices);
    }

    cmd.setShader(shader);
    cmd.setDepthTest(false);
    cmd.setBlendState(true);
    cmd.setCullFace(false, false);
    shader.setMat4(cmd, "u_ViewProjection", proj * view);
    cmd.setVertexArray(vao);

    if (!triangleVertices.empty() && !triangleIndices.empty()) {
        vbo.setData(cmd, triangleVertices.data(), triangleVertices.size() * sizeof(GizmoVertex));
        ibo.setData(cmd, triangleIndices.data(), triangleIndices.size(), true);
        cmd.drawIndexed(triangleIndices.size(), true, 0, Graphics::PrimitiveType::Triangles);
    }

    if (!lineVertices.empty() && !lineIndices.empty()) {
        vbo.setData(cmd, lineVertices.data(), lineVertices.size() * sizeof(GizmoVertex));
        ibo.setData(cmd, lineIndices.data(), lineIndices.size(), true);
        cmd.setLineWidth(lineWidth);
        cmd.drawIndexed(lineIndices.size(), true, 0, Graphics::PrimitiveType::Lines);
    }

    cmd.setCullFace(true, false);

    if (activeTransform == nullptr || activeTransform == &transform) {
        handleInput(mousePos, mousePressed, mouseDown);

        if (currentAction != Action::None) {
            activeTransform = &transform;
        }
    }

    syncToTransform(transform);

    // Return true if this gizmo is currently transforming
    return currentAction != Action::None && activeTransform == &transform;
}

}
