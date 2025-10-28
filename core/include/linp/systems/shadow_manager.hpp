#pragma once
#include "linp/util/non_copyable.hpp"
#include "render_types.hpp"
#include <memory>
#include <raylib.h>
#include <rlgl.h>
#include <vector>

namespace Linp::Core {
class AssetManager;
}

namespace Linp::Core::Systems {

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
                         Linp::Core::AssetManager*            assetMgr);
};

}
