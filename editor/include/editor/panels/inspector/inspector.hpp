#pragma once

namespace Linp::Editor {
template <DrawComponent T>
void renderInspectorComponent(T& component) { draw(component); }
}