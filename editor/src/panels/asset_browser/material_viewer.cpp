#include "editor/panels/asset_browser/material_viewer.hpp"
#include "IconsFontAwesome6.h"
#include "corvus/log.hpp"
#include "editor/imguiutils.hpp"
#include "imgui_internal.h"
#include <format>
#include <glm/gtc/matrix_transform.hpp>

namespace Corvus::Editor {

MaterialViewer::MaterialViewer(const Core::UUID&          id,
                               Core::AssetManager*        manager,
                               Graphics::GraphicsContext& context)
    : AssetViewer(id, manager, "Material Viewer"), context_(&context), sceneRenderer(context) {

    materialHandle = assetManager->loadByID<Core::MaterialAsset>(id);
    initPreview();
}

MaterialViewer::~MaterialViewer() { cleanupPreview(); }

void MaterialViewer::initPreview() {
    if (previewInitialized)
        return;

    // Create render target
    colorTexture = context_->createTexture2D(previewResolution, previewResolution);
    depthTexture = context_->createDepthTexture(previewResolution, previewResolution);

    framebuffer = context_->createFramebuffer(previewResolution, previewResolution);
    framebuffer.attachTexture2D(colorTexture, 0);
    framebuffer.attachDepthTexture(depthTexture);

    // Setup fake mesh renderer component
    // Set it to Sphere primitive so it will generate the sphere mesh
    previewMeshRenderer.primitiveType        = Core::Components::PrimitiveType::Sphere;
    previewMeshRenderer.params.sphere.radius = 1.0f;
    previewMeshRenderer.params.sphere.rings  = 32;
    previewMeshRenderer.params.sphere.slices = 32;
    previewMeshRenderer.materialHandle       = materialHandle; // Use the material handle

    previewTransform.position = glm::vec3(0.0f);
    previewTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    previewTransform.scale    = glm::vec3(1.0f);

    // Setup camera
    previewCamera.setPosition(glm::vec3(0.0f, 1.5f, 3.0f));
    previewCamera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    previewCamera.setPerspective(45.0f, 1.0f, 0.1f, 100.0f);

    // Initialize lighting system
    previewLighting.initialize(*context_);
    setupPreviewLights();

    previewInitialized = true;
    CORVUS_CORE_INFO("Material viewer preview initialized");
}

void MaterialViewer::cleanupPreview() {
    if (!previewInitialized)
        return;

    CORVUS_CORE_INFO("Cleaning up material viewer preview");

    previewInitialized = false;
    previewLighting.shutdown();

    context_->flush();

    // The mesh renderer component will clean up its own generated model
    colorTexture.release();
    depthTexture.release();
    framebuffer.release();
}

void MaterialViewer::setupPreviewLights() {
    previewLighting.clear();

    // Key light from top-front-right (standard 3-point lighting)
    Renderer::Light keyLight;
    keyLight.type      = Renderer::LightType::Directional;
    keyLight.direction = glm::normalize(glm::vec3(-0.3f, -0.7f, -0.5f));
    keyLight.color     = glm::vec3(1.0f, 1.0f, 1.0f);
    keyLight.intensity = 1.0f;
    previewLighting.addLight(keyLight);

    // Fill light from left (softer, slightly blue)
    Renderer::Light fillLight;
    fillLight.type      = Renderer::LightType::Directional;
    fillLight.direction = glm::normalize(glm::vec3(0.5f, -0.3f, 0.5f));
    fillLight.color     = glm::vec3(0.7f, 0.78f, 0.86f);
    fillLight.intensity = 0.4f;
    previewLighting.addLight(fillLight);

    // Rim light from back (highlights edges)
    Renderer::Light rimLight;
    rimLight.type      = Renderer::LightType::Directional;
    rimLight.direction = glm::normalize(glm::vec3(0.0f, 0.3f, 1.0f));
    rimLight.color     = glm::vec3(1.0f, 1.0f, 1.0f);
    rimLight.intensity = 0.3f;
    previewLighting.addLight(rimLight);

    // Ambient light
    previewLighting.setAmbientColor(glm::vec3(0.1f, 0.1f, 0.12f));
}

void MaterialViewer::updateCameraPosition() {
    previewCamera.setPosition(glm::vec3(cameraDistance * cosf(cameraAngleX) * sinf(cameraAngleY),
                                        cameraDistance * sinf(cameraAngleX),
                                        cameraDistance * cosf(cameraAngleX) * cosf(cameraAngleY)));
    previewCamera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

void MaterialViewer::updatePreview() {
    auto mat = materialHandle.get();
    if (!mat || !previewInitialized)
        return;

    needsPreviewUpdate = false;
}

void MaterialViewer::handleCameraControls() {
    ImVec2 mousePos = ImGui::GetMousePos();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isDragging   = true;
        lastMousePos = mousePos;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        isDragging = false;
    }

    if (isDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImVec2(mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y);

        cameraAngleY += delta.x * 0.01f;
        cameraAngleX += delta.y * 0.01f;

        // Clamp vertical rotation
        constexpr float PI = 3.14159265359f;
        if (cameraAngleX > PI / 2.0f - 0.1f)
            cameraAngleX = PI / 2.0f - 0.1f;
        if (cameraAngleX < -PI / 2.0f + 0.1f)
            cameraAngleX = -PI / 2.0f + 0.1f;

        updateCameraPosition();
        lastMousePos = mousePos;
    }

    // Mouse wheel zoom
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
        cameraDistance -= wheel * 0.3f;
        cameraDistance = glm::clamp(cameraDistance, 1.5f, 10.0f);
        updateCameraPosition();
    }
}

void MaterialViewer::renderPreview() {
    if (!previewInitialized)
        return;

    updatePreview();

    // Clear frmaebuffer before rendering
    auto clearCmd = context_->createCommandBuffer();
    clearCmd.begin();
    clearCmd.bindFramebuffer(framebuffer);
    clearCmd.setViewport(0, 0, previewResolution, previewResolution);
    clearCmd.clear(0.176f, 0.176f, 0.188f, 1.0f, true); // Clear color + depth
    clearCmd.unbindFramebuffer();
    clearCmd.end();
    clearCmd.submit();

    // Create a fake renderable entity
    std::vector<Renderer::RenderableEntity> renderables;
    Renderer::RenderableEntity              renderable;
    renderable.meshRenderer = &previewMeshRenderer;
    renderable.transform    = &previewTransform;
    renderable.isEnabled    = true;
    renderables.push_back(renderable);

    // Let the scene renderer handle everything!
    sceneRenderer.render(renderables, previewCamera, assetManager, &previewLighting, &framebuffer);
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

    std::string title = "";
    if (materialHandle.isValid()) {
        auto meta = assetManager->getMetadata(materialHandle.getID());
        title     = std::format(
            "{} Material: {}", ICON_FA_PALETTE, meta.path.substr(meta.path.find_last_of('/') + 1));
    }

    if (!ImGui::Begin(title.c_str(), &isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Menu Bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
            if (materialHandle.save()) {
                CORVUS_CORE_INFO("Saved material: {}", title);
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

    if (previewInitialized) {
        renderPreview();
    }

    // Two-column layout
    ImGui::Columns(2, "##MaterialColumns", true);
    ImGui::SetColumnWidth(0, 380);

    // Left Column: Preview
    {
        ImGui::BeginChild("##PreviewSection", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

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

        // Display the rendered texture using the helper
        ImGui::RenderFramebuffer(framebuffer,
                                 colorTexture,
                                 ImVec2(previewSize - 2, previewSize - 2),
                                 true // flipY = true for OpenGL
        );

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
        ImGui::TextDisabled(ICON_FA_COMPUTER_MOUSE " Drag to rotate • Scroll to zoom");

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
                mat->shaderAsset   = Core::UUID();
                needsPreviewUpdate = true;
            }
            if (isDefault) {
                ImGui::SetItemDefaultFocus();
            }

            // List all available shaders
            auto allShaders = assetManager->getAllOfType<Graphics::Shader>();
            for (auto& shaderHandle : allShaders) {
                auto        shaderMeta = assetManager->getMetadata(shaderHandle.getID());
                std::string shaderName = shaderMeta.path.empty()
                    ? boost::uuids::to_string(shaderMeta.id)
                    : shaderMeta.path.substr(shaderMeta.path.find_last_of('/') + 1);

                bool isSelected = (shaderHandle.getID() == mat->shaderAsset);
                if (ImGui::Selectable(shaderName.c_str(), isSelected)) {
                    mat->shaderAsset   = shaderHandle.getID();
                    needsPreviewUpdate = true;
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
                    typeColor = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);
                    typeIcon  = ICON_FA_VECTOR_SQUARE;
                    break;
                case Core::MaterialPropertyType::Vector4:
                    typeColor = ImVec4(0.8f, 0.4f, 0.4f, 1.0f);
                    typeIcon  = ICON_FA_PALETTE;
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
                case Core::MaterialPropertyType::Vector4:
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
    glm::vec4 color  = prop.value.getVector4();
    float     col[4] = { color.r, color.g, color.b, color.a };

    ImGui::SetNextItemWidth(-30);
    if (ImGui::ColorEdit4(std::format("##{}_color", name).c_str(),
                          col,
                          ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar)) {
        prop.value = Core::MaterialPropertyValue(glm::vec4(col[0], col[1], col[2], col[3]));
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
            changed = true;
        }

        auto allTextures = assetManager->getAllOfType<Graphics::Texture2D>();
        for (auto& texHandle : allTextures) {
            auto        texMeta = assetManager->getMetadata(texHandle.getID());
            std::string texName = texMeta.path.substr(texMeta.path.find_last_of('/') + 1);

            bool isSelected = (texHandle.getID() == texID);
            if (ImGui::Selectable(texName.c_str(), isSelected)) {
                auto mat = materialHandle.get();
                mat->setTexture(name, texHandle.getID(), slot);
                changed = true;
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
            changed = true;
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
            changed = true;
        }
    }
    ImGui::PopButtonRepeat();

    return changed;
}

bool MaterialViewer::renderVectorProperty(const std::string& name, Core::MaterialProperty& prop) {
    glm::vec3 vec  = prop.value.getVector3();
    float     v[3] = { vec.x, vec.y, vec.z };

    ImGui::SetNextItemWidth(-30);
    if (ImGui::DragFloat3(std::format("##{}_vec3", name).c_str(), v, 0.01f, 0.0f, 0.0f, "%.2f")) {
        prop.value = Core::MaterialPropertyValue(glm::vec3(v[0], v[1], v[2]));
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
                mat->setVector4(propName, glm::vec4(1.0f));
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
                mat->setVector3(propName, glm::vec3(0.0f));
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

} // namespace Corvus::Editor
