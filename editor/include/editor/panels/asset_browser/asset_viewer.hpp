#pragma once
#include "corvus/asset/asset_manager.hpp"
#include <string>

namespace Corvus::Editor {

class AssetViewer {
protected:
    Core::UUID          assetID;
    Core::AssetManager* assetManager;
    bool                isOpen = true;
    std::string         windowTitle;

public:
    AssetViewer(const Core::UUID& id, Core::AssetManager* manager, const std::string& title)
        : assetID(id), assetManager(manager), windowTitle(title) { }

    virtual ~AssetViewer() = default;

    // Render the viewer window
    virtual void render() = 0;

    // Check if the window is still open
    bool shouldClose() const { return !isOpen; }

    // Get the asset ID this viewer is displaying
    Core::UUID getAssetID() const { return assetID; }
};

}
