#pragma once

#include "corvus/asset/material/material.hpp"
#include "corvus/asset/material/material_loader.hpp"
#include "corvus/asset/raylib_loaders.hpp"
#include "corvus/asset/scene_loader.hpp"
#include "corvus/renderer/model.hpp"

namespace Corvus::Core {
inline void registerLoaders(AssetManager& assetMgr) {
    assetMgr.registerLoader<Graphics::Texture2D, TextureLoader>(
        { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr" });
    assetMgr.registerLoader<Renderer::Model, ModelLoader>({ ".obj" });
    assetMgr.registerLoader<MaterialAsset, MaterialLoader>({ ".mat" });
    assetMgr.registerLoader<Graphics::Shader, ShaderLoader>({ ".vert", ".frag", ".glsl" });
    assetMgr.registerLoader<Scene, SceneLoader>({ ".scene" });
}

}
