#pragma once

#include <string>
#include <utility>

namespace Linp::Core {
class Layer {
public:
    explicit Layer(std::string name = "Layer") : debugName(std::move(name)) { }

    virtual ~Layer() = default;

    virtual void onAttach() { }
    virtual void onDetach() { }
    virtual void onUpdate() { }
    virtual void onImGuiRender() { }

    const std::string& getName() const { return debugName; }

protected:
    std::string debugName;
};
}
