#include "editor/editor.hpp"
#include "linp/application.hpp"

int main() {
    Linp::Log::init();
    Linp::Core::Application app(1280, 720, "Lost in Purgatory (EDITOR)");

    Linp::Editor::EditorLayer editorLayer;
    app.pushLayer(&editorLayer);

    app.run();
}
