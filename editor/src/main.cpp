#include "corvus/application.hpp"
#include "editor/editor.hpp"
#include "editor/project_selector.hpp"
#include <memory>

int main() {
    Corvus::Log::init();
    Corvus::Core::Application app(1280, 720, "Corvus Engine");

    app.getLayerStack().pushLayer(std::make_unique<Corvus::Editor::ProjectSelector>(&app));

    app.run();

    return 0;
}
