#pragma once

#include <concepts>
#include <linp/components/entity_info.hpp>

namespace Linp::Editor {
template <typename T>
concept DrawComponent = requires(T component) {
                            { draw(component) } -> std::same_as<void>;
                        };

void draw(const Linp::Core::Components::EntityInfoComponent& entityInfo) {

}
}
