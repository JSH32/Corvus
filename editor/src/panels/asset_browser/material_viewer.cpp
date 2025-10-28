#include "editor/panels/asset_browser/material_viewer.hpp"
#include "IconsFontAwesome6.h"
#include "corvus/log.hpp"
#include "corvus/systems/lighting_system.hpp"
#include "imgui_internal.h"
#include "rlImGui.h"
#include <format>

namespace Corvus::Editor {

MaterialViewer::MaterialViewer(const Core::UUID& id, Core::AssetManager* manager)
    : AssetViewer(id, manager, "Material Viewer") {
    materialHandle = assetManager->loadByID<Core::Material>(id);
    if (materialHandle.isValid()) {
        auto meta   = assetManager->getMetadata(id);
        windowTitle = std::format(
            "{} Material: {}", ICON_FA_PALETTE, meta.path.substr(meta.path.find_last_of('/') + 1));
    }

    initPreview();
}

MaterialViewer::~MaterialViewer() { cleanupPreview(); }

void MaterialViewer::initPreview() {
    if (previewInitialized)
        return;

    previewTexture = LoadRenderTexture(512, 512);

    previewCamera.position   = Vector3 { 0.0f, 1.5f, 3.0f };
    previewCamera.target     = Vector3 { 0.0f, 0.0f, 0.0f };
    previewCamera.up         = Vector3 { 0.0f, 1.0f, 0.0f };
    previewCamera.fovy       = 45.0f;
    previewCamera.projection = CAMERA_PERSPECTIVE;

    Mesh sphereMesh = GenMeshSphere(1.0f, 32, 32);
    previewSphere   = LoadModelFromMesh(sphereMesh);

    previewInitialized = true;
    CORVUS_CORE_INFO("Material viewer preview initialized");
}

void MaterialViewer::cleanupPreview() {
    if (!previewInitialized)
        return;

    CORVUS_CORE_INFO("Cleaning up material viewer preview");

    if (previewSphere.meshCount > 0) {
        UnloadModel(previewSphere);
        previewSphere = { 0 };
    }

    if (previewTexture.id > 0) {
        UnloadRenderTexture(previewTexture);
        previewTexture = { 0 };
    }

    previewInitialized = false;
}

void MaterialViewer::updatePreview() {
    auto mat = materialHandle.get();
    if (!mat || !previewInitialized)
        return;

    if (previewSphere.materialCount > 0) {
        raylib::Material& raylibMat
            = *reinterpret_cast<raylib::Material*>(&previewSphere.materials[0]);
        mat->applyToRaylibMaterial(raylibMat, assetManager);
    }

    needsPreviewUpdate = false;
}

void MaterialViewer::handleCameraControls() {
    ImVec2 mousePos = ImGui::GetMousePos();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isDragging   = true;
        lastMousePos = { mousePos.x, mousePos.y };
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        isDragging = false;
    }

    if (isDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        Vector2 delta = { mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y };

        cameraAngleY += delta.x * 0.01f;
        cameraAngleX += delta.y * 0.01f;

        // Clamp vertical rotation
        if (cameraAngleX > PI / 2.0f - 0.1f)
            cameraAngleX = PI / 2.0f - 0.1f;
        if (cameraAngleX < -PI / 2.0f + 0.1f)
            cameraAngleX = -PI / 2.0f + 0.1f;

        // Update camera position
        float radius             = 3.0f;
        previewCamera.position.x = radius * cosf(cameraAngleX) * sinf(cameraAngleY);
        previewCamera.position.y = radius * sinf(cameraAngleX);
        previewCamera.position.z = radius * cosf(cameraAngleX) * cosf(cameraAngleY);

        lastMousePos = { mousePos.x, mousePos.y };
    }
}

void MaterialViewer::renderPreview() {
    if (!previewInitialized)
        return;

    updatePreview();

    BeginTextureMode(previewTexture);
    ClearBackground(Color { 45, 45, 48, 255 });

    previewLighting.clear();

    // Key light from top-front-right (standard 3-point lighting)
    Core::Systems::LightData keyLight;
    keyLight.type      = Core::Systems::LightType::Directional;
    keyLight.direction = Vector3Normalize({ -0.3f, -0.7f, -0.5f });
    keyLight.color     = WHITE;
    keyLight.intensity = 1.0f;
    previewLighting.addLight(keyLight);

    // Fill light from left (softer)
    Core::Systems::LightData fillLight;
    fillLight.type      = Core::Systems::LightType::Directional;
    fillLight.direction = Vector3Normalize({ 0.5f, -0.3f, 0.5f });
    fillLight.color     = Color { 180, 200, 220, 255 }; // Slightly blue tint
    fillLight.intensity = 0.4f;
    previewLighting.addLight(fillLight);

    // Rim light from back (highlights edges)
    Core::Systems::LightData rimLight;
    rimLight.type      = Core::Systems::LightType::Directional;
    rimLight.direction = Vector3Normalize({ 0.0f, 0.3f, 1.0f });
    rimLight.color     = WHITE;
    rimLight.intensity = 0.3f;
    previewLighting.addLight(rimLight);

    if (previewSphere.materialCount > 0) {
        auto& rayMat = reinterpret_cast<raylib::Material&>(previewSphere.materials[0]);
        previewLighting.applyToMaterial(&rayMat, Vector3 { 0, 0, 0 }, 1.0f);
    }

    BeginMode3D(previewCamera);
    // Draw subtle grid lines without the ugly built-in grid
    for (int i = -5; i <= 5; i++) {
        Color lineColor = (i == 0) ? Color { 100, 100, 100, 100 } : Color { 60, 60, 60, 60 };
        DrawLine3D({ (float)i, -0.01f, -5.0f }, { (float)i, -0.01f, 5.0f }, lineColor);
        DrawLine3D({ -5.0f, -0.01f, (float)i }, { 5.0f, -0.01f, (float)i }, lineColor);
    }
    DrawModel(previewSphere, Vector3 { 0, 0, 0 }, 1.0f, WHITE);
    EndMode3D();

    EndTextureMode();
}

void MaterialViewer::render() {
    if (!materialHandle.isValid()) {
        isOpen = false;
        return;
    }

    auto mat = materialHandle.get();
    if (!mat) {
        isOpen = false;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(windowTitle.c_str(), &isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Menu Bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
            if (materialHandle.save()) {
                CORVUS_CORE_INFO("Saved material: {}", windowTitle);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Save material changes");
        }

        if (ImGui::Button(ICON_FA_ROTATE_LEFT " Revert")) {
            materialHandle.reload();
            mat                = materialHandle.get();
            needsPreviewUpdate = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Revert to saved version");
        }

        ImGui::EndMenuBar();
    }

    renderPreview();

    // Two-column layout
    ImGui::Columns(2, "##MaterialColumns", true);
    ImGui::SetColumnWidth(0, 380);

    // Left Column: Preview
    {
        ImGui::BeginChild("##PreviewSection", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

        // Section header
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::SeparatorText(ICON_FA_EYE " Preview");
        ImGui::PopStyleVar();

        ImGui::Spacing();

        // Calculate centered preview size
        ImVec2 availRegion = ImGui::GetContentRegionAvail();
        float  previewSize = std::min(availRegion.x - 20, 340.0f);

        // Center the preview
        float offsetX = (availRegion.x - previewSize) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        ImVec2 cursorBefore = ImGui::GetCursorScreenPos();

        // Preview with border
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));
        ImGui::BeginChild(
            "##PreviewFrame", ImVec2(previewSize, previewSize), true, ImGuiWindowFlags_NoScrollbar);

        rlImGuiImageRect(
            &previewTexture.texture,
            (int)previewSize - 2,
            (int)previewSize - 2,
            Rectangle {
                0, 0, (float)previewTexture.texture.width, -(float)previewTexture.texture.height });

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // Camera controls hint
        ImVec2 cursorAfter = ImGui::GetCursorScreenPos();
        ImRect previewRect(cursorBefore,
                           ImVec2(cursorBefore.x + previewSize, cursorBefore.y + previewSize));

        if (previewRect.Contains(ImGui::GetMousePos())) {
            handleCameraControls();
        }

        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImGui::TextDisabled(ICON_FA_COMPUTER_MOUSE " Drag to rotate");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Shader info section
        ImGui::SeparatorText(ICON_FA_CODE " Shader");
        ImGui::Spacing();

        std::string shaderText = "Default";
        if (!mat->shaderAsset.is_nil()) {
            auto shaderMeta = assetManager->getMetadata(mat->shaderAsset);
            if (!shaderMeta.path.empty()) {
                shaderText = shaderMeta.path.substr(shaderMeta.path.find_last_of('/') + 1);
            } else {
                shaderText = "Custom";
            }
        }

        ImGui::Text("Shader:");
        ImGui::SameLine(80);

        // Shader selector dropdown
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##ShaderSelect", shaderText.c_str())) {
            // "Default" option (clears shader asset)
            bool isDefault = mat->shaderAsset.is_nil();
            if (ImGui::Selectable("Default", isDefault)) {
                mat->shaderAsset        = Core::UUID();
                mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
                needsPreviewUpdate      = true;
            }
            if (isDefault) {
                ImGui::SetItemDefaultFocus();
            }

            // List all available shaders
            auto allShaders = assetManager->getAllOfType<raylib::Shader>();
            for (auto& shaderHandle : allShaders) {
                auto        shaderMeta = assetManager->getMetadata(shaderHandle.getID());
                std::string shaderName = shaderMeta.path.empty()
                    ? boost::uuids::to_string(shaderMeta.id)
                    : shaderMeta.path.substr(shaderMeta.path.find_last_of('/') + 1);

                bool isSelected = (shaderHandle.getID() == mat->shaderAsset);
                if (ImGui::Selectable(shaderName.c_str(), isSelected)) {
                    mat->shaderAsset        = shaderHandle.getID();
                    mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
                    needsPreviewUpdate      = true;
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::EndChild();
    }

    ImGui::NextColumn();

    // Right Column: Properties
    {
        ImGui::BeginChild("##PropertiesSection", ImVec2(0, 0), true);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::SeparatorText(ICON_FA_SLIDERS " Material Properties");
        ImGui::PopStyleVar();

        ImGui::Spacing();

        // Properties list
        std::vector<std::string> toRemove;
        bool                     hasProperties = false;

        for (auto& [name, prop] : mat->properties) {
            hasProperties = true;
            ImGui::PushID(name.c_str());

            // Property row with colored type badge
            ImVec4      typeColor;
            const char* typeIcon;

            switch (prop.value.type) {
                case Core::MaterialPropertyType::Color:
                    typeColor = ImVec4(0.8f, 0.4f, 0.4f, 1.0f);
                    typeIcon  = ICON_FA_PALETTE;
                    break;
                case Core::MaterialPropertyType::Float:
                    typeColor = ImVec4(0.4f, 0.7f, 0.9f, 1.0f);
                    typeIcon  = ICON_FA_HASHTAG;
                    break;
                case Core::MaterialPropertyType::Texture:
                    typeColor = ImVec4(0.7f, 0.5f, 0.9f, 1.0f);
                    typeIcon  = ICON_FA_IMAGE;
                    break;
                case Core::MaterialPropertyType::Vector2:
                case Core::MaterialPropertyType::Vector3:
                case Core::MaterialPropertyType::Vector4:
                    typeColor = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);
                    typeIcon  = ICON_FA_VECTOR_SQUARE;
                    break;
                case Core::MaterialPropertyType::Int:
                    typeColor = ImVec4(0.4f, 0.7f, 0.9f, 1.0f);
                    typeIcon  = ICON_FA_HASHTAG;
                    break;
                case Core::MaterialPropertyType::Bool:
                    typeColor = ImVec4(0.9f, 0.7f, 0.4f, 1.0f);
                    typeIcon  = ICON_FA_CHECK;
                    break;
                default:
                    typeColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                    typeIcon  = ICON_FA_CIRCLE_QUESTION;
                    break;
            }

            // Type badge
            ImGui::PushStyleColor(ImGuiCol_Button, typeColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, typeColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, typeColor);
            ImGui::SmallButton(typeIcon);
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            ImGui::Text("%s", name.c_str());

            ImGui::Spacing();
            ImGui::Indent(30);

            bool changed = false;
            switch (prop.value.type) {
                case Core::MaterialPropertyType::Color:
                    changed = renderColorProperty(name, prop);
                    break;
                case Core::MaterialPropertyType::Float:
                    changed = renderFloatProperty(name, prop);
                    break;
                case Core::MaterialPropertyType::Texture:
                    changed = renderTextureProperty(name, prop);
                    break;
                case Core::MaterialPropertyType::Vector3:
                    changed = renderVectorProperty(name, prop);
                    break;
                default:
                    ImGui::TextDisabled("(unsupported type in viewer)");
                    break;
            }

            ImGui::Unindent(30);

            // Delete button
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::SmallButton(ICON_FA_TRASH)) {
                toRemove.push_back(name);
                needsPreviewUpdate = true;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Delete property");
            }

            if (changed) {
                needsPreviewUpdate = true;
            }

            ImGui::PopID();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Remove deleted properties
        for (const auto& name : toRemove) {
            mat->properties.erase(name);
        }

        // Empty state message
        if (!hasProperties) {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextWrapped("No properties yet. Add properties using the button below.");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Add property button (at bottom)
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
        if (ImGui::Button(ICON_FA_PLUS " Add Property", ImVec2(-1, 30))) {
            showAddPropertyPopup  = true;
            propertyNameBuffer[0] = '\0';
        }

        ImGui::EndChild();
    }

    ImGui::Columns(1);

    renderAddPropertyPopup();

    ImGui::End();
}

bool MaterialViewer::renderColorProperty(const std::string& name, Core::MaterialProperty& prop) {
    Color color  = prop.value.getColor();
    float col[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };

    ImGui::SetNextItemWidth(-30);
    if (ImGui::ColorEdit4(std::format("##{}_color", name).c_str(),
                          col,
                          ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar)) {
        Color newColor = { static_cast<unsigned char>(col[0] * 255),
                           static_cast<unsigned char>(col[1] * 255),
                           static_cast<unsigned char>(col[2] * 255),
                           static_cast<unsigned char>(col[3] * 255) };
        prop.value     = Core::MaterialPropertyValue(newColor);
        return true;
    }
    return false;
}

bool MaterialViewer::renderFloatProperty(const std::string& name, Core::MaterialProperty& prop) {
    float value = prop.value.getFloat();
    ImGui::SetNextItemWidth(-30);
    if (ImGui::SliderFloat(std::format("##{}_float", name).c_str(), &value, 0.0f, 1.0f, "%.3f")) {
        prop.value = Core::MaterialPropertyValue(value);
        return true;
    }
    return false;
}

bool MaterialViewer::renderTextureProperty(const std::string& name, Core::MaterialProperty& prop) {
    Core::UUID texID = prop.value.getTexture();
    int        slot  = prop.value.getTextureSlot();

    std::string buttonText = "None";
    if (!texID.is_nil()) {
        auto texMeta = assetManager->getMetadata(texID);
        buttonText   = texMeta.path.substr(texMeta.path.find_last_of('/') + 1);
    }

    bool changed = false;

    // Texture dropdown
    ImGui::SetNextItemWidth(-100);
    if (ImGui::BeginCombo(std::format("##{}_texture", name).c_str(), buttonText.c_str())) {
        if (ImGui::Selectable("None", texID.is_nil())) {
            auto mat = materialHandle.get();
            mat->setTexture(name, Core::UUID(), slot);
            changed                 = true;
            mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
        }

        auto allTextures = assetManager->getAllOfType<raylib::Texture>();
        for (auto& texHandle : allTextures) {
            auto        texMeta = assetManager->getMetadata(texHandle.getID());
            std::string texName = texMeta.path.substr(texMeta.path.find_last_of('/') + 1);

            bool isSelected = (texHandle.getID() == texID);
            if (ImGui::Selectable(texName.c_str(), isSelected)) {
                auto mat = materialHandle.get();
                mat->setTexture(name, texHandle.getID(), slot);
                changed                 = true;
                mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    // Slot controls
    ImGui::SameLine();
    ImGui::Text("Slot:");
    ImGui::SameLine();

    ImGui::PushButtonRepeat(true);
    if (ImGui::SmallButton(std::format("-##{}", name).c_str())) {
        if (slot > 0) {
            slot--;
            auto mat = materialHandle.get();
            mat->setTexture(name, texID, slot);
            changed                 = true;
            mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
        }
    }

    ImGui::SameLine();
    ImGui::Text("%d", slot);

    ImGui::SameLine();
    if (ImGui::SmallButton(std::format("+##{}", name).c_str())) {
        if (slot < 10) {
            slot++;
            auto mat = materialHandle.get();
            mat->setTexture(name, texID, slot);
            changed                 = true;
            mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
        }
    }
    ImGui::PopButtonRepeat();

    return changed;
}

bool MaterialViewer::renderVectorProperty(const std::string& name, Core::MaterialProperty& prop) {
    Vector3 vec  = prop.value.getVector3();
    float   v[3] = { vec.x, vec.y, vec.z };

    ImGui::SetNextItemWidth(-30);
    if (ImGui::DragFloat3(std::format("##{}_vec3", name).c_str(), v, 0.01f, 0.0f, 0.0f, "%.2f")) {
        prop.value = Core::MaterialPropertyValue(Vector3 { v[0], v[1], v[2] });
        return true;
    }
    return false;
}

void MaterialViewer::renderAddPropertyPopup() {
    if (showAddPropertyPopup) {
        ImGui::OpenPopup("Add Property");
        showAddPropertyPopup = false;
    }

    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Add Property", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto mat = materialHandle.get();

        ImGui::Text("Property Name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##PropName", propertyNameBuffer.data(), propertyNameBuffer.size());

        ImGui::Spacing();
        ImGui::SeparatorText("Type");
        ImGui::Spacing();

        // Type buttons in grid
        ImVec2 buttonSize(135, 40);

        if (ImGui::Button(ICON_FA_PALETTE " Color", buttonSize)) {
            std::string propName(propertyNameBuffer.data());
            if (!propName.empty()) {
                mat->setColor(propName, WHITE);
                needsPreviewUpdate = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_HASHTAG " Float", buttonSize)) {
            std::string propName(propertyNameBuffer.data());
            if (!propName.empty()) {
                mat->setFloat(propName, 0.5f);
                needsPreviewUpdate = true;
                ImGui::CloseCurrentPopup();
            }
        }

        if (ImGui::Button(ICON_FA_VECTOR_SQUARE " Vector3", buttonSize)) {
            std::string propName(propertyNameBuffer.data());
            if (!propName.empty()) {
                mat->setVector(propName, Vector3 { 0, 0, 0 });
                needsPreviewUpdate = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_IMAGE " Texture", buttonSize)) {
            std::string propName(propertyNameBuffer.data());
            if (!propName.empty()) {
                mat->setTexture(propName, Core::UUID(), 0);
                needsPreviewUpdate = true;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}
