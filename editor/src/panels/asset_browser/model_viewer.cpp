#include "editor/panels/asset_browser/model_viewer.hpp"
#include "IconsFontAwesome6.h"
#include "corvus/asset/asset_handle.hpp"
#include "corvus/log.hpp"
#include "corvus/renderer/material.hpp"
#include "corvus/renderer/mesh.hpp"
#include "editor/imguiutils.hpp"
#include "imgui_internal.h"
#include <cfloat>
#include <format>
#include <glm/gtc/matrix_transform.hpp>

namespace Corvus::Editor {

ModelViewer::ModelViewer(const Core::UUID&          id,
                         Core::AssetManager*        manager,
                         Graphics::GraphicsContext& context)
    : AssetViewer(id, manager, "Model Viewer"), context_(&context), sceneRenderer(context) {

    modelHandle = assetManager->loadByID<Renderer::Model>(id);

    initPreview();
}

ModelViewer::~ModelViewer() { cleanupPreview(); }

void ModelViewer::initPreview() {
    if (previewInitialized)
        return;

    colorTexture = context_->createTexture2D(previewResolution, previewResolution);
    depthTexture = context_->createDepthTexture(previewResolution, previewResolution);

    framebuffer = context_->createFramebuffer(previewResolution, previewResolution);
    framebuffer.attachTexture2D(colorTexture, 0);
    framebuffer.attachDepthTexture(depthTexture);

    previewCamera.setPosition(glm::vec3(0.0f, 1.5f, 3.0f));
    previewCamera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    previewCamera.setPerspective(45.0f, 1.0f, 0.1f, 100.0f);

    setupPreviewLights();

    previewInitialized = true;
    CORVUS_CORE_INFO("Model viewer preview initialized");
}

void ModelViewer::cleanupPreview() {
    if (!previewInitialized)
        return;

    sceneRenderer.getLighting().shutdown();
    context_->flush();

    colorTexture.release();
    depthTexture.release();
    framebuffer.release();

    previewInitialized = false;
}

void ModelViewer::setupPreviewLights() {
    sceneRenderer.clearLights();

    Renderer::Light keyLight;
    keyLight.type      = Renderer::LightType::Directional;
    keyLight.direction = glm::normalize(glm::vec3(-0.3f, -0.7f, -0.5f));
    keyLight.color     = glm::vec3(1.0f);
    keyLight.intensity = 0.9f;
    sceneRenderer.addLight(keyLight);

    Renderer::Light fillLight;
    fillLight.type      = Renderer::LightType::Directional;
    fillLight.direction = glm::normalize(glm::vec3(0.5f, -0.3f, 0.5f));
    fillLight.color     = glm::vec3(0.7f, 0.78f, 0.86f);
    fillLight.intensity = 0.4f;
    sceneRenderer.addLight(fillLight);

    Renderer::Light rimLight;
    rimLight.type      = Renderer::LightType::Directional;
    rimLight.direction = glm::normalize(glm::vec3(0.0f, 0.3f, 1.0f));
    rimLight.color     = glm::vec3(1.0f);
    rimLight.intensity = 0.3f;
    sceneRenderer.addLight(rimLight);

    sceneRenderer.setAmbientColor(glm::vec3(0.1f));
}

void ModelViewer::calculateModelBounds() {
    auto model = modelHandle.get();
    if (!model)
        return;

    glm::vec3 min(FLT_MAX), max(-FLT_MAX);
    for (const auto& mesh : model->getMeshes()) {
        if (!mesh)
            continue;
        min = glm::min(min, mesh->getBoundingBoxMin());
        max = glm::max(max, mesh->getBoundingBoxMax());
    }
    boundsMin = min;
    boundsMax = max;

    glm::vec3 size   = max - min;
    float     maxDim = std::max({ size.x, size.y, size.z });

    // Automatically scale and distance the camera based on model size
    cameraDistance = std::clamp(maxDim * 2.0f, 2.0f, 50.0f);
    modelCenter    = (min + max) * 0.5f;
}

void ModelViewer::updateCameraPosition() {
    glm::vec3 pos = glm::vec3(cameraDistance * cosf(cameraAngleX) * sinf(cameraAngleY),
                              cameraDistance * sinf(cameraAngleX),
                              cameraDistance * cosf(cameraAngleX) * cosf(cameraAngleY));
    previewCamera.setPosition(pos);
    previewCamera.lookAt(glm::vec3(0.0f));
}

void ModelViewer::updatePreview() {
    if (!modelHandle.isValid() || !previewInitialized)
        return;

    if (needsPreviewUpdate) {
        calculateModelBounds();
        needsPreviewUpdate = false;
    }

    if (autoRotate)
        cameraAngleY += autoRotateSpeed * 0.01f;

    updateCameraPosition();
}

void ModelViewer::handleCameraControls() {
    ImVec2 mousePos = ImGui::GetMousePos();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isDragging   = true;
        lastMousePos = mousePos;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        isDragging = false;

    if (isDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = { mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y };

        cameraAngleY += delta.x * 0.01f;
        cameraAngleX += delta.y * 0.01f;
        cameraAngleX
            = glm::clamp(cameraAngleX, -glm::half_pi<float>() + 0.1f, glm::half_pi<float>() - 0.1f);

        updateCameraPosition();
        lastMousePos = mousePos;
    }

    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
        cameraDistance -= wheel * 0.3f;
        cameraDistance = glm::clamp(cameraDistance, 1.0f, 50.0f);
        updateCameraPosition();
    }
}

void ModelViewer::renderPreview() {
    if (!previewInitialized)
        return;

    updatePreview();

    auto model = modelHandle.get();
    if (!model || !model->valid())
        return;

    sceneRenderer.clear(glm::vec4(0.176f, 0.176f, 0.188f, 1.0f), true, &framebuffer);

    auto& defaultShader = sceneRenderer.getMaterialRenderer().getDefaultShader();
    if (!defaultShader.valid()) {
        CORVUS_CORE_ERROR("Default shader not available for model preview");
        return;
    }

    Renderer::Material whiteMaterial(defaultShader);
    whiteMaterial.setVec4("_MainColor", glm::vec4(1.0f));

    Renderer::RenderState state;
    state.depthTest  = true;
    state.depthWrite = true;
    state.blend      = false;
    state.cullFace   = true;
    whiteMaterial.setRenderState(state);

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), -modelCenter);

    std::vector<Renderer::Renderable> renderables;
    Renderer::Renderable              renderable;
    renderable.model          = model.get();
    renderable.material       = &whiteMaterial;
    renderable.transform      = transform;
    renderable.position       = -modelCenter;
    renderable.boundingRadius = glm::length(boundsMax - boundsMin) * 0.5f;
    renderable.wireframe      = showWireframe;
    renderable.enabled        = true;
    renderables.push_back(renderable);

    sceneRenderer.render(renderables, previewCamera, &framebuffer);
}

void ModelViewer::renderModelInfo(const Renderer::Model& model) {
    ImGui::SeparatorText(ICON_FA_CIRCLE_INFO " Model Info");
    ImGui::Spacing();

    int totalMeshes = static_cast<int>(model.getMeshes().size());
    int totalVerts = 0, totalTris = 0;

    for (auto& mesh : model.getMeshes()) {
        if (!mesh)
            continue;
        totalVerts += mesh->getVertexCount();
        totalTris += mesh->getIndexCount() / 3;
    }

    ImGui::Text("Meshes: %d", totalMeshes);
    ImGui::Text("Vertices: %d", totalVerts);
    ImGui::Text("Triangles: %d", totalTris);

    glm::vec3 size = boundsMax - boundsMin;
    ImGui::Spacing();
    ImGui::SeparatorText(ICON_FA_CUBE " Bounding Box");
    ImGui::Text("Size: %.2f x %.2f x %.2f", size.x, size.y, size.z);
    ImGui::Text("Min:  (%.2f, %.2f, %.2f)", boundsMin.x, boundsMin.y, boundsMin.z);
    ImGui::Text("Max:  (%.2f, %.2f, %.2f)", boundsMax.x, boundsMax.y, boundsMax.z);

    if (!model.getMeshes().empty()) {
        ImGui::Spacing();
        ImGui::SeparatorText(ICON_FA_LAYER_GROUP " Meshes");
        ImGui::Spacing();

        int i = 0;
        for (auto& mesh : model.getMeshes()) {
            ImGui::PushID(i);
            if (ImGui::TreeNode("Mesh", "Mesh %d", i)) {
                ImGui::Text("Vertices: %d", mesh->getVertexCount());
                ImGui::Text("Indices:  %d", mesh->getIndexCount());
                glm::vec3 min = mesh->getBoundingBoxMin();
                glm::vec3 max = mesh->getBoundingBoxMax();
                ImGui::Text("Bounds: (%.2f %.2f %.2f) → (%.2f %.2f %.2f)",
                            min.x,
                            min.y,
                            min.z,
                            max.x,
                            max.y,
                            max.z);

                ImGui::Text("Has Normals: %s", mesh->hasNormals() ? "Yes" : "No");
                ImGui::Text("Has UVs:     %s", mesh->hasTexcoords() ? "Yes" : "No");
                ImGui::Text("Has Colors:  %s", mesh->hasColors() ? "Yes" : "No");

                ImGui::TreePop();
            }
            ImGui::PopID();
            i++;
        }
    }
}

void ModelViewer::renderDisplayOptions() {
    ImGui::SeparatorText(ICON_FA_SLIDERS " Display Options");
    ImGui::Spacing();

    ImGui::Checkbox("Show Wireframe", &showWireframe);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Display model as wireframe (line mode)");

    ImGui::Checkbox("Show Bounding Box", &showBoundingBox);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Display the model's bounding box");

    ImGui::Checkbox("Auto Rotate", &autoRotate);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Automatically rotate the model");

    if (autoRotate)
        ImGui::SliderFloat("Speed", &autoRotateSpeed, 0.1f, 2.0f);

    ImGui::SliderFloat("Camera Distance", &cameraDistance, 1.0f, 50.0f);
}

void ModelViewer::render() {
    if (!modelHandle.isValid()) {
        isOpen = false;
        return;
    }

    auto model = modelHandle.get();
    if (!model) {
        isOpen = false;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

    std::string title;
    if (modelHandle.isValid()) {
        auto meta = assetManager->getMetadata(modelHandle.getID());
        title     = std::format(
            "{} Model: {}", ICON_FA_CUBE, meta.path.substr(meta.path.find_last_of('/') + 1));
    }

    if (!ImGui::Begin(title.c_str(), &isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button(ICON_FA_ARROWS_TO_DOT " Reset View")) {
            cameraAngleX = cameraAngleY = 0.0f;
            autoRotate                  = false;
            cameraDistance              = 3.0f;
            updateCameraPosition();
        }
        ImGui::EndMenuBar();
    }

    renderPreview();

    ImGui::Columns(2, "##ModelColumns", true);
    ImGui::SetColumnWidth(0, 550);

    {
        ImGui::BeginChild("##PreviewSection", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::SeparatorText(ICON_FA_EYE " Preview");

        ImVec2 availRegion = ImGui::GetContentRegionAvail();
        float  previewSize = std::min(availRegion.x - 20, 480.0f);
        float  offsetX     = (availRegion.x - previewSize) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        ImVec2 cursorBefore = ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));
        ImGui::BeginChild(
            "##PreviewFrame", ImVec2(previewSize, previewSize), true, ImGuiWindowFlags_NoScrollbar);

        ImGui::RenderFramebuffer(
            framebuffer, colorTexture, ImVec2(previewSize - 2, previewSize - 2), true);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImRect previewRect(cursorBefore,
                           ImVec2(cursorBefore.x + previewSize, cursorBefore.y + previewSize));
        if (previewRect.Contains(ImGui::GetMousePos()))
            handleCameraControls();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImGui::TextDisabled(ICON_FA_COMPUTER_MOUSE " Drag to rotate • Scroll to zoom");
        ImGui::EndChild();
    }

    ImGui::NextColumn();

    {
        ImGui::BeginChild("##OptionsSection", ImVec2(0, 0), true);
        renderDisplayOptions();
        ImGui::Separator();
        renderModelInfo(*model);
        ImGui::EndChild();
    }

    ImGui::Columns(1);
    ImGui::End();
}

}
