#pragma once

#include <concepts>

namespace Linp::Editor {
template <typename T>
concept DrawComponent = requires(T component) {
                            { draw(component) } -> std::same_as<void>;
                        };
}