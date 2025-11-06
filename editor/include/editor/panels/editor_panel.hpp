#pragma once

#include "IconsFontAwesome6.h"
#include <string>

namespace Corvus::Editor {
class EditorPanel {
public:
    virtual ~EditorPanel() = default;
    virtual void onUpdate() { }

    virtual std::string title() { return fmt::format("{} Panel", ICON_FA_PAPERCLIP); };
};
}
