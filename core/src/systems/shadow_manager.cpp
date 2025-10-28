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

    resolution = res;

    // Initialize the RenderTexture2D structure properly
    depthTexture = { 0 };

    // Load framebuffer
    depthTexture.id = rlLoadFramebuffer();

    if (depthTexture.id > 0) {
        rlEnableFramebuffer(depthTexture.id);

        // We need to set texture width/height even though we're depth-only
        depthTexture.texture.width  = resolution;
        depthTexture.texture.height = resolution;

        // Create depth texture (NOT a renderbuffer - we need to sample it)
        depthTexture.depth.id      = rlLoadTextureDepth(resolution, resolution, false);
        depthTexture.depth.width   = resolution;
        depthTexture.depth.height  = resolution;
        depthTexture.depth.format  = 19; // DEPTH_COMPONENT_24BIT
        depthTexture.depth.mipmaps = 1;

        // Attach depth texture to framebuffer
        rlFramebufferAttach(depthTexture.id,
                            depthTexture.depth.id,
                            RL_ATTACHMENT_DEPTH,
                            RL_ATTACHMENT_TEXTURE2D,
                            0);

        // Check if framebuffer is complete
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

ShadowManager::ShadowManager(ShadowManager&& other) noexcept
    : shadowMaps(std::move(other.shadowMaps)), shadowDepthShader(other.shadowDepthShader),
      initialized(other.initialized) {

    other.initialized          = false;
    other.shadowDepthShader.id = 0;
}

ShadowManager& ShadowManager::operator=(ShadowManager&& other) noexcept {
    if (this != &other) {
        cleanup();
        shadowMaps                 = std::move(other.shadowMaps);
        shadowDepthShader          = other.shadowDepthShader;
        initialized                = other.initialized;
        other.initialized          = false;
        other.shadowDepthShader.id = 0;
    }
    return *this;
}

void ShadowManager::initialize() {
    if (initialized)
        return;

    // Load shadow depth shader
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
    // Depth written automatically to depth buffer
}
)";

    shadowDepthShader = LoadShaderFromMemory(vsDepth, fsDepth);

    if (shadowDepthShader.id == 0) {
        LINP_ERROR("Failed to load shadow depth shader!");
        return;
    }

    LINP_INFO("Shadow depth shader loaded successfully (ID: {})", shadowDepthShader.id);
    initialized = true;
}

void ShadowManager::cleanup() {
    if (!initialized)
        return;

    shadowMaps.clear();
    if (shadowDepthShader.id != 0) {
        UnloadShader(shadowDepthShader);
        shadowDepthShader.id = 0;
    }
    initialized = false;
}

ShadowMap* ShadowManager::getShadowMap(int index) {
    while (index >= shadowMaps.size()) {
        shadowMaps.push_back(std::make_unique<ShadowMap>());
    }
    return shadowMaps[index].get();
}

Matrix ShadowManager::calculateDirectionalLightMatrix(
    Vector3 lightDir, Vector3 sceneCenter, float shadowDistance, float nearPlane, float farPlane) {

    // Normalize light direction
    lightDir = Vector3Normalize(lightDir);

    // Light position: opposite direction from scene center
    Vector3 lightPos
        = Vector3Add(sceneCenter, Vector3Scale(Vector3Negate(lightDir), shadowDistance * 0.5f));

    // Choose appropriate up vector (avoid parallel to light direction)
    Vector3 up    = { 0, 1, 0 };
    float   upDot = fabsf(Vector3DotProduct(lightDir, up));
    if (upDot > 0.99f) {
        up = { 1, 0, 0 };
    }

    Matrix lightView = MatrixLookAt(lightPos, sceneCenter, up);

    // Orthographic projection for directional light
    float  orthoSize = shadowDistance * 0.5f;
    Matrix lightProjection
        = MatrixOrtho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);

    return MatrixMultiply(lightView, lightProjection);
}

Matrix ShadowManager::calculateSpotLightMatrix(
    Vector3 lightPos, Vector3 lightDir, float outerCutoff, float nearPlane, float farPlane) {

    lightDir       = Vector3Normalize(lightDir);
    Vector3 target = Vector3Add(lightPos, lightDir);

    // Choose appropriate up vector
    Vector3 up    = { 0, 1, 0 };
    float   upDot = fabsf(Vector3DotProduct(lightDir, up));
    if (upDot > 0.99f) {
        up = { 1, 0, 0 };
    }

    Matrix lightView = MatrixLookAt(lightPos, target, up);

    // Perspective projection for spot light
    float  fov             = outerCutoff * 2.0f * DEG2RAD;
    Matrix lightProjection = MatrixPerspective(fov, 1.0f, nearPlane, farPlane);

    return MatrixMultiply(lightView, lightProjection);
}

void ShadowManager::renderShadowMap(ShadowMap*                           shadowMap,
                                    const Matrix&                        lightSpaceMatrix,
                                    const std::vector<RenderableEntity>& renderables,
                                    Linp::Core::AssetManager*            assetMgr) {
    if (!shadowMap || !shadowMap->initialized) {
        LINP_ERROR("Shadow map not initialized!");
        return;
    }

    if (shadowDepthShader.id == 0) {
        LINP_ERROR("Shadow depth shader not initialized!");
        return;
    }

    shadowMap->lightSpaceMatrix = lightSpaceMatrix;

    // Flush any pending batched draw calls before changing state
    rlDrawRenderBatchActive();

    // Save current state
    unsigned int prevFBO            = rlGetActiveFramebuffer();
    int          prevViewportWidth  = rlGetFramebufferWidth();
    int          prevViewportHeight = rlGetFramebufferHeight();

    // Enable shadow framebuffer
    rlEnableFramebuffer(shadowMap->depthTexture.id);
    rlViewport(0, 0, shadowMap->resolution, shadowMap->resolution);
    rlClearScreenBuffers();
    rlEnableDepthTest();
    rlEnableShader(shadowDepthShader.id);

    // Set light space matrix uniform
    int lightSpaceLoc = GetShaderLocation(shadowDepthShader, "lightSpaceMatrix");
    SetShaderValueMatrix(shadowDepthShader, lightSpaceLoc, lightSpaceMatrix);

    // Render all objects with shadow shader
    int meshCount = 0;
    for (const auto& renderable : renderables) {
        if (!renderable.meshRenderer || !renderable.transform)
            continue;

        if (auto model = renderable.meshRenderer->getModel(assetMgr)) {
            // Set model matrix for this object
            int    modelLoc    = GetShaderLocation(shadowDepthShader, "matModel");
            Matrix modelMatrix = renderable.transform->getMatrix();
            SetShaderValueMatrix(shadowDepthShader, modelLoc, modelMatrix);

            // Render each mesh
            for (int i = 0; i < model->meshCount; ++i) {
                Mesh& mesh = model->meshes[i];

                if (mesh.vaoId > 0) {
                    rlEnableVertexArray(mesh.vaoId);

                    // Draw mesh
                    if (mesh.indices != nullptr) {
                        rlDrawVertexArrayElements(0, mesh.triangleCount * 3, 0);
                    } else {
                        rlDrawVertexArray(0, mesh.vertexCount);
                    }

                    rlDisableVertexArray();
                    meshCount++;
                }
            }
        }
    }

    // Flush shadow rendering
    rlDrawRenderBatchActive();

    // Disable shadow shader
    rlDisableShader();

    // Restore previous framebuffer
    if (prevFBO > 0) {
        rlEnableFramebuffer(prevFBO);
    } else {
        rlDisableFramebuffer();
    }

    // Restore viewport
    rlViewport(0, 0, prevViewportWidth, prevViewportHeight);
}

ShadowManager::~ShadowManager() { cleanup(); }

}
