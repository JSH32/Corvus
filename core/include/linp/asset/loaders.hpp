#pragma once

#include "linp/asset/raylib_loaders.hpp"
#include "linp/asset/scene_loader.hpp"

namespace Linp::Core {
inline void registerLoaders(AssetManager& assetMgr) {
    SetLoadFileDataCallback(LoadFileDataFromPhysFS);
    SetLoadFileTextCallback(LoadFileTextFromPhysFS);

    assetMgr.registerLoader<raylib::Texture, TextureLoader>({".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr"});
    assetMgr.registerLoader<raylib::Model, ModelLoader>({".obj", ".gltf", ".glb", ".iqm", ".vox", ".m3d"});
    assetMgr.registerLoader<raylib::Shader, ShaderLoader>({".vs", ".fs", ".glsl"});
    assetMgr.registerLoader<raylib::Sound, SoundLoader>({".wav", ".ogg"});
    assetMgr.registerLoader<raylib::Music, MusicLoader>({".mp3", ".ogg", ".flac", ".xm", ".mod"});
    assetMgr.registerLoader<raylib::Font, FontLoader>({".ttf", ".otf"});
    assetMgr.registerLoader<Scene, SceneLoader>({".scene"});
}
}