#pragma once

#include "raylib.h"
#include "vector"

std::vector<Mesh> splitTo16BitMeshes(const std::vector<float>&        vertices,
                                     const std::vector<float>&        normals,
                                     const std::vector<float>&        texcoords,
                                     const std::vector<unsigned int>& indices);
