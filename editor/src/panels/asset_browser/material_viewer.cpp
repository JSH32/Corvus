#include "editor/panels/asset_browser/material_viewer.hpp"
#include "IconsFontAwesome6.h"
#include "imgui_internal.h"
#include "linp/log.hpp"
#include "rlImGui.h"
#include <format>

namespace Linp::Editor {

MaterialViewer::MaterialViewer(const Core::UUID& id, Core::AssetManager* manager)
    : AssetViewer(id, manager, "Material Viewer") {
    materialHandle = assetManager->loadByID<Core::Material>(id);
    if (materialHandle.isValid()) {
        // Get material name from file path, not from material's name property
        auto meta = assetManager->getMetadata(id);
        windowTitle
            = std::format("Material: {}", meta.path.substr(meta.path.find_last_of('/') + 1));
    }

    initPreview();
}

MaterialViewer::~MaterialViewer() { cleanupPreview(); }

void MaterialViewer::initPreview() {
    if (previewInitialized)
        return;

    // Create preview render texture
    previewTexture = LoadRenderTexture(512, 512);

    // Setup preview camera
    previewCamera.position   = Vector3 { 0.0f, 1.5f, 3.0f };
    previewCamera.target     = Vector3 { 0.0f, 0.0f, 0.0f };
    previewCamera.up         = Vector3 { 0.0f, 1.0f, 0.0f };
    previewCamera.fovy       = 45.0f;
    previewCamera.projection = CAMERA_PERSPECTIVE;

    // Create preview sphere
    Mesh sphereMesh = GenMeshSphere(1.0f, 32, 32);
    previewSphere   = LoadModelFromMesh(sphereMesh);

    previewInitialized = true;
    LINP_CORE_INFO("Material viewer preview initialized");
}

void MaterialViewer::cleanupPreview() {
    if (!previewInitialized)
        return;

    LINP_CORE_INFO("Cleaning up material viewer preview");

    // Unload in reverse order of creation
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

    // Get the raylib material from the model
    if (previewSphere.materialCount > 0) {
        raylib::Material& raylibMat
            = *reinterpret_cast<raylib::Material*>(&previewSphere.materials[0]);

        // Apply our material properties
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
    ClearBackground(Color { 35, 35, 38, 255 });

    BeginMode3D(previewCamera);
    DrawGrid(10, 1.0f);
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

    ImGui::SetNextWindowSize(ImVec2(750, 650), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(windowTitle.c_str(), &isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Menu Bar at the Top
    if (ImGui::BeginMenuBar()) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

        // Save button with green tint
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.3f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));

        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
            if (materialHandle.save()) {
                LINP_CORE_INFO("Saved material: {}", windowTitle);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Save material changes");
        }

        ImGui::PopStyleColor(3);

        // Revert button with orange/yellow tint
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.4f, 0.2f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.5f, 0.3f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.4f, 0.2f, 1.0f));

        if (ImGui::Button(ICON_FA_ROTATE_LEFT " Revert")) {
            materialHandle.reload();
            mat                = materialHandle.get();
            needsPreviewUpdate = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Revert to saved version");
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::EndMenuBar();
    }

    // Render preview
    renderPreview();

    // Top Section: Preview and Basic Info
    ImGui::BeginChild("TopSection", ImVec2(0, 350), true);
    {
        // Preview on left
        ImGui::BeginChild("PreviewArea", ImVec2(350, 0), false);
        {
            ImVec2 previewSize = ImGui::GetContentRegionAvail();
            float  size        = std::min(previewSize.x, previewSize.y);

            ImVec2 cursorBefore = ImGui::GetCursorScreenPos();

            // Display preview texture
            rlImGuiImageRect(&previewTexture.texture,
                             (int)size,
                             (int)size,
                             Rectangle { 0,
                                         0,
                                         (float)previewTexture.texture.width,
                                         -(float)previewTexture.texture.height });

            // Check if mouse is over preview for camera controls
            ImVec2 cursorAfter = ImGui::GetCursorScreenPos();
            ImRect previewRect(cursorBefore, ImVec2(cursorBefore.x + size, cursorBefore.y + size));

            if (previewRect.Contains(ImGui::GetMousePos())) {
                handleCameraControls();
                ImGui::SetTooltip("Drag to rotate camera");
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Info on right
        ImGui::BeginChild("InfoArea", ImVec2(0, 0), false);
        {
            // Display material file path (read-only)
            auto meta = assetManager->getMetadata(assetID);
            ImGui::TextDisabled("File Path");
            ImGui::TextWrapped("%s", meta.path.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Shader selection dropdown
            ImGui::Text("Shader");
            std::string shaderName = "Default Shader";
            if (!mat->shaderAsset.is_nil()) {
                auto shaderMeta = assetManager->getMetadata(mat->shaderAsset);
                shaderName      = shaderMeta.path.substr(shaderMeta.path.find_last_of('/') + 1);
            }

            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##ShaderSelect", shaderName.c_str())) {
                // "Default Shader" option
                if (ImGui::Selectable("Default Shader", mat->shaderAsset.is_nil())) {
                    mat->shaderAsset        = Core::UUID();
                    mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
                    needsPreviewUpdate      = true;
                }

                // List all available shaders
                auto allShaders = assetManager->getAllOfType<raylib::Shader>();
                for (auto& shaderHandle : allShaders) {
                    auto        shaderMeta = assetManager->getMetadata(shaderHandle.getID());
                    std::string shaderDisplayName
                        = shaderMeta.path.substr(shaderMeta.path.find_last_of('/') + 1);

                    bool isSelected = (shaderHandle.getID() == mat->shaderAsset);
                    if (ImGui::Selectable(shaderDisplayName.c_str(), isSelected)) {
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

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Render Settings
            ImGui::Text("Render Settings");
            if (ImGui::Checkbox("Double Sided", &mat->doubleSided)) {
                needsPreviewUpdate = true;
            }
            if (ImGui::Checkbox("Alpha Blend", &mat->alphaBlend)) {
                needsPreviewUpdate = true;
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // --- Bottom Section: Properties ---
    ImGui::BeginChild("PropertiesSection", ImVec2(0, 0), true);
    {
        ImGui::Text("Properties");
        ImGui::Separator();
        ImGui::Spacing();

        std::vector<std::string> toRemove;

        // Calculate the maximum label width for alignment
        float labelWidth = 120.0f;

        for (auto& [name, prop] : mat->properties) {
            ImGui::PushID(name.c_str());

            bool changed = false;

            // Left column: property name (aligned)
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", name.c_str());

            ImGui::SameLine(labelWidth);

            // Right column: property value
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
                    ImGui::Text("(unsupported type)");
                    break;
            }

            // Delete button at the end
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.3f, 0.3f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::SmallButton(ICON_FA_TRASH)) {
                toRemove.push_back(name);
                needsPreviewUpdate = true;
            }
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Delete property");
            }

            if (changed) {
                needsPreviewUpdate = true;
            }

            ImGui::PopID();
            ImGui::Spacing();
        }

        for (const auto& name : toRemove) {
            mat->properties.erase(name);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button(ICON_FA_PLUS " Add Property", ImVec2(-1, 0))) {
            showAddPropertyPopup  = true;
            propertyNameBuffer[0] = '\0';
        }
    }
    ImGui::EndChild();

    renderAddPropertyPopup();

    ImGui::End();
}

bool MaterialViewer::renderColorProperty(const std::string& name, Core::MaterialProperty& prop) {
    Color color  = prop.value.getColor();
    float col[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };

    ImGui::SetNextItemWidth(300);
    if (ImGui::ColorEdit4(
            std::format("##{}_color", name).c_str(), col, ImGuiColorEditFlags_NoLabel)) {
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
    ImGui::SetNextItemWidth(300);
    if (ImGui::SliderFloat(std::format("##{}_float", name).c_str(), &value, 0.0f, 1.0f)) {
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

    // Dropdown combo for texture selection (wider to prevent cutoff)
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo(std::format("##{}_texture", name).c_str(), buttonText.c_str())) {
        // "None" option to clear texture
        if (ImGui::Selectable("None", texID.is_nil())) {
            auto mat = materialHandle.get();
            mat->setTexture(name, Core::UUID(), slot);
            changed = true;
            // Force shader to update by clearing cached handle
            mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
        }

        // List all available textures
        auto allTextures = assetManager->getAllOfType<raylib::Texture>();
        for (auto& texHandle : allTextures) {
            auto        texMeta = assetManager->getMetadata(texHandle.getID());
            std::string texName = texMeta.path.substr(texMeta.path.find_last_of('/') + 1);

            bool isSelected = (texHandle.getID() == texID);
            if (ImGui::Selectable(texName.c_str(), isSelected)) {
                auto mat = materialHandle.get();
                mat->setTexture(name, texHandle.getID(), slot);
                changed = true;
                // Force shader to update by clearing cached handle
                mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    // Texture slot controls with better spacing
    ImGui::SameLine();
    ImGui::Text("Slot:");
    ImGui::SameLine();

    // Use button repeat for increment/decrement
    ImGui::PushButtonRepeat(true);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));

    if (ImGui::SmallButton(std::format("{}##{}_minus", ICON_FA_MINUS, name).c_str())) {
        if (slot > 0) {
            slot--;
            auto mat = materialHandle.get();
            mat->setTexture(name, texID, slot);
            changed = true;
            // Force shader to update by clearing cached handle
            mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Decrease slot");
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(30);
    ImGui::Text("%d", slot);

    ImGui::SameLine();
    if (ImGui::SmallButton(std::format("{}##{}_plus", ICON_FA_PLUS, name).c_str())) {
        if (slot < 10) {
            slot++;
            auto mat = materialHandle.get();
            mat->setTexture(name, texID, slot);
            changed = true;
            // Force shader to update by clearing cached handle
            mat->cachedShaderHandle = Core::AssetHandle<raylib::Shader>();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Increase slot");
    }

    ImGui::PopStyleVar();
    ImGui::PopButtonRepeat();

    return changed;
}

bool MaterialViewer::renderVectorProperty(const std::string& name, Core::MaterialProperty& prop) {
    Vector3 vec  = prop.value.getVector3();
    float   v[3] = { vec.x, vec.y, vec.z };

    ImGui::SetNextItemWidth(300);
    if (ImGui::DragFloat3(std::format("##{}_vec3", name).c_str(), v, 0.01f)) {
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

    if (ImGui::BeginPopupModal("Add Property", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto mat = materialHandle.get();

        ImGui::InputText("Property Name", propertyNameBuffer.data(), propertyNameBuffer.size());
        ImGui::Spacing();

        if (ImGui::Button("Color", ImVec2(120, 0))) {
            std::string propName(propertyNameBuffer.data());
            if (!propName.empty()) {
                mat->setColor(propName, WHITE);
                needsPreviewUpdate = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Float", ImVec2(120, 0))) {
            std::string propName(propertyNameBuffer.data());
            if (!propName.empty()) {
                mat->setFloat(propName, 0.5f);
                needsPreviewUpdate = true;
                ImGui::CloseCurrentPopup();
            }
        }

        if (ImGui::Button("Vector3", ImVec2(120, 0))) {
            std::string propName(propertyNameBuffer.data());
            if (!propName.empty()) {
                mat->setVector(propName, Vector3 { 0, 0, 0 });
                needsPreviewUpdate = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Texture", ImVec2(120, 0))) {
            std::string propName(propertyNameBuffer.data());
            if (!propName.empty()) {
                mat->setTexture(propName, Core::UUID(), 0); // Default to slot 0
                needsPreviewUpdate = true;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}
