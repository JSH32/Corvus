#include "corvus/input/input_producer.hpp"
#include <GLFW/glfw3.h>

namespace Corvus::Core::Events {

void InputProducer::update() {
    if (!window)
        return;

    window->pollEvents();
}

}
