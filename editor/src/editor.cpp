#pragma once

#include "editor/editor.hpp"
#include "imgui.h"

namespace Linp::Editor {
EditorLayer::EditorLayer() : Core::Layer("Editor") { }

void EditorLayer::onImGuiRender() {
    ImGui::ShowDemoWindow();
}
}