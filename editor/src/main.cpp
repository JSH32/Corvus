#include "corvus/application.hpp"
#include "editor/editor.hpp"
#include <memory>

int main() {
    Corvus::Log::init();
    Corvus::Core::Application app(1280, 720, "Corvus Engine");

    app.pushLayer(std::make_unique<Corvus::Editor::EditorLayer>(&app));

    app.run();

    return 0;
}
