#pragma once

#include "IconsFontAwesome6.h"
#include "editor/imguiutils.hpp"
#include "editor/panels/inspector/inspector_panel.hpp"
#include "imgui_internal.h"
#include "linp/components/entity_info.hpp"
#include <cstring>
#include <imgui.h>
#include <string.h>

namespace Linp::Editor {

/**
 * @brief Specialization of ComponentInfo for Linp::Core::Components::EntityInfoComponent.
 */
template <>
struct ComponentInfo<Linp::Core::Components::EntityInfoComponent> {
    using ComponentType = Linp::Core::Components::EntityInfoComponent;
    static constexpr std::string_view name
        = ICON_FA_CIRCLE_INFO " Entity Info"; // Using a more fitting icon
    static constexpr bool removable = false;
    static constexpr bool flat      = true;

    static void draw(ComponentType& entityInfo) {
        char buffer[256] = {};
        if (!entityInfo.tag.empty()) {
            strncpy(buffer, entityInfo.tag.c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
        }

        ImGui::Checkbox("##Enabled",
                        &entityInfo.enabled); // Use "##" to hide label but keep ID unique
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText(
                "##Tag", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            entityInfo.tag = std::string(buffer);
        }
        // Tooltip for clarity
        if (ImGui::IsItemHovered() && !entityInfo.tag.empty()) {
            ImGui::SetTooltip("Tag: %s", entityInfo.tag.c_str());
        } else if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Entity Tag");
        }
    }
};

template <>
struct ComponentInfo<Linp::Core::Components::TransformComponent> {
    using ComponentType                         = Linp::Core::Components::TransformComponent;
    static constexpr std::string_view name      = ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE " Transform";
    static constexpr bool             removable = false;
    static constexpr bool             flat      = false;

    static void draw(Linp::Core::Components::TransformComponent& transform) {
        // Position
        raylib::Vector3 position = transform.position;
        if (ImGui::Vector3Editor("Position", position)) {
            transform.position = position;
        }

        // Scale
        raylib::Vector3 scale = transform.scale;
        if (ImGui::Vector3Editor("Scale", scale)) {
            transform.scale = scale;
        }

        // Rotation - convert quaternion to Euler for editing
        // ALWAYS update the Euler angles to reflect current quaternion
        raylib::Vector3 eulerAngles = QuaternionToEuler(transform.rotation);
        eulerAngles.x *= RAD2DEG;
        eulerAngles.y *= RAD2DEG;
        eulerAngles.z *= RAD2DEG;

        if (ImGui::Vector3Editor("Rotation", eulerAngles)) {
            transform.rotation = QuaternionFromEuler(
                eulerAngles.x * DEG2RAD, eulerAngles.y * DEG2RAD, eulerAngles.z * DEG2RAD);
        }
    }
};

template <>
struct ComponentInfo<Linp::Core::Components::MeshRendererComponent> {
    using ComponentType                         = Linp::Core::Components::MeshRendererComponent;
    static constexpr std::string_view name      = ICON_FA_CUBE " Mesh Renderer";
    static constexpr bool             removable = true;
    static constexpr bool             flat      = false;

    static void draw(Linp::Core::Components::MeshRendererComponent& renderer) {
        // Primitive type selection
        const char* primitiveNames[] = { "Cube", "Sphere", "Plane", "Cylinder", "Custom" };
        int         currentPrimitive = static_cast<int>(renderer.primitiveType);

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, 100);
        ImGui::Text("Primitive");
        ImGui::NextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::Combo("##PrimitiveType",
                         &currentPrimitive,
                         primitiveNames,
                         IM_ARRAYSIZE(primitiveNames))) {
            renderer.primitiveType
                = static_cast<Linp::Core::Components::PrimitiveType>(currentPrimitive);
            if (renderer.primitiveType != Linp::Core::Components::PrimitiveType::Custom) {
                renderer.generateMesh();
            }
        }
        ImGui::PopItemWidth();
        ImGui::EndColumns();

        // Parameters based on primitive type
        bool needsRegeneration = false;

        switch (renderer.primitiveType) {
            case Linp::Core::Components::PrimitiveType::Cube:
                needsRegeneration
                    = ImGui::FloatEditor("Size", renderer.params.cube.size, 0.1f, 0.1f, 100.0f);
                break;

            case Linp::Core::Components::PrimitiveType::Sphere:
                needsRegeneration |= ImGui::FloatEditor(
                    "Radius", renderer.params.sphere.radius, 0.1f, 0.1f, 100.0f);
                needsRegeneration
                    |= ImGui::IntEditor("Rings", renderer.params.sphere.rings, 1, 3, 50);
                needsRegeneration
                    |= ImGui::IntEditor("Slices", renderer.params.sphere.slices, 1, 3, 50);
                break;

            case Linp::Core::Components::PrimitiveType::Plane:
                needsRegeneration
                    |= ImGui::FloatEditor("Width", renderer.params.plane.width, 0.1f, 0.1f, 100.0f);
                needsRegeneration |= ImGui::FloatEditor(
                    "Length", renderer.params.plane.length, 0.1f, 0.1f, 100.0f);
                break;

            case Linp::Core::Components::PrimitiveType::Cylinder:
                needsRegeneration |= ImGui::FloatEditor(
                    "Radius", renderer.params.cylinder.radius, 0.1f, 0.0f, 100.0f);
                needsRegeneration |= ImGui::FloatEditor(
                    "Height", renderer.params.cylinder.height, 0.1f, 0.1f, 100.0f);
                needsRegeneration
                    |= ImGui::IntEditor("Slices", renderer.params.cylinder.slices, 1, 3, 50);
                break;

            case Linp::Core::Components::PrimitiveType::Custom:
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 100);
                ImGui::Text("Mesh File");
                ImGui::NextColumn();
                ImGui::PushItemWidth(-80);
                ImGui::TextDisabled("No file selected");
                ImGui::SameLine();
                if (ImGui::Button("Browse...")) {
                    // TODO: File dialog to load custom mesh
                    ImGui::OpenPopup("Load Mesh");
                }
                ImGui::PopItemWidth();
                ImGui::EndColumns();
                break;
        }

        if (needsRegeneration) {
            renderer.generateMesh();
        }

        // Material properties
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (renderer.material) {
            // Basic material color
            float color[4] = { renderer.material->maps[MATERIAL_MAP_DIFFUSE].color.r / 255.0f,
                               renderer.material->maps[MATERIAL_MAP_DIFFUSE].color.g / 255.0f,
                               renderer.material->maps[MATERIAL_MAP_DIFFUSE].color.b / 255.0f,
                               renderer.material->maps[MATERIAL_MAP_DIFFUSE].color.a / 255.0f };

            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100);
            ImGui::Text("Diffuse");
            ImGui::NextColumn();
            ImGui::PushItemWidth(-80);
            if (ImGui::ColorEdit4("##DiffuseColor", color, ImGuiColorEditFlags_NoLabel)) {
                renderer.material->maps[MATERIAL_MAP_DIFFUSE].color
                    = { (unsigned char)(color[0] * 255),
                        (unsigned char)(color[1] * 255),
                        (unsigned char)(color[2] * 255),
                        (unsigned char)(color[3] * 255) };
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset##Material")) {
                renderer.material = std::make_shared<raylib::Material>(LoadMaterialDefault());
            }
            ImGui::PopItemWidth();
            ImGui::EndColumns();
        }

        // Mesh info (compact)
        if (renderer.mesh) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100);
            ImGui::Text("Mesh Info");
            ImGui::NextColumn();
            ImGui::Text("%d vertices, %d triangles",
                        renderer.mesh->vertexCount,
                        renderer.mesh->triangleCount);
            ImGui::EndColumns();
        }
    }
};

} // namespace Linp::Editor
