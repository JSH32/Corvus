#pragma once

#include "corvus/asset/material/material.hpp"
#include "corvus/asset/material/material_loader.hpp"
#include "corvus/asset/raylib_loaders.hpp"
#include "corvus/asset/scene_loader.hpp"

namespace Corvus::Core {
inline void registerLoaders(AssetManager& assetMgr) {
    assetMgr.registerLoader<raylib::Texture, TextureLoader>(
        { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr" });
    assetMgr.registerLoader<raylib::Model, ModelLoader>({ ".obj" });
    assetMgr.registerLoader<Material, MaterialLoader>({ ".mat" });
    assetMgr.registerLoader<raylib::Shader, ShaderLoader>({ ".vert", ".frag", ".glsl" });
    assetMgr.registerLoader<raylib::Sound, SoundLoader>({ ".wav", ".ogg" });
    assetMgr.registerLoader<raylib::Music, MusicLoader>({ ".mp3", ".ogg", ".flac", ".xm", ".mod" });
    assetMgr.registerLoader<raylib::Font, FontLoader>({ ".ttf", ".otf" });
    assetMgr.registerLoader<Scene, SceneLoader>({ ".scene" });
}
}
