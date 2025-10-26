#include "editor/editor.hpp"
#include "linp/application.hpp"
#include <memory>

int main() {
    Linp::Log::init();
    Linp::Core::Application app(1280, 720, "Lost in Purgatory (EDITOR)");

    app.pushLayer(std::make_unique<Linp::Editor::EditorLayer>(&app));

    app.run();

    return 0;
}
