#pragma once
#include "asset_handle.hpp"
#include "asset_manager.hpp"
#include "corvus/log.hpp"
#include "corvus/renderer/model.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "tiny_obj_loader.h"
#include <physfs.h>

namespace Corvus::Core {

class TextureLoader : public AssetLoader<Graphics::Texture2D> {
public:
    Graphics::Texture2D* loadTyped(const std::string& path) override {
        auto ctx = getLoaderContext()->graphics;
        if (!ctx) {
            CORVUS_CORE_CRITICAL("TextureLoader requires GraphicsContext!");
            return nullptr;
        }

        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) {
            CORVUS_CORE_ERROR("Failed to open texture: {}", path);
            return nullptr;
        }

        PHYSFS_sint64              size = PHYSFS_fileLength(file);
        std::vector<unsigned char> data(size);
        PHYSFS_readBytes(file, data.data(), size);
        PHYSFS_close(file);

        int            w, h, comp;
        unsigned char* decoded = stbi_load_from_memory(data.data(), size, &w, &h, &comp, 4);
        if (!decoded) {
            CORVUS_CORE_ERROR("Failed to decode image: {}", path);
            return nullptr;
        }

        auto texture = new Graphics::Texture2D(ctx->createTexture2D(w, h));
        texture->setData(decoded, w * h * 4);
        stbi_image_free(decoded);

        CORVUS_CORE_INFO("Loaded texture: {} ({}x{})", path, w, h);
        return texture;
    }

    void unloadTyped(Graphics::Texture2D* tex) override {
        if (tex) {
            tex->release();
            delete tex;
        }
    }

    AssetType getType() const override { return AssetType::Texture; }
};

class ModelLoader : public AssetLoader<Renderer::Model> {
public:
    Renderer::Model* loadTyped(const std::string& path) override {
        auto ctx = getLoaderContext() ? getLoaderContext()->graphics : nullptr;
        if (!ctx) {
            CORVUS_CORE_CRITICAL("ModelLoader requires GraphicsContext!");
            assert(ctx && "ModelLoader missing GraphicsContext");
            return nullptr;
        }

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

        // Parse OBJ using TinyObjLoader
        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string                      warn, err;

        bool ok = tinyobj::LoadObj(
            &attrib, &shapes, &materials, &warn, &err, &objStream, nullptr, true);
        if (!warn.empty())
            CORVUS_CORE_WARN("TinyObj warning: {}", warn);
        if (!ok) {
            CORVUS_CORE_ERROR("TinyObj parse failed for {}: {}", path, err);
            return nullptr;
        }

        auto* model = new Renderer::Model();

        for (const auto& shape : shapes) {
            std::vector<Renderer::Vertex> vertices;
            std::vector<uint32_t>         indices;
            vertices.reserve(shape.mesh.indices.size());
            indices.reserve(shape.mesh.indices.size());

            for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
                const tinyobj::index_t& idx = shape.mesh.indices[i];

                glm::vec3 pos(0.0f);
                glm::vec3 norm(0.0f, 1.0f, 0.0f);
                glm::vec2 uv(0.0f);

                // Position
                if (idx.vertex_index >= 0) {
                    int vx = idx.vertex_index * 3;
                    if (vx + 2 < attrib.vertices.size()) {
                        pos = glm::vec3(attrib.vertices[vx + 0],
                                        attrib.vertices[vx + 1],
                                        attrib.vertices[vx + 2]);
                    }
                }

                // Normal
                if (idx.normal_index >= 0) {
                    int nx = idx.normal_index * 3;
                    if (nx + 2 < attrib.normals.size()) {
                        norm = glm::vec3(
                            attrib.normals[nx + 0], attrib.normals[nx + 1], attrib.normals[nx + 2]);
                    }
                }

                // TexCoord (flip V for consistency)
                if (idx.texcoord_index >= 0) {
                    int tx = idx.texcoord_index * 2;
                    if (tx + 1 < attrib.texcoords.size()) {
                        uv = glm::vec2(attrib.texcoords[tx + 0], 1.0f - attrib.texcoords[tx + 1]);
                    }
                }

                vertices.push_back({ pos, norm, uv });
                indices.push_back(static_cast<uint32_t>(i));
            }

            if (vertices.empty()) {
                CORVUS_CORE_WARN("Skipping empty shape in OBJ: {}", path);
                continue;
            }

            Renderer::Mesh mesh = Renderer::Mesh::createFromVertices(*ctx, vertices, indices);
            model->addMesh(std::move(mesh));
        }

        CORVUS_CORE_INFO("Loaded OBJ: {} ({} meshes)", path, model->getMeshes().size());
        return model;
    }

    void unloadTyped(Renderer::Model* model) override {
        if (model) {
            model->release();
            delete model;
        }
    }

    AssetType getType() const override { return AssetType::Model; }
};

class ShaderLoader : public AssetLoader<Graphics::Shader> {
public:
    Graphics::Shader* loadTyped(const std::string& path) override {
        std::string vsPath = path;
        std::string fsPath = path;

        // Determine complementary shader path
        if (path.ends_with(".vert")) {
            fsPath = path.substr(0, path.length() - 5) + ".frag";
        } else if (path.ends_with(".frag")) {
            vsPath = path.substr(0, path.length() - 5) + ".vert";
        }

        auto readFileToString = [](const std::string& physfsPath) -> std::string {
            PHYSFS_File* file = PHYSFS_openRead(physfsPath.c_str());
            if (!file) {
                CORVUS_CORE_ERROR("Failed to open shader file: {}", physfsPath);
                return {};
            }
            PHYSFS_sint64 size = PHYSFS_fileLength(file);
            std::string   buffer(size, '\0');
            PHYSFS_readBytes(file, buffer.data(), size);
            PHYSFS_close(file);
            return buffer;
        };

        std::string vsSource = readFileToString(vsPath);
        std::string fsSource = readFileToString(fsPath);

        if (vsSource.empty() || fsSource.empty()) {
            CORVUS_CORE_ERROR("Shader source missing or unreadable: {}", path);
            return nullptr;
        }

        // Use graphics context API to compile shader
        Graphics::Shader shader = getLoaderContext()->graphics->createShader(vsSource, fsSource);

        if (!shader.valid()) {
            CORVUS_CORE_ERROR("Failed to compile shader: {}", path);
            return nullptr;
        }

        CORVUS_CORE_INFO("Loaded shader successfully: {}", path);
        return new Graphics::Shader(std::move(shader));
    }

    void unloadTyped(Graphics::Shader* shader) override {
        if (!shader)
            return;

        if (shader->valid()) {
            shader->release();
            CORVUS_CORE_INFO("Unloaded shader (id={})", shader->id);
        }

        delete shader;
    }

    AssetType getType() const override { return AssetType::Shader; }
};

}
