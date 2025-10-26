#pragma once
#include "Sound.hpp"
#include "asset_handle.hpp"
#include "asset_manager.hpp"
#include "linp/log.hpp"
#include "raylib.h"
#include "raylib-cpp.hpp"
#include <physfs.h>

namespace Linp::Core {

// PhysFS callback for raylib's file loading
// This allows LoadModel() and other functions to read from PhysFS
inline unsigned char* LoadFileDataFromPhysFS(const char* fileName, int* bytesRead) {
    PHYSFS_File* file = PHYSFS_openRead(fileName);
    if (!file) {
        *bytesRead = 0;
        return nullptr;
    }
    
    PHYSFS_sint64 fileSize = PHYSFS_fileLength(file);
    if (fileSize <= 0) {
        PHYSFS_close(file);
        *bytesRead = 0;
        return nullptr;
    }
    
    unsigned char* data = (unsigned char*)RL_MALLOC(fileSize);
    PHYSFS_sint64 readBytes = PHYSFS_readBytes(file, data, fileSize);
    PHYSFS_close(file);
    
    *bytesRead = (int)readBytes;
    return data;
}

inline char* LoadFileTextFromPhysFS(const char* fileName) {
    PHYSFS_File* file = PHYSFS_openRead(fileName);
    if (!file) {
        return nullptr;
    }
    
    PHYSFS_sint64 fileSize = PHYSFS_fileLength(file);
    if (fileSize <= 0) {
        PHYSFS_close(file);
        return nullptr;
    }
    
    char* text = (char*)RL_CALLOC(fileSize + 1, 1);
    PHYSFS_readBytes(file, text, fileSize);
    PHYSFS_close(file);
    
    return text;
}

class TextureLoader : public AssetLoader<raylib::Texture> {
public:
    raylib::Texture* loadTyped(const std::string& path) override {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            LINP_CORE_ERROR("Failed to open texture file: {}", path);
            return nullptr;
        }
        
        PHYSFS_sint64 fileSize = PHYSFS_fileLength(file);
        std::vector<unsigned char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);
        
        std::string ext = path.substr(path.find_last_of('.'));
        Image img = LoadImageFromMemory(ext.c_str(), buffer.data(), buffer.size());
        if (img.data == nullptr) {
            LINP_CORE_ERROR("Failed to load image: {}", path);
            return nullptr;
        }
        
        Texture2D texture = LoadTextureFromImage(img);
        UnloadImage(img);
        
        LINP_CORE_INFO("Loaded texture: {} ({}x{})", path, texture.width, texture.height);
        return new raylib::Texture(texture);
    }
    
    void unloadTyped(raylib::Texture* texture) override {
        if (texture) {
            UnloadTexture(*texture);
            delete texture;
        }
    }
    
    AssetType getType() const override { return AssetType::Texture; }
};

class ModelLoader : public AssetLoader<raylib::Model> {
public:
    raylib::Model* loadTyped(const std::string& path) override {
        Model model = LoadModel(path.c_str());
        
        if (!IsModelValid(model)) {
            LINP_CORE_ERROR("Failed to load model: {}", path);
            return nullptr;
        }
        
        LINP_CORE_INFO("Loaded model: {} ({} meshes)", path, model.meshCount);
        return new raylib::Model(model);
    }
    
    void unloadTyped(raylib::Model* model) override {
        if (model) {
            UnloadModel(*model);
            delete model;
        }
    }
    
    AssetType getType() const override { return AssetType::Model; }
};


class ShaderLoader : public AssetLoader<raylib::Shader> {
public:
    raylib::Shader* loadTyped(const std::string& path) override {
        std::string vsPath = path;
        std::string fsPath = path;
        
        if (path.ends_with(".vs")) {
            fsPath = path.substr(0, path.length() - 3) + ".fs";
        } else if (path.ends_with(".fs")) {
            vsPath = path.substr(0, path.length() - 3) + ".vs";
        }
        
        PHYSFS_File* vsFile = PHYSFS_openRead(vsPath.c_str());
        if (!vsFile) {
            LINP_CORE_ERROR("Failed to open vertex shader: {}", vsPath);
            return nullptr;
        }
        
        PHYSFS_sint64 vsSize = PHYSFS_fileLength(vsFile);
        std::vector<char> vsBuffer(vsSize + 1);
        PHYSFS_readBytes(vsFile, vsBuffer.data(), vsSize);
        vsBuffer[vsSize] = '\0';
        PHYSFS_close(vsFile);
        
        PHYSFS_File* fsFile = PHYSFS_openRead(fsPath.c_str());
        if (!fsFile) {
            LINP_CORE_ERROR("Failed to open fragment shader: {}", fsPath);
            return nullptr;
        }
        
        PHYSFS_sint64 fsSize = PHYSFS_fileLength(fsFile);
        std::vector<char> fsBuffer(fsSize + 1);
        PHYSFS_readBytes(fsFile, fsBuffer.data(), fsSize);
        fsBuffer[fsSize] = '\0';
        PHYSFS_close(fsFile);
        
        Shader shader = LoadShaderFromMemory(vsBuffer.data(), fsBuffer.data());
        if (shader.id == 0) {
            LINP_CORE_ERROR("Failed to load shader: {}", path);
            return nullptr;
        }
        
        LINP_CORE_INFO("Loaded shader: {}", path);
        return new raylib::Shader(shader);
    }
    
    void unloadTyped(raylib::Shader* shader) override {
        if (shader) {
            UnloadShader(*shader);
            delete shader;
        }
    }
    
    AssetType getType() const override { return AssetType::Shader; }
};

class SoundLoader : public AssetLoader<raylib::Sound> {
public:
    raylib::Sound* loadTyped(const std::string& path) override {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            LINP_CORE_ERROR("Failed to open sound file: {}", path);
            return nullptr;
        }
        
        PHYSFS_sint64 fileSize = PHYSFS_fileLength(file);
        std::vector<unsigned char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);
        
        std::string ext = path.substr(path.find_last_of('.'));
        Wave wave = LoadWaveFromMemory(ext.c_str(), buffer.data(), buffer.size());
        if (wave.data == nullptr) {
            LINP_CORE_ERROR("Failed to load wave: {}", path);
            return nullptr;
        }

        raylib::Sound* sound = new raylib::Sound(wave);
        UnloadWave(wave);

        LINP_CORE_INFO("Loaded sound: {}", path);
        return sound;
    }
    
    void unloadTyped(raylib::Sound* sound) override {
        if (sound) {
            UnloadSound(*sound);
            delete sound;
        }
    }
    
    AssetType getType() const override { return AssetType::Audio; }
};

class MusicLoader : public AssetLoader<raylib::Music> {
public:
    raylib::Music* loadTyped(const std::string& path) override {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            LINP_CORE_ERROR("Failed to open music file: {}", path);
            return nullptr;
        }
        
        PHYSFS_sint64 fileSize = PHYSFS_fileLength(file);
        std::vector<unsigned char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);
        
        std::string ext = path.substr(path.find_last_of('.'));
        Music music = LoadMusicStreamFromMemory(ext.c_str(), buffer.data(), buffer.size());
        if (music.stream.buffer == nullptr) {
            LINP_CORE_ERROR("Failed to load music: {}", path);
            return nullptr;
        }
        
        LINP_CORE_INFO("Loaded music: {}", path);
        return new raylib::Music(music);
    }
    
    void unloadTyped(raylib::Music* music) override {
        if (music) {
            UnloadMusicStream(*music);
            delete music;
        }
    }
    
    AssetType getType() const override { return AssetType::Audio; }
};

class FontLoader : public AssetLoader<raylib::Font> {
public:
    raylib::Font* loadTyped(const std::string& path) override {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            LINP_CORE_ERROR("Failed to open font file: {}", path);
            return nullptr;
        }
        
        PHYSFS_sint64 fileSize = PHYSFS_fileLength(file);
        std::vector<unsigned char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);
        
        std::string ext = path.substr(path.find_last_of('.'));
        Font font = LoadFontFromMemory(ext.c_str(), buffer.data(), buffer.size(), 32, nullptr, 0);
        if (font.texture.id == 0) {
            LINP_CORE_ERROR("Failed to load font: {}", path);
            return nullptr;
        }
        
        LINP_CORE_INFO("Loaded font: {}", path);
        return new raylib::Font(font);
    }
    
    void unloadTyped(raylib::Font* font) override {
        if (font) {
            UnloadFont(*font);
            delete font;
        }
    }
    
    AssetType getType() const override { return AssetType::Font; }
};

}