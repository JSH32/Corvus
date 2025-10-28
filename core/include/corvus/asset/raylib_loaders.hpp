#pragma once
#include "Model.hpp"
#include "ShaderUnmanaged.hpp"
#include "Sound.hpp"
#include "asset_handle.hpp"
#include "asset_manager.hpp"
#include "corvus/log.hpp"
#include "corvus/util/raylib.hpp"
#include "raylib-cpp.hpp"
#include "raylib.h"
#include "tiny_obj_loader.h"
#include <physfs.h>

namespace Corvus::Core {

class TextureLoader : public AssetLoader<raylib::Texture> {
public:
    raylib::Texture* loadTyped(const std::string& path) override {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            CORVUS_CORE_ERROR("Failed to open texture file: {}", path);
            return nullptr;
        }

        PHYSFS_sint64              fileSize = PHYSFS_fileLength(file);
        std::vector<unsigned char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);

        std::string ext = path.substr(path.find_last_of('.'));
        Image       img = LoadImageFromMemory(ext.c_str(), buffer.data(), buffer.size());
        if (img.data == nullptr) {
            CORVUS_CORE_ERROR("Failed to load image: {}", path);
            return nullptr;
        }

        Texture2D texture = LoadTextureFromImage(img);
        UnloadImage(img);

        CORVUS_CORE_INFO("Loaded texture: {} ({}x{})", path, texture.width, texture.height);
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
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            CORVUS_CORE_ERROR("Failed to open OBJ: {}", path);
            return nullptr;
        }

        PHYSFS_sint64     size = PHYSFS_fileLength(file);
        std::vector<char> buf(size);
        if (PHYSFS_readBytes(file, buf.data(), size) != size) {
            CORVUS_CORE_ERROR("Failed to read OBJ file: {}", path);
            PHYSFS_close(file);
            return nullptr;
        }
        PHYSFS_close(file);

        std::string        objData(buf.begin(), buf.end());
        std::istringstream objStream(objData);

        // TinyObj parsing
        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials; // ignored
        std::string                      warn, err;

        bool ok = tinyobj::LoadObj(&attrib,
                                   &shapes,
                                   &materials,
                                   &warn,
                                   &err,
                                   &objStream,
                                   nullptr, // ignore MTL base path
                                   true     // triangulate
        );

        if (!warn.empty())
            CORVUS_CORE_WARN("TinyObj warning: {}", warn);
        if (!ok) {
            CORVUS_CORE_ERROR("TinyObj parse failed for {}: {}", path, err);
            return nullptr;
        }

        std::vector<float>        vertices;
        std::vector<float>        normals;
        std::vector<float>        texcoords;
        std::vector<unsigned int> indices;

        for (const auto& shape : shapes) {
            for (const auto& idx : shape.mesh.indices) {
                int vx = idx.vertex_index * 3;
                vertices.push_back(attrib.vertices[vx + 0]);
                vertices.push_back(attrib.vertices[vx + 1]);
                vertices.push_back(attrib.vertices[vx + 2]);

                if (idx.normal_index >= 0) {
                    int nx = idx.normal_index * 3;
                    normals.push_back(attrib.normals[nx + 0]);
                    normals.push_back(attrib.normals[nx + 1]);
                    normals.push_back(attrib.normals[nx + 2]);
                }

                if (idx.texcoord_index >= 0) {
                    int tx = idx.texcoord_index * 2;
                    texcoords.push_back(attrib.texcoords[tx + 0]);
                    texcoords.push_back(1.0f - attrib.texcoords[tx + 1]);
                }

                indices.push_back((unsigned int)indices.size());
            }
        }

        // Split if too large for 16-bit indices
        std::vector<Mesh> meshes;
        if (indices.size() > 65535) {
            CORVUS_CORE_WARN(
                "OBJ '{}' has {} indices — splitting into submeshes", path, indices.size());
            meshes = splitTo16BitMeshes(vertices, normals, texcoords, indices);
        } else {
            Mesh mesh          = { 0 };
            mesh.vertexCount   = (int)(vertices.size() / 3);
            mesh.triangleCount = (int)(indices.size() / 3);

            mesh.vertices = (float*)MemAlloc(vertices.size() * sizeof(float));
            memcpy(mesh.vertices, vertices.data(), vertices.size() * sizeof(float));

            if (!normals.empty()) {
                mesh.normals = (float*)MemAlloc(normals.size() * sizeof(float));
                memcpy(mesh.normals, normals.data(), normals.size() * sizeof(float));
            }

            if (!texcoords.empty()) {
                mesh.texcoords = (float*)MemAlloc(texcoords.size() * sizeof(float));
                memcpy(mesh.texcoords, texcoords.data(), texcoords.size() * sizeof(float));
            }

            std::vector<unsigned short> shortIndices(indices.begin(), indices.end());
            mesh.indices = (unsigned short*)MemAlloc(shortIndices.size() * sizeof(unsigned short));
            memcpy(mesh.indices, shortIndices.data(), shortIndices.size() * sizeof(unsigned short));

            UploadMesh(&mesh, false);
            meshes.push_back(mesh);
        }

        ::Model rawModel   = { 0 };
        rawModel.meshCount = (int)meshes.size();
        rawModel.meshes    = (Mesh*)MemAlloc(meshes.size() * sizeof(Mesh));
        for (size_t i = 0; i < meshes.size(); ++i)
            rawModel.meshes[i] = meshes[i];

        rawModel.materials    = (::Material*)MemAlloc(sizeof(::Material));
        rawModel.materials[0] = LoadMaterialDefault();
        rawModel.meshMaterial = (int*)MemAlloc(meshes.size() * sizeof(int));
        for (size_t i = 0; i < meshes.size(); ++i)
            rawModel.meshMaterial[i] = 0;

        auto* model = new raylib::Model(rawModel);

        CORVUS_CORE_INFO("Loaded OBJ: {} (verts: {}, tris: {}, submeshes: {})",
                         path,
                         vertices.size() / 3,
                         indices.size() / 3,
                         meshes.size());

        return model;
    }

    void unloadTyped(raylib::Model* model) override {
        if (model) {
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

        if (path.ends_with(".vert")) {
            fsPath = path.substr(0, path.length() - 3) + ".frag";
        } else if (path.ends_with(".frag")) {
            vsPath = path.substr(0, path.length() - 3) + ".vert";
        }

        PHYSFS_File* vsFile = PHYSFS_openRead(vsPath.c_str());
        if (!vsFile) {
            CORVUS_CORE_ERROR("Failed to open vertex shader: {}", vsPath);
            return nullptr;
        }

        PHYSFS_sint64     vsSize = PHYSFS_fileLength(vsFile);
        std::vector<char> vsBuffer(vsSize + 1);
        PHYSFS_readBytes(vsFile, vsBuffer.data(), vsSize);
        vsBuffer[vsSize] = '\0';
        PHYSFS_close(vsFile);

        PHYSFS_File* fsFile = PHYSFS_openRead(fsPath.c_str());
        if (!fsFile) {
            CORVUS_CORE_ERROR("Failed to open fragment shader: {}", fsPath);
            return nullptr;
        }

        PHYSFS_sint64     fsSize = PHYSFS_fileLength(fsFile);
        std::vector<char> fsBuffer(fsSize + 1);
        PHYSFS_readBytes(fsFile, fsBuffer.data(), fsSize);
        fsBuffer[fsSize] = '\0';
        PHYSFS_close(fsFile);

        Shader shader = LoadShaderFromMemory(vsBuffer.data(), fsBuffer.data());
        if (shader.id == 0) {
            CORVUS_CORE_ERROR("Failed to load shader: {}", path);
            return nullptr;
        }

        CORVUS_CORE_INFO("Loaded shader: {}", path);
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
            CORVUS_CORE_ERROR("Failed to open sound file: {}", path);
            return nullptr;
        }

        PHYSFS_sint64              fileSize = PHYSFS_fileLength(file);
        std::vector<unsigned char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);

        std::string ext  = path.substr(path.find_last_of('.'));
        Wave        wave = LoadWaveFromMemory(ext.c_str(), buffer.data(), buffer.size());
        if (wave.data == nullptr) {
            CORVUS_CORE_ERROR("Failed to load wave: {}", path);
            return nullptr;
        }

        raylib::Sound* sound = new raylib::Sound(wave);
        UnloadWave(wave);

        CORVUS_CORE_INFO("Loaded sound: {}", path);
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
            CORVUS_CORE_ERROR("Failed to open music file: {}", path);
            return nullptr;
        }

        PHYSFS_sint64              fileSize = PHYSFS_fileLength(file);
        std::vector<unsigned char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);

        std::string ext   = path.substr(path.find_last_of('.'));
        Music       music = LoadMusicStreamFromMemory(ext.c_str(), buffer.data(), buffer.size());
        if (music.stream.buffer == nullptr) {
            CORVUS_CORE_ERROR("Failed to load music: {}", path);
            return nullptr;
        }

        CORVUS_CORE_INFO("Loaded music: {}", path);
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
            CORVUS_CORE_ERROR("Failed to open font file: {}", path);
            return nullptr;
        }

        PHYSFS_sint64              fileSize = PHYSFS_fileLength(file);
        std::vector<unsigned char> buffer(fileSize);
        PHYSFS_readBytes(file, buffer.data(), fileSize);
        PHYSFS_close(file);

        std::string ext = path.substr(path.find_last_of('.'));
        Font font = LoadFontFromMemory(ext.c_str(), buffer.data(), buffer.size(), 32, nullptr, 0);
        if (font.texture.id == 0) {
            CORVUS_CORE_ERROR("Failed to load font: {}", path);
            return nullptr;
        }

        CORVUS_CORE_INFO("Loaded font: {}", path);
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
