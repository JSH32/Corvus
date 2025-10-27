#include "linp/layerstack.hpp"
#include <algorithm>

namespace Linp::Core {

void LayerStack::pushLayer(std::unique_ptr<Layer> layer) {
    // Insert before overlays
    layers.emplace(layers.begin() + layerInsertIndex, std::move(layer));
    ++layerInsertIndex; // Move the divider forward
}

void LayerStack::pushOverlay(std::unique_ptr<Layer> overlay) {
    // Always push to the end
    layers.emplace_back(std::move(overlay));
}

void LayerStack::popLayer(Layer* layer) {
    auto it
        = std::find_if(layers.begin(),
                       layers.begin() + layerInsertIndex,
                       [layer](const std::unique_ptr<Layer>& ptr) { return ptr.get() == layer; });

    if (it != layers.begin() + layerInsertIndex) {
        layers.erase(it);
        --layerInsertIndex; // Move divider back
    }
}

void LayerStack::popOverlay(Layer* overlay) {
    auto it = std::find_if(
        layers.begin() + layerInsertIndex,
        layers.end(),
        [overlay](const std::unique_ptr<Layer>& ptr) { return ptr.get() == overlay; });

    if (it != layers.end())
        layers.erase(it);
}

void LayerStack::clear() {
    for (auto& layer : layers)
        layer->onDetach();

    layers.clear();
    layerInsertIndex = 0;
}
}
