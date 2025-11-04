#include "corvus/renderer/renderable.hpp"

namespace Corvus::Renderer {

Renderable::Renderable(const Mesh& mesh, MaterialRef material)
    : mesh_(mesh), material_(material) { }

}
