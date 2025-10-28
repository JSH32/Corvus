#include "editor/panels/asset_browser/model_viewer.hpp"
#include "IconsFontAwesome6.h"
#include "corvus/asset/material/material.hpp"
#include "corvus/log.hpp"
#include "imgui_internal.h"
#include "rlImGui.h"
#include <format>

namespace Corvus::Editor {

ModelViewer::ModelViewer(const Core::UUID& id, Core::AssetManager* manager)
    : AssetViewer(id, manager, "Model Viewer") {
    modelHandle = assetManager->loadByID<raylib::Model>(id);
    if (modelHandle.isValid()) {
        auto meta   = assetManager->getMetadata(id);
        windowTitle = std::format(
            "{} Model: {}", ICON_FA_CUBE, meta.path.substr(meta.path.find_last_of('/') + 1));
    }

    initPreview();
}

ModelViewer::~ModelViewer() { cleanupPreview(); }

void ModelViewer::initPreview() {
    if (previewInitialized)
        return;

    previewTexture = LoadRenderTexture(512, 512);

    previewCamera.position   = Vector3 { 0.0f, 2.0f, 5.0f };
    previewCamera.target     = Vector3 { 0.0f, 0.0f, 0.0f };
    previewCamera.up         = Vector3 { 0.0f, 1.0f, 0.0f };
    previewCamera.fovy       = 45.0f;
    previewCamera.projection = CAMERA_PERSPECTIVE;

    previewInitialized = true;
    CORVUS_CORE_INFO("Model viewer preview initialized");
}

void ModelViewer::cleanupPreview() {
    if (!previewInitialized)
        return;

    CORVUS_CORE_INFO("Cleaning up model viewer preview");

    if (previewTexture.id > 0) {
        UnloadRenderTexture(previewTexture);
        previewTexture = { 0 };
    }

    previewInitialized = false;
}

void ModelViewer::calculateModelBounds(raylib::Model* model) {
    if (!model || model->meshCount == 0)
        return;

    // Calculate bounding box from all meshes
    bool first = true;
    for (int i = 0; i < model->meshCount; i++) {
        BoundingBox meshBox = GetMeshBoundingBox(model->meshes[i]);

        if (first) {
            modelBounds = meshBox;
            first       = false;
        } else {
            // Expand bounding box
            modelBounds.min.x = std::min(modelBounds.min.x, meshBox.min.x);
            modelBounds.min.y = std::min(modelBounds.min.y, meshBox.min.y);
            modelBounds.min.z = std::min(modelBounds.min.z, meshBox.min.z);
            modelBounds.max.x = std::max(modelBounds.max.x, meshBox.max.x);
            modelBounds.max.y = std::max(modelBounds.max.y, meshBox.max.y);
            modelBounds.max.z = std::max(modelBounds.max.z, meshBox.max.z);
        }
    }

    Vector3 size = { modelBounds.max.x - modelBounds.min.x,
                     modelBounds.max.y - modelBounds.min.y,
                     modelBounds.max.z - modelBounds.min.z };

    float maxDim = std::max({ size.x, size.y, size.z });

    // Adjust camera distance based on model size
    cameraDistance = maxDim * 2.0f;
    if (cameraDistance < 2.0f)
        cameraDistance = 2.0f;
    if (cameraDistance > 50.0f)
        cameraDistance = 50.0f;

    // Camera target is at origin since we're centering the model there
    previewCamera.target = Vector3 { 0.0f, 0.0f, 0.0f };
}

void ModelViewer::updatePreview() {
    auto model = modelHandle.get();
    if (!model || !previewInitialized)
        return;

    if (needsPreviewUpdate) {
        calculateModelBounds(model.get());
        needsPreviewUpdate = false;
    }

    // Auto-rotate
    if (autoRotate) {
        cameraAngleY += autoRotateSpeed * 0.01f;
    }

    // Update camera position based on angles and distance
    previewCamera.position.x
        = previewCamera.target.x + cameraDistance * cosf(cameraAngleX) * sinf(cameraAngleY);
    previewCamera.position.y = previewCamera.target.y + cameraDistance * sinf(cameraAngleX);
    previewCamera.position.z
        = previewCamera.target.z + cameraDistance * cosf(cameraAngleX) * cosf(cameraAngleY);
}

void ModelViewer::handleCameraControls() {
    ImGuiIO& io       = ImGui::GetIO();
    ImVec2   mousePos = ImGui::GetMousePos();

    // Left mouse drag to rotate
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

        lastMousePos = { mousePos.x, mousePos.y };
    }

    // Mouse wheel zoom
    if (io.MouseWheel != 0.0f) {
        cameraDistance -= io.MouseWheel * 0.5f;
        cameraDistance = std::clamp(cameraDistance, 1.0f, 100.0f);
    }
}

void ModelViewer::renderPreview() {
    if (!previewInitialized)
        return;

    auto model = modelHandle.get();
    if (!model)
        return;

    updatePreview();

    BeginTextureMode(previewTexture);
    ClearBackground(Color { 45, 45, 48, 255 });

    previewLighting.clear();

    // Key light from top-front-right (reduced intensity)
    Core::Systems::LightData keyLight;
    keyLight.type      = Core::Systems::LightType::Directional;
    keyLight.direction = Vector3Normalize({ -0.3f, -0.7f, -0.5f });
    keyLight.color     = WHITE;
    keyLight.intensity = 0.6f;
    previewLighting.addLight(keyLight);

    // Fill light from left (reduced intensity)
    Core::Systems::LightData fillLight;
    fillLight.type      = Core::Systems::LightType::Directional;
    fillLight.direction = Vector3Normalize({ 0.5f, -0.3f, 0.5f });
    fillLight.color     = Color { 180, 200, 220, 255 };
    fillLight.intensity = 0.2f;
    previewLighting.addLight(fillLight);

    // Rim light from back (reduced intensity)
    Core::Systems::LightData rimLight;
    rimLight.type      = Core::Systems::LightType::Directional;
    rimLight.direction = Vector3Normalize({ 0.0f, 0.3f, 1.0f });
    rimLight.color     = WHITE;
    rimLight.intensity = 0.15f;
    previewLighting.addLight(rimLight);

    BeginMode3D(previewCamera);

    // Calculate offset to center the model at origin
    Vector3 modelCenter = { (modelBounds.min.x + modelBounds.max.x) * 0.5f,
                            (modelBounds.min.y + modelBounds.max.y) * 0.5f,
                            (modelBounds.min.z + modelBounds.max.z) * 0.5f };

    Vector3 drawOffset = Vector3Negate(modelCenter);
    Matrix  transform  = MatrixTranslate(drawOffset.x, drawOffset.y, drawOffset.z);

    // Draw grid at origin (since model is centered at origin now)
    if (showGrid) {
        // Grid should be at the bottom of the centered model
        float gridY = modelBounds.min.y + drawOffset.y; // Apply the same offset

        for (int i = -10; i <= 10; i++) {
            Color lineColor = (i == 0) ? Color { 100, 100, 100, 100 } : Color { 60, 60, 60, 60 };
            DrawLine3D({ (float)i, gridY, -10.0f }, { (float)i, gridY, 10.0f }, lineColor);
            DrawLine3D({ -10.0f, gridY, (float)i }, { 10.0f, gridY, (float)i }, lineColor);
        }
    }

    // Create a fallback material
    static std::unique_ptr<Corvus::Core::Material> fallbackMat;
    if (!fallbackMat) {
        fallbackMat = std::make_unique<Corvus::Core::Material>();
        fallbackMat->setColor("_MainColor", WHITE);
        fallbackMat->setFloat("_Metallic", 0.0f);
        fallbackMat->setFloat("_Smoothness", 0.5f);
        fallbackMat->loadDefaultShader();
    }

    // Draw the model with materials and lighting
    if (!showWireframe) {
        raylib::Material* mat = fallbackMat->getRaylibMaterial(nullptr);

        // Apply lighting to the material - use origin since model is centered there
        previewLighting.applyToMaterial(mat, Vector3 { 0, 0, 0 }, 10.0f, previewCamera.position);

        // Draw each mesh with the lit material
        for (int i = 0; i < model->meshCount; i++) {
            DrawMesh(model->meshes[i], *mat, transform);
        }
    }

    // Draw wireframe
    if (showWireframe) {
        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(transform));

        for (int i = 0; i < model->meshCount; i++) {
            rlBegin(RL_LINES);
            rlColor4ub(WHITE.r, WHITE.g, WHITE.b, WHITE.a);

            Mesh* mesh = &model->meshes[i];

            // Draw triangles as lines
            if (mesh->indices != nullptr) {
                for (int j = 0; j < mesh->triangleCount * 3; j += 3) {
                    unsigned short i0 = mesh->indices[j];
                    unsigned short i1 = mesh->indices[j + 1];
                    unsigned short i2 = mesh->indices[j + 2];

                    Vector3 v0 = { mesh->vertices[i0 * 3],
                                   mesh->vertices[i0 * 3 + 1],
                                   mesh->vertices[i0 * 3 + 2] };
                    Vector3 v1 = { mesh->vertices[i1 * 3],
                                   mesh->vertices[i1 * 3 + 1],
                                   mesh->vertices[i1 * 3 + 2] };
                    Vector3 v2 = { mesh->vertices[i2 * 3],
                                   mesh->vertices[i2 * 3 + 1],
                                   mesh->vertices[i2 * 3 + 2] };

                    rlVertex3f(v0.x, v0.y, v0.z);
                    rlVertex3f(v1.x, v1.y, v1.z);

                    rlVertex3f(v1.x, v1.y, v1.z);
                    rlVertex3f(v2.x, v2.y, v2.z);

                    rlVertex3f(v2.x, v2.y, v2.z);
                    rlVertex3f(v0.x, v0.y, v0.z);
                }
            }

            rlEnd();
        }

        rlPopMatrix();
    }

    // Draw bounding box
    if (showBoundingBox) {
        BoundingBox offsetBounds = modelBounds;
        offsetBounds.min         = Vector3Add(offsetBounds.min, drawOffset);
        offsetBounds.max         = Vector3Add(offsetBounds.max, drawOffset);
        DrawBoundingBox(offsetBounds, GREEN);
    }

    EndMode3D();

    EndTextureMode();
}

void ModelViewer::renderModelInfo(raylib::Model* model) {
    if (!model)
        return;

    ImGui::SeparatorText(ICON_FA_CIRCLE_INFO " Model Info");
    ImGui::Spacing();

    ImGui::Text("Meshes:");
    ImGui::SameLine(120);
    ImGui::Text("%d", model->meshCount);

    ImGui::Text("Materials:");
    ImGui::SameLine(120);
    ImGui::Text("%d", model->materialCount);

    ImGui::Text("Bones:");
    ImGui::SameLine(120);
    ImGui::Text("%d", model->boneCount);

    // Calculate total vertices and triangles
    int totalVertices  = 0;
    int totalTriangles = 0;
    for (int i = 0; i < model->meshCount; i++) {
        totalVertices += model->meshes[i].vertexCount;
        totalTriangles += model->meshes[i].triangleCount;
    }

    ImGui::Text("Vertices:");
    ImGui::SameLine(120);
    ImGui::Text("%d", totalVertices);

    ImGui::Text("Triangles:");
    ImGui::SameLine(120);
    ImGui::Text("%d", totalTriangles);

    // Bounding box info
    Vector3 size = { modelBounds.max.x - modelBounds.min.x,
                     modelBounds.max.y - modelBounds.min.y,
                     modelBounds.max.z - modelBounds.min.z };

    ImGui::Spacing();
    ImGui::SeparatorText(ICON_FA_CUBE " Bounding Box");
    ImGui::Spacing();

    ImGui::Text("Size:");
    ImGui::SameLine(120);
    ImGui::Text("%.2f x %.2f x %.2f", size.x, size.y, size.z);

    ImGui::Text("Min:");
    ImGui::SameLine(120);
    ImGui::Text("(%.2f, %.2f, %.2f)", modelBounds.min.x, modelBounds.min.y, modelBounds.min.z);

    ImGui::Text("Max:");
    ImGui::SameLine(120);
    ImGui::Text("(%.2f, %.2f, %.2f)", modelBounds.max.x, modelBounds.max.y, modelBounds.max.z);

    // Mesh details
    if (model->meshCount > 0) {
        ImGui::Spacing();
        ImGui::SeparatorText(ICON_FA_LAYER_GROUP " Meshes");
        ImGui::Spacing();

        for (int i = 0; i < model->meshCount; i++) {
            ImGui::PushID(i);

            if (ImGui::TreeNode("Mesh", "Mesh %d", i)) {
                Mesh& mesh = model->meshes[i];

                ImGui::Text("Vertices:");
                ImGui::SameLine(120);
                ImGui::Text("%d", mesh.vertexCount);

                ImGui::Text("Triangles:");
                ImGui::SameLine(120);
                ImGui::Text("%d", mesh.triangleCount);

                ImGui::Text("Has Normals:");
                ImGui::SameLine(120);
                ImGui::Text("%s", mesh.normals ? "Yes" : "No");

                ImGui::Text("Has UVs:");
                ImGui::SameLine(120);
                ImGui::Text("%s", mesh.texcoords ? "Yes" : "No");

                ImGui::Text("Has Colors:");
                ImGui::SameLine(120);
                ImGui::Text("%s", mesh.colors ? "Yes" : "No");

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }
}

void ModelViewer::renderDisplayOptions() {
    ImGui::SeparatorText(ICON_FA_SLIDERS " Display Options");
    ImGui::Spacing();

    ImGui::Checkbox("Show Wireframe", &showWireframe);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Display model as wireframe");
    }

    ImGui::Checkbox("Show Bounding Box", &showBoundingBox);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Display model bounding box");
    }

    ImGui::Checkbox("Show Grid", &showGrid);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Display ground grid");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Auto Rotate", &autoRotate);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically rotate the model");
    }

    if (autoRotate) {
        ImGui::Text("Speed:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##RotateSpeed", &autoRotateSpeed, 0.1f, 2.0f, "%.1f");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Camera Distance:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##CameraDistance", &cameraDistance, 1.0f, 50.0f, "%.1f")) { }
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

    if (!ImGui::Begin(windowTitle.c_str(), &isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Menu Bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button(ICON_FA_ARROWS_TO_DOT " Reset View")) {
            cameraAngleY       = 0.0f;
            cameraAngleX       = 0.0f;
            autoRotate         = false;
            needsPreviewUpdate = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset camera to default position");
        }

        ImGui::EndMenuBar();
    }

    renderPreview();

    // Two-column layout
    ImGui::Columns(2, "##ModelColumns", true);
    ImGui::SetColumnWidth(0, 550);

    // Preview
    {
        ImGui::BeginChild("##PreviewSection", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::SeparatorText(ICON_FA_EYE " Preview");
        ImGui::PopStyleVar();

        ImGui::Spacing();

        // Calculate centered preview size
        ImVec2 availRegion = ImGui::GetContentRegionAvail();
        float  previewSize = std::min(availRegion.x - 20, 480.0f);

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
        ImGui::TextDisabled(ICON_FA_COMPUTER_MOUSE " Drag to rotate, scroll to zoom");

        ImGui::EndChild();
    }

    ImGui::NextColumn();

    // Info & Options
    {
        ImGui::BeginChild("##OptionsSection", ImVec2(0, 0), true);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        renderDisplayOptions();
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Model information
        renderModelInfo(model.get());

        ImGui::EndChild();
    }

    ImGui::Columns(1);

    ImGui::End();
}

}
