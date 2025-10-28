#include "corvus/util/raylib.hpp"
#include <cstring>
#include <unordered_map>

std::vector<Mesh> splitTo16BitMeshes(const std::vector<float>&        vertices,
                                     const std::vector<float>&        normals,
                                     const std::vector<float>&        texcoords,
                                     const std::vector<unsigned int>& indices) {
    constexpr size_t  MAX_INDEX = 65535;
    std::vector<Mesh> result;

    size_t start = 0;
    while (start < indices.size()) {
        std::unordered_map<unsigned int, unsigned short> remap;
        std::vector<float>                               subVerts, subNorms, subTex;
        std::vector<unsigned short>                      subIdx;

        size_t end = std::min(start + MAX_INDEX, indices.size());
        for (size_t i = start; i < end; ++i) {
            unsigned int   global = indices[i];
            unsigned short local;
            auto           it = remap.find(global);
            if (it != remap.end()) {
                local = it->second;
            } else {
                local         = (unsigned short)remap.size();
                remap[global] = local;

                // copy vertex
                subVerts.push_back(vertices[global * 3 + 0]);
                subVerts.push_back(vertices[global * 3 + 1]);
                subVerts.push_back(vertices[global * 3 + 2]);

                if (!normals.empty()) {
                    subNorms.push_back(normals[global * 3 + 0]);
                    subNorms.push_back(normals[global * 3 + 1]);
                    subNorms.push_back(normals[global * 3 + 2]);
                }
                if (!texcoords.empty()) {
                    subTex.push_back(texcoords[global * 2 + 0]);
                    subTex.push_back(texcoords[global * 2 + 1]);
                }
            }
            subIdx.push_back(local);
        }

        Mesh m          = { 0 };
        m.vertexCount   = (int)(subVerts.size() / 3);
        m.triangleCount = (int)(subIdx.size() / 3);

        m.vertices = (float*)MemAlloc(subVerts.size() * sizeof(float));
        memcpy(m.vertices, subVerts.data(), subVerts.size() * sizeof(float));

        if (!subNorms.empty()) {
            m.normals = (float*)MemAlloc(subNorms.size() * sizeof(float));
            memcpy(m.normals, subNorms.data(), subNorms.size() * sizeof(float));
        }
        if (!subTex.empty()) {
            m.texcoords = (float*)MemAlloc(subTex.size() * sizeof(float));
            memcpy(m.texcoords, subTex.data(), subTex.size() * sizeof(float));
        }

        m.indices = (unsigned short*)MemAlloc(subIdx.size() * sizeof(unsigned short));
        memcpy(m.indices, subIdx.data(), subIdx.size() * sizeof(unsigned short));

        UploadMesh(&m, false);
        result.push_back(m);
        start = end;
    }

    return result;
}
