#include "editor/panels/asset_browser/material_viewer.hpp"
#include "IconsFontAwesome6.h"
#include "corvus/log.hpp"
#include "editor/imguiutils.hpp"
#include "imgui_internal.h"
#include <format>

namespace Corvus::Editor {

MaterialViewer::MaterialViewer(const Core::UUID&          id,
                               Core::AssetManager*        manager,
                               Graphics::GraphicsContext& context)
    : AssetViewer(id, manager, "Material Viewer"), context_(&context), sceneRenderer(context) {

    materialHandle = assetManager->loadByID<Core::MaterialAsset>(id);
    colorTexture   = context_->createTexture2D(previewResolution, previewResolution);
    depthTexture   = context_->createDepthTexture(previewResolution, previewResolution);

    framebuffer = context_->createFramebuffer(previewResolution, previewResolution);
    framebuffer.attachTexture2D(colorTexture, 0);
    framebuffer.attachDepthTexture(depthTexture);

    previewModel = Renderer::ModelGenerator::createSphere(*context_, 1.0f, 32, 32);

    previewCamera.setPosition(glm::vec3(0.0f, 1.5f, 3.0f));
    previewCamera.lookAt(glm::vec3(0.0f));
    previewCamera.setPerspective(45.0f, 1.0f, 0.1f, 100.0f);

    setupPreviewLights();

    CORVUS_CORE_INFO("Material viewer preview initialized for {}", materialHandle.getPath());
}

MaterialViewer::~MaterialViewer() {
    sceneRenderer.getLighting().shutdown();
    context_->flush();
    previewModel.release();
    colorTexture.release();
    depthTexture.release();
    framebuffer.release();
}

void MaterialViewer::setupPreviewLights() {
    sceneRenderer.clearLights();

    auto makeLight = [&](glm::vec3 dir, glm::vec3 color, float intensity) {
        Renderer::Light l;
        l.type      = Renderer::LightType::Directional;
        l.direction = glm::normalize(dir);
        l.color     = color;
        l.intensity = intensity;
        sceneRenderer.addLight(l);
    };

    makeLight({ -0.3f, -0.7f, -0.5f }, { 1, 1, 1 }, 1.0f);
    makeLight({ 0.5f, -0.3f, 0.5f }, { 0.7f, 0.78f, 0.86f }, 0.4f);
    makeLight({ 0.0f, 0.3f, 1.0f }, { 1, 1, 1 }, 0.3f);

    sceneRenderer.setAmbientColor(glm::vec3(0.1f, 0.1f, 0.12f));
}

void MaterialViewer::updateCameraPosition() {
    previewCamera.setPosition(glm::vec3(cameraDistance * cosf(cameraAngleX) * sinf(cameraAngleY),
                                        cameraDistance * sinf(cameraAngleX),
                                        cameraDistance * cosf(cameraAngleX) * cosf(cameraAngleY)));
    previewCamera.lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

void MaterialViewer::updatePreview() {
    if (const auto mat = materialHandle.get(); !mat)
        return;
    needsPreviewUpdate = false;
}

void MaterialViewer::handleCameraControls() {
    const ImVec2 mousePos = ImGui::GetMousePos();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isDragging   = true;
        lastMousePos = mousePos;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        isDragging = false;
    }

    if (isDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 delta = ImVec2(mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y);
        cameraAngleY += delta.x * 0.01f;
        cameraAngleX += delta.y * 0.01f;

        constexpr float PI = 3.14159265359f;
        cameraAngleX       = glm::clamp(cameraAngleX, -PI / 2.0f + 0.1f, PI / 2.0f - 0.1f);
        updateCameraPosition();
        lastMousePos = mousePos;
    }

    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
        cameraDistance = glm::clamp(cameraDistance - wheel * 0.3f, 1.5f, 10.0f);
        updateCameraPosition();
    }
}

void MaterialViewer::renderPreview() {
    updatePreview();
    const auto mat = materialHandle.get();
    if (!mat)
        return;

    sceneRenderer.clear(glm::vec4(0.176f, 0.176f, 0.188f, 1.0f), true, &framebuffer);

    Renderer::Material* material
        = sceneRenderer.getMaterialRenderer().getMaterialFromAsset(*mat, assetManager);
    if (!material)
        return;

    Renderer::Renderable renderable;
    renderable.model          = &previewModel;
    renderable.material       = material;
    renderable.transform      = previewTransform;
    renderable.position       = glm::vec3(0.0f);
    renderable.boundingRadius = 1.0f;
    renderable.enabled        = true;

    sceneRenderer.render({ renderable }, previewCamera, &framebuffer);
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
    auto        meta  = assetManager->getMetadata(materialHandle.getID());
    std::string title = fmt::format(
        "{} Material: {}", ICON_FA_PALETTE, meta.path.substr(meta.path.find_last_of('/') + 1));

    if (!ImGui::Begin(title.c_str(), &isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // === MENU BAR ===
    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save") && materialHandle.save())
            CORVUS_CORE_INFO("Saved material: {}", title);
        if (ImGui::Button(ICON_FA_ROTATE_LEFT " Revert")) {
            materialHandle.reload();
            mat                = materialHandle.get();
            needsPreviewUpdate = true;
        }
        ImGui::EndMenuBar();
    }

    renderPreview();

    // === LAYOUT ===
    ImGui::Columns(2, "##MaterialColumns", true);
    ImGui::SetColumnWidth(0, 380);

    // === LEFT: Preview ===
    ImGui::BeginChild("##PreviewSection", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::SeparatorText(ICON_FA_EYE " Preview");
    ImGui::Spacing();
    ImVec2 avail       = ImGui::GetContentRegionAvail();
    float  previewSize = std::min(avail.x - 20, 340.0f);
    float  offsetX     = (avail.x - previewSize) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::BeginChild("##PreviewFrame", ImVec2(previewSize, previewSize), true);
    ImGui::RenderFramebuffer(
        framebuffer, colorTexture, ImVec2(previewSize - 2, previewSize - 2), true);
    ImGui::EndChild();

    ImRect previewRect(start, { start.x + previewSize, start.y + previewSize });
    if (previewRect.Contains(ImGui::GetMousePos()))
        handleCameraControls();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    ImGui::TextDisabled(ICON_FA_COMPUTER_MOUSE " Drag to rotate • Scroll to zoom");

    ImGui::SeparatorText(ICON_FA_CODE " Shader");
    std::string shaderText = "Default";
    if (!mat->getShaderAsset().is_nil()) {
        auto shaderMeta = assetManager->getMetadata(mat->getShaderAsset());
        if (!shaderMeta.path.empty())
            shaderText = shaderMeta.path.substr(shaderMeta.path.find_last_of('/') + 1);
    }
    ImGui::Text("Shader:");
    ImGui::SameLine(80);

    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##ShaderSelect", shaderText.c_str())) {
        bool isDefault = mat->getShaderAsset().is_nil();
        if (ImGui::Selectable("Default", isDefault))
            mat->setProperty("shader", Core::MaterialPropertyValue(Core::UUID()));
        for (auto  allShaders = assetManager->getAllOfType<Graphics::Shader>();
             auto& shaderHandle : allShaders) {
            auto        shaderMeta = assetManager->getMetadata(shaderHandle.getID());
            std::string name       = shaderMeta.path.substr(shaderMeta.path.find_last_of('/') + 1);
            bool        selected   = (shaderHandle.getID() == mat->getShaderAsset());
            if (ImGui::Selectable(name.c_str(), selected)) {
                mat->setProperty("shader", Core::MaterialPropertyValue(shaderHandle.getID()));
                needsPreviewUpdate = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndChild();

    ImGui::NextColumn();
    ImGui::BeginChild("##PropertiesSection", ImVec2(0, 0), true);
    ImGui::SeparatorText(ICON_FA_SLIDERS " Material Properties");

    std::vector<std::string> toRemove;
    bool                     hasProps = false;

    mat->forEachProperty([&](const std::string& name, const Core::MaterialProperty& prop) {
        hasProps = true;
        ImGui::PushID(name.c_str());
        ImVec4 color;
        auto   icon = ICON_FA_CIRCLE_QUESTION;
        switch (prop.value.type) {
            case Core::MaterialPropertyType::Float:
                color = { 0.4, 0.7, 0.9, 1 };
                icon  = ICON_FA_HASHTAG;
                break;
            case Core::MaterialPropertyType::Vector3:
                color = { 0.5, 0.8, 0.5, 1 };
                icon  = ICON_FA_VECTOR_SQUARE;
                break;
            case Core::MaterialPropertyType::Vector4:
                color = { 0.8, 0.4, 0.4, 1 };
                icon  = ICON_FA_PALETTE;
                break;
            case Core::MaterialPropertyType::Texture:
                color = { 0.7, 0.5, 0.9, 1 };
                icon  = ICON_FA_IMAGE;
                break;
            default:
                color = { 0.5, 0.5, 0.5, 1 };
                break;
        }
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::SmallButton(icon);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", name.c_str());
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
                ImGui::TextDisabled("(unsupported type)");
        }

        ImGui::Unindent(30);
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_TRASH))
            toRemove.push_back(name);
        if (changed)
            needsPreviewUpdate = true;
        ImGui::PopID();
        ImGui::Separator();
    });

    for (auto& name : toRemove)
        mat->removeProperty(name);

    if (!hasProps)
        ImGui::TextDisabled("No properties yet. Use 'Add Property' below.");

    if (ImGui::Button(ICON_FA_PLUS " Add Property", ImVec2(-1, 30))) {
        showAddPropertyPopup  = true;
        propertyNameBuffer[0] = '\0';
    }

    ImGui::EndChild();
    ImGui::Columns(1);

    renderAddPropertyPopup();
    ImGui::End();
}

// ==== PROPERTY RENDERERS ====

bool MaterialViewer::renderColorProperty(const std::string&            name,
                                         const Core::MaterialProperty& prop) const {
    const glm::vec4 color  = prop.value.getVector4();
    float           col[4] = { color.r, color.g, color.b, color.a };

    if (ImGui::ColorEdit4(fmt::format("##{}_color", name).c_str(),
                          col,
                          ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar)) {
        materialHandle.get()->setProperty(
            name, Core::MaterialPropertyValue(glm::vec4(col[0], col[1], col[2], col[3])));
        return true;
    }
    return false;
}

bool MaterialViewer::renderFloatProperty(const std::string&            name,
                                         const Core::MaterialProperty& prop) const {
    float val = prop.value.getFloat();
    if (ImGui::SliderFloat(fmt::format("##{}_float", name).c_str(), &val, 0.0f, 1.0f, "%.3f")) {
        materialHandle.get()->setProperty(name, Core::MaterialPropertyValue(val));
        return true;
    }
    return false;
}

bool MaterialViewer::renderVectorProperty(const std::string&            name,
                                          const Core::MaterialProperty& prop) const {
    const glm::vec3 v      = prop.value.getVector3();
    float           arr[3] = { v.x, v.y, v.z };
    if (ImGui::DragFloat3(fmt::format("##{}_vec3", name).c_str(), arr, 0.01f)) {
        materialHandle.get()->setProperty(
            name, Core::MaterialPropertyValue(glm::vec3(arr[0], arr[1], arr[2])));
        return true;
    }
    return false;
}

bool MaterialViewer::renderTextureProperty(const std::string&            name,
                                           const Core::MaterialProperty& prop) const {
    const Core::UUID texID = prop.value.getTexture();
    int              slot  = prop.value.getTextureSlot();
    std::string      label = "None";
    if (!texID.is_nil()) {
        auto meta = assetManager->getMetadata(texID);
        label     = meta.path.substr(meta.path.find_last_of('/') + 1);
    }

    bool changed = false;
    ImGui::SetNextItemWidth(-100);
    if (ImGui::BeginCombo(fmt::format("##{}_texture", name).c_str(), label.c_str())) {
        if (ImGui::Selectable("None", texID.is_nil())) {
            materialHandle.get()->setProperty(name,
                                              Core::MaterialPropertyValue(Core::UUID(), slot));
            changed = true;
        }
        for (auto  textures = assetManager->getAllOfType<Graphics::Texture2D>();
             auto& t : textures) {
            auto        meta  = assetManager->getMetadata(t.getID());
            std::string tName = meta.path.substr(meta.path.find_last_of('/') + 1);
            bool        sel   = (t.getID() == texID);
            if (ImGui::Selectable(tName.c_str(), sel)) {
                materialHandle.get()->setProperty(name,
                                                  Core::MaterialPropertyValue(t.getID(), slot));
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Text("Slot:");
    ImGui::SameLine();
    ImGui::PushButtonRepeat(true);
    if (ImGui::SmallButton(fmt::format("-##{}", name).c_str()) && slot > 0) {
        slot--;
        materialHandle.get()->setProperty(name, Core::MaterialPropertyValue(texID, slot));
        changed = true;
    }
    ImGui::SameLine();
    ImGui::Text("%d", slot);
    ImGui::SameLine();
    if (ImGui::SmallButton(fmt::format("+##{}", name).c_str()) && slot < 10) {
        slot++;
        materialHandle.get()->setProperty(name, Core::MaterialPropertyValue(texID, slot));
        changed = true;
    }
    ImGui::PopButtonRepeat();
    return changed;
}

void MaterialViewer::renderAddPropertyPopup() {
    if (showAddPropertyPopup) {
        ImGui::OpenPopup("Add Property");
        showAddPropertyPopup = false;
    }
    if (ImGui::BeginPopupModal("Add Property", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto mat = materialHandle.get();
        ImGui::Text("Property Name:");
        ImGui::InputText("##PropName", propertyNameBuffer.data(), propertyNameBuffer.size());

        ImGui::SeparatorText("Type");
        constexpr ImVec2  btn(135, 40);
        const std::string name(propertyNameBuffer.data());

        if (ImGui::Button(ICON_FA_PALETTE " Color", btn) && !name.empty()) {
            mat->setValue(name, glm::vec4(1.0f));
            needsPreviewUpdate = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_HASHTAG " Float", btn) && !name.empty()) {
            mat->setValue(name, 0.5f);
            needsPreviewUpdate = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button(ICON_FA_VECTOR_SQUARE " Vector3", btn) && !name.empty()) {
            mat->setValue(name, glm::vec3(0.0f));
            needsPreviewUpdate = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_IMAGE " Texture", btn) && !name.empty()) {
            mat->setProperty(name, Core::MaterialPropertyValue(Core::UUID(), 0));
            needsPreviewUpdate = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel", ImVec2(-1, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

}
