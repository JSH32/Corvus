#include "linp/systems/shadow_manager.hpp"
#include "linp/asset/asset_manager.hpp"
#include "linp/components/mesh_renderer.hpp"
#include "linp/components/transform.hpp"
#include "linp/log.hpp"

namespace Linp::Core::Systems {

void ShadowMap::initialize(int res) {
    if (initialized && resolution == res)
        return;

    if (initialized) {
        cleanup();
    }

    resolution   = res;
    depthTexture = { 0 };

    depthTexture.id = rlLoadFramebuffer();

    if (depthTexture.id > 0) {
        rlEnableFramebuffer(depthTexture.id);

        depthTexture.texture.width  = resolution;
        depthTexture.texture.height = resolution;

        depthTexture.depth.id      = rlLoadTextureDepth(resolution, resolution, false);
        depthTexture.depth.width   = resolution;
        depthTexture.depth.height  = resolution;
        depthTexture.depth.format  = 19;
        depthTexture.depth.mipmaps = 1;

        rlFramebufferAttach(depthTexture.id,
                            depthTexture.depth.id,
                            RL_ATTACHMENT_DEPTH,
                            RL_ATTACHMENT_TEXTURE2D,
                            0);

        if (rlFramebufferComplete(depthTexture.id)) {
            LINP_INFO("Shadow map framebuffer [ID {}] created successfully (resolution: {}x{})",
                      depthTexture.id,
                      resolution,
                      resolution);
        } else {
            LINP_ERROR("Shadow map framebuffer [ID {}] is incomplete!", depthTexture.id);
        }

        rlDisableFramebuffer();
    } else {
        LINP_ERROR("Failed to create shadow map framebuffer!");
    }

    initialized = true;
}

void ShadowMap::cleanup() {
    if (initialized) {
        if (depthTexture.depth.id > 0) {
            rlUnloadTexture(depthTexture.depth.id);
            depthTexture.depth.id = 0;
        }
        if (depthTexture.id > 0) {
            rlUnloadFramebuffer(depthTexture.id);
            depthTexture.id = 0;
        }
        initialized = false;
        resolution  = 0;
    }
}

ShadowMap::~ShadowMap() { cleanup(); }

// Cubemap shadow map for point lights
void CubemapShadowMap::initialize(int res) {
    if (initialized && resolution == res)
        return;

    if (initialized) {
        cleanup();
    }

    resolution = res;

    // Create cubemap depth texture
    cubemapDepthTexture
        = rlLoadTextureCubemap(nullptr, resolution, RL_PIXELFORMAT_UNCOMPRESSED_R32, 1);

    // Create 6 framebuffers (one for each cubemap face)
    for (int i = 0; i < 6; ++i) {
        faceFramebuffers[i] = rlLoadFramebuffer();

        if (faceFramebuffers[i] > 0) {
            rlEnableFramebuffer(faceFramebuffers[i]);

            // Attach this face of the cubemap as depth attachment
            rlFramebufferAttach(faceFramebuffers[i],
                                cubemapDepthTexture,
                                RL_ATTACHMENT_DEPTH,
                                RL_ATTACHMENT_CUBEMAP_POSITIVE_X + i,
                                0);

            if (!rlFramebufferComplete(faceFramebuffers[i])) {
                LINP_ERROR("Cubemap shadow framebuffer face {} incomplete!", i);
            }

            rlDisableFramebuffer();
        }
    }

    initialized = true;
    LINP_INFO(
        "Cubemap shadow map created successfully (resolution: {}x{})", resolution, resolution);
}

void CubemapShadowMap::cleanup() {
    if (initialized) {
        if (cubemapDepthTexture > 0) {
            rlUnloadTexture(cubemapDepthTexture);
            cubemapDepthTexture = 0;
        }
        for (int i = 0; i < 6; ++i) {
            if (faceFramebuffers[i] > 0) {
                rlUnloadFramebuffer(faceFramebuffers[i]);
                faceFramebuffers[i] = 0;
            }
        }
        initialized = false;
        resolution  = 0;
    }
}

CubemapShadowMap::~CubemapShadowMap() { cleanup(); }

// ShadowManager implementation

ShadowManager::ShadowManager(ShadowManager&& other) noexcept
    : shadowMaps(std::move(other.shadowMaps)),
      cubemapShadowMaps(std::move(other.cubemapShadowMaps)),
      shadowDepthShader(other.shadowDepthShader),
      pointLightShadowShader(other.pointLightShadowShader), initialized(other.initialized) {

    other.initialized               = false;
    other.shadowDepthShader.id      = 0;
    other.pointLightShadowShader.id = 0;
}

ShadowManager& ShadowManager::operator=(ShadowManager&& other) noexcept {
    if (this != &other) {
        cleanup();
        shadowMaps                      = std::move(other.shadowMaps);
        cubemapShadowMaps               = std::move(other.cubemapShadowMaps);
        shadowDepthShader               = other.shadowDepthShader;
        pointLightShadowShader          = other.pointLightShadowShader;
        initialized                     = other.initialized;
        other.initialized               = false;
        other.shadowDepthShader.id      = 0;
        other.pointLightShadowShader.id = 0;
    }
    return *this;
}

void ShadowManager::initialize() {
    if (initialized)
        return;

    // Directional/Spot light shadow shader
    const char* vsDepth = R"(
#version 330
in vec3 vertexPosition;
uniform mat4 lightSpaceMatrix;
uniform mat4 matModel;

void main() {
    gl_Position = lightSpaceMatrix * matModel * vec4(vertexPosition, 1.0);
}
)";

    const char* fsDepth = R"(
#version 330

void main() {
    // Depth written automatically
}
)";

    shadowDepthShader = LoadShaderFromMemory(vsDepth, fsDepth);

    if (shadowDepthShader.id == 0) {
        LINP_ERROR("Failed to load shadow depth shader!");
        return;
    }

    // Point light cubemap shadow shader (without geometry shader)
    // We'll render each face separately in a loop instead
    const char* vsPointDepth = R"(
#version 330
in vec3 vertexPosition;
uniform mat4 lightSpaceMatrix;
uniform mat4 matModel;

out vec3 fragWorldPos;

void main() {
    vec4 worldPos = matModel * vec4(vertexPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    gl_Position = lightSpaceMatrix * worldPos;
}
)";

    const char* fsPointDepth = R"(
#version 330
in vec3 fragWorldPos;

uniform vec3 u_LightPos;
uniform float u_FarPlane;

void main() {
    float lightDistance = length(fragWorldPos - u_LightPos);
    lightDistance = lightDistance / u_FarPlane;
    gl_FragDepth = lightDistance;
}
)";

    pointLightShadowShader = LoadShaderFromMemory(vsPointDepth, fsPointDepth);

    if (pointLightShadowShader.id == 0) {
        LINP_ERROR("Failed to load point light shadow shader!");
        return;
    }

    LINP_INFO("Shadow depth shader loaded successfully (ID: {})", shadowDepthShader.id);
    LINP_INFO("Point light shadow shader loaded successfully (ID: {})", pointLightShadowShader.id);
    initialized = true;
}

void ShadowManager::cleanup() {
    if (!initialized)
        return;

    shadowMaps.clear();
    cubemapShadowMaps.clear();

    if (shadowDepthShader.id != 0) {
        UnloadShader(shadowDepthShader);
        shadowDepthShader.id = 0;
    }
    if (pointLightShadowShader.id != 0) {
        UnloadShader(pointLightShadowShader);
        pointLightShadowShader.id = 0;
    }
    initialized = false;
}

ShadowMap* ShadowManager::getShadowMap(int index) {
    while (index >= shadowMaps.size()) {
        shadowMaps.push_back(std::make_unique<ShadowMap>());
    }
    return shadowMaps[index].get();
}

CubemapShadowMap* ShadowManager::getCubemapShadowMap(int index) {
    while (index >= cubemapShadowMaps.size()) {
        cubemapShadowMaps.push_back(std::make_unique<CubemapShadowMap>());
    }
    return cubemapShadowMaps[index].get();
}

Matrix ShadowManager::calculateDirectionalLightMatrix(
    Vector3 lightDir, Vector3 sceneCenter, float shadowDistance, float nearPlane, float farPlane) {

    lightDir = Vector3Normalize(lightDir);
    Vector3 lightPos
        = Vector3Add(sceneCenter, Vector3Scale(Vector3Negate(lightDir), shadowDistance * 0.5f));

    Vector3 up    = { 0, 1, 0 };
    float   upDot = fabsf(Vector3DotProduct(lightDir, up));
    if (upDot > 0.99f) {
        up = { 1, 0, 0 };
    }

    Matrix lightView = MatrixLookAt(lightPos, sceneCenter, up);
    float  orthoSize = shadowDistance * 0.5f;
    Matrix lightProjection
        = MatrixOrtho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);

    return MatrixMultiply(lightView, lightProjection);
}

Matrix ShadowManager::calculateSpotLightMatrix(
    Vector3 lightPos, Vector3 lightDir, float outerCutoff, float nearPlane, float farPlane) {

    lightDir       = Vector3Normalize(lightDir);
    Vector3 target = Vector3Add(lightPos, lightDir);

    Vector3 up    = { 0, 1, 0 };
    float   upDot = fabsf(Vector3DotProduct(lightDir, up));
    if (upDot > 0.99f) {
        up = { 1, 0, 0 };
    }

    Matrix lightView       = MatrixLookAt(lightPos, target, up);
    float  fov             = outerCutoff * 2.0f * DEG2RAD;
    Matrix lightProjection = MatrixPerspective(fov, 1.0f, nearPlane, farPlane);

    return MatrixMultiply(lightView, lightProjection);
}

std::array<Matrix, 6> ShadowManager::calculatePointLightMatrices(Vector3 lightPos, float farPlane) {
    std::array<Matrix, 6> matrices;

    Matrix projection = MatrixPerspective(90.0f * DEG2RAD, 1.0f, 0.1f, farPlane);

    // +X, -X, +Y, -Y, +Z, -Z
    std::array<Vector3, 6> targets = {
        Vector3 { lightPos.x + 1.0f, lightPos.y, lightPos.z }, // +X
        Vector3 { lightPos.x - 1.0f, lightPos.y, lightPos.z }, // -X
        Vector3 { lightPos.x, lightPos.y + 1.0f, lightPos.z }, // +Y
        Vector3 { lightPos.x, lightPos.y - 1.0f, lightPos.z }, // -Y
        Vector3 { lightPos.x, lightPos.y, lightPos.z + 1.0f }, // +Z
        Vector3 { lightPos.x, lightPos.y, lightPos.z - 1.0f }  // -Z
    };

    std::array<Vector3, 6> ups = {
        Vector3 { 0, -1, 0 }, // +X
        Vector3 { 0, -1, 0 }, // -X
        Vector3 { 0, 0, 1 },  // +Y
        Vector3 { 0, 0, -1 }, // -Y
        Vector3 { 0, -1, 0 }, // +Z
        Vector3 { 0, -1, 0 }  // -Z
    };

    for (int i = 0; i < 6; ++i) {
        Matrix view = MatrixLookAt(lightPos, targets[i], ups[i]);
        matrices[i] = MatrixMultiply(view, projection);
    }

    return matrices;
}

void ShadowManager::renderShadowMap(ShadowMap*                           shadowMap,
                                    const Matrix&                        lightSpaceMatrix,
                                    const std::vector<RenderableEntity>& renderables,
                                    Linp::Core::AssetManager*            assetMgr) {
    if (!shadowMap || !shadowMap->initialized || shadowDepthShader.id == 0) {
        return;
    }

    shadowMap->lightSpaceMatrix = lightSpaceMatrix;

    rlDrawRenderBatchActive();

    unsigned int prevFBO            = rlGetActiveFramebuffer();
    int          prevViewportWidth  = rlGetFramebufferWidth();
    int          prevViewportHeight = rlGetFramebufferHeight();

    rlEnableFramebuffer(shadowMap->depthTexture.id);
    rlViewport(0, 0, shadowMap->resolution, shadowMap->resolution);
    rlClearScreenBuffers();
    rlEnableDepthTest();
    rlEnableShader(shadowDepthShader.id);

    int lightSpaceLoc = GetShaderLocation(shadowDepthShader, "lightSpaceMatrix");
    SetShaderValueMatrix(shadowDepthShader, lightSpaceLoc, lightSpaceMatrix);

    for (const auto& renderable : renderables) {
        if (!renderable.meshRenderer || !renderable.transform)
            continue;

        if (auto model = renderable.meshRenderer->getModel(assetMgr)) {
            int    modelLoc    = GetShaderLocation(shadowDepthShader, "matModel");
            Matrix modelMatrix = renderable.transform->getMatrix();
            SetShaderValueMatrix(shadowDepthShader, modelLoc, modelMatrix);

            for (int i = 0; i < model->meshCount; ++i) {
                Mesh& mesh = model->meshes[i];

                if (mesh.vaoId > 0) {
                    rlEnableVertexArray(mesh.vaoId);

                    if (mesh.indices != nullptr) {
                        rlDrawVertexArrayElements(0, mesh.triangleCount * 3, 0);
                    } else {
                        rlDrawVertexArray(0, mesh.vertexCount);
                    }

                    rlDisableVertexArray();
                }
            }
        }
    }

    rlDrawRenderBatchActive();
    rlDisableShader();

    if (prevFBO > 0) {
        rlEnableFramebuffer(prevFBO);
    } else {
        rlDisableFramebuffer();
    }

    rlViewport(0, 0, prevViewportWidth, prevViewportHeight);
}

void ShadowManager::renderCubemapShadowMap(CubemapShadowMap*                    cubemapShadow,
                                           Vector3                              lightPos,
                                           float                                farPlane,
                                           const std::vector<RenderableEntity>& renderables,
                                           Linp::Core::AssetManager*            assetMgr) {
    if (!cubemapShadow || !cubemapShadow->initialized || shadowDepthShader.id == 0) {
        return;
    }

    auto matrices = calculatePointLightMatrices(lightPos, farPlane);

    rlDrawRenderBatchActive();

    unsigned int prevFBO            = rlGetActiveFramebuffer();
    int          prevViewportWidth  = rlGetFramebufferWidth();
    int          prevViewportHeight = rlGetFramebufferHeight();

    // Render to each face of the cubemap
    for (int face = 0; face < 6; ++face) {
        rlEnableFramebuffer(cubemapShadow->faceFramebuffers[face]);
        rlViewport(0, 0, cubemapShadow->resolution, cubemapShadow->resolution);
        rlClearScreenBuffers();
        rlEnableDepthTest();
        rlEnableShader(shadowDepthShader.id);

        int lightSpaceLoc = GetShaderLocation(shadowDepthShader, "lightSpaceMatrix");
        SetShaderValueMatrix(shadowDepthShader, lightSpaceLoc, matrices[face]);

        for (const auto& renderable : renderables) {
            if (!renderable.meshRenderer || !renderable.transform)
                continue;

            if (auto model = renderable.meshRenderer->getModel(assetMgr)) {
                int    modelLoc    = GetShaderLocation(shadowDepthShader, "matModel");
                Matrix modelMatrix = renderable.transform->getMatrix();
                SetShaderValueMatrix(shadowDepthShader, modelLoc, modelMatrix);

                for (int i = 0; i < model->meshCount; ++i) {
                    Mesh& mesh = model->meshes[i];

                    if (mesh.vaoId > 0) {
                        rlEnableVertexArray(mesh.vaoId);

                        if (mesh.indices != nullptr) {
                            rlDrawVertexArrayElements(0, mesh.triangleCount * 3, 0);
                        } else {
                            rlDrawVertexArray(0, mesh.vertexCount);
                        }

                        rlDisableVertexArray();
                    }
                }
            }
        }

        rlDrawRenderBatchActive();
        rlDisableShader();
    }

    if (prevFBO > 0) {
        rlEnableFramebuffer(prevFBO);
    } else {
        rlDisableFramebuffer();
    }

    rlViewport(0, 0, prevViewportWidth, prevViewportHeight);
}

ShadowManager::~ShadowManager() { cleanup(); }

}
