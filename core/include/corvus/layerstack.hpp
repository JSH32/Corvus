#pragma once
#include "corvus/layer.hpp"
#include <memory>
#include <vector>

namespace Corvus::Core {
class LayerStack {
public:
    LayerStack()  = default;
    ~LayerStack() = default;

    void pushLayer(std::unique_ptr<Layer> layer);
    void pushOverlay(std::unique_ptr<Layer> overlay);
    void popLayer(Layer* layer);
    void popOverlay(Layer* overlay);
    void clear();

    struct Iterator {
        using BaseIter = std::vector<std::unique_ptr<Layer>>::iterator;
        BaseIter it;

        Iterator(BaseIter it) : it(it) { }
        Layer*    operator*() { return it->get(); }
        Iterator& operator++() {
            ++it;
            return *this;
        }
        bool operator!=(const Iterator& other) const { return it != other.it; }
    };

    Iterator begin() { return Iterator(layers.begin()); }
    Iterator end() { return Iterator(layers.end()); }

    size_t size() const { return layers.size(); }

private:
    std::vector<std::unique_ptr<Layer>> layers;
    unsigned int                        layerInsertIndex = 0; // Divider between layers and overlays
};
}
