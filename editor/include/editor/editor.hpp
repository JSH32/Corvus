#pragma once

#include "linp/layer.hpp"

namespace Linp::Editor {
class EditorLayer : public Core::Layer {
public:
    EditorLayer();

    void onImGuiRender() override;
};
}