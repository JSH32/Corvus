#pragma once

#include <string>

namespace Linp::Core {
class Layer {
public:
    Layer(const std::string& name = "Layer")
        : debugName(name) { }

    virtual ~Layer() { }

    virtual void onAttach() { }
    virtual void onDetach() { }
    virtual void onUpdate() { }
    virtual void onImGuiRender() { }

    inline const std::string& getName() const { return debugName; }

protected:
    std::string debugName;
};
}