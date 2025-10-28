#pragma once
#include "corvus/util/non_copyable.hpp"
#include "render_types.hpp"
#include <memory>
#include <raylib.h>
#include <rlgl.h>
#include <vector>

namespace Corvus::Core {
class AssetManager;
}

namespace Corvus::Core::Systems {

struct ShadowMap {
    RenderTexture2D depthTexture;
    Matrix          lightSpaceMatrix;
    int             resolution  = 0;
    bool            initialized = false;

    void initialize(int res);
    void cleanup();

    ShadowMap() = default;
    ~ShadowMap();
    ShadowMap(const ShadowMap&)            = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
};

struct CubemapShadowMap {
    unsigned int cubemapDepthTexture = 0;     // Cubemap depth texture
    unsigned int faceFramebuffers[6] = { 0 }; // One FBO per cubemap face
    int          resolution          = 1024;
    bool         initialized         = false;
    Vector3      lightPosition       = { 0, 0, 0 };
    float        farPlane            = 25.0f;

    void initialize(int res);
    void cleanup();
    ~CubemapShadowMap();
};

class ShadowManager {
public:
    ~ShadowManager();
    ShadowManager()                                = default;
    ShadowManager(const ShadowManager&)            = delete;
    ShadowManager& operator=(const ShadowManager&) = delete;
    ShadowManager(ShadowManager&& other) noexcept;
    ShadowManager& operator=(ShadowManager&& other) noexcept;

    static constexpr int MAX_SHADOW_MAPS = 4;

    std::vector<std::unique_ptr<ShadowMap>> shadowMaps;
    Shader                                  shadowDepthShader;
    bool                                    initialized = false;

    std::vector<std::unique_ptr<CubemapShadowMap>> cubemapShadowMaps;
    Shader                                         pointLightShadowShader = { 0 };

    void initialize();
    void cleanup();

    ShadowMap* getShadowMap(int index);

    Matrix calculateDirectionalLightMatrix(Vector3 lightDir,
                                           Vector3 sceneCenter,
                                           float   shadowDistance,
                                           float   nearPlane,
                                           float   farPlane);

    Matrix calculateSpotLightMatrix(
        Vector3 lightPos, Vector3 lightDir, float outerCutoff, float nearPlane, float farPlane);

    void renderShadowMap(ShadowMap*                           shadowMap,
                         const Matrix&                        lightSpaceMatrix,
                         const std::vector<RenderableEntity>& renderables,
                         Corvus::Core::AssetManager*            assetMgr);

    CubemapShadowMap*     getCubemapShadowMap(int index);
    std::array<Matrix, 6> calculatePointLightMatrices(Vector3 lightPos, float farPlane);

    void renderCubemapShadowMap(CubemapShadowMap*                    cubemapShadow,
                                Vector3                              lightPos,
                                float                                farPlane,
                                const std::vector<RenderableEntity>& renderables,
                                Corvus::Core::AssetManager*            assetMgr);
};

}
