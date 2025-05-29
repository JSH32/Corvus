#pragma once

#include <string>

namespace Linp::Core::Components {
class EntityInfoComponent {
public:
    explicit EntityInfoComponent(std::string tag = "New entity", const bool enabled = true)
        : tag(std::move(tag)), enabled(enabled) { }

    std::string tag;
    bool        enabled {};
};

}
