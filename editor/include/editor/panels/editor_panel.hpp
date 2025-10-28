#pragma once

#include "IconsFontAwesome6.h"
#include "corvus/entity.hpp"
#include <format>
#include <string>

namespace Corvus::Editor {
class EditorPanel {
public:
    virtual ~EditorPanel() = default;
    virtual void onUpdate() { }

    std::string title() { return std::format("{} Panel", ICON_FA_PAPERCLIP); };

private:
    void drawEntity(Core::Entity entity) const;
};
}
