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

    static void draw(ComponentType& entityInfo, Linp::Core::AssetManager*) {
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

    static void draw(Linp::Core::Components::TransformComponent& transform,
                     Linp::Core::AssetManager*) {
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

    static void draw(Linp::Core::Components::MeshRendererComponent& renderer,
                     Linp::Core::AssetManager*                      assetMgr) {
        using namespace Linp::Core;
        using namespace Linp::Core::Components;

        auto labeledField = [](const char* label, auto fn) {
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100);
            ImGui::TextUnformatted(label);
            ImGui::NextColumn();
            fn();
            ImGui::EndColumns();
        };

        auto buildAssetDropdown
            = [&](auto& handle, const char* label, auto assets, auto&& nameFromMeta) {
                  labeledField(label, [&] {
                      UUID currentID = handle.isValid() ? handle.getID() : UUID();
                      int  selected  = -1;

                      std::vector<std::string> names;
                      names.reserve(assets.size());
                      for (size_t i = 0; i < assets.size(); ++i) {
                          auto        meta = assetMgr->getMetadata(assets[i].getID());
                          std::string n    = nameFromMeta(meta);
                          if (assets[i].getID() == currentID)
                              selected = static_cast<int>(i);
                          names.push_back(std::move(n));
                      }

                      const char* currentLabel = (selected >= 0 && selected < (int)names.size())
                          ? names[selected].c_str()
                          : "None";

                      ImGui::PushItemWidth(-80);
                      if (ImGui::BeginCombo(fmt::format("##{}", label).c_str(), currentLabel)) {
                          // None entry
                          bool noneSel = (selected == -1);
                          if (ImGui::Selectable("None", noneSel))
                              handle = {};
                          if (noneSel)
                              ImGui::SetItemDefaultFocus();

                          // Asset entries
                          for (int i = 0; i < (int)names.size(); ++i) {
                              bool sel = (i == selected);
                              if (ImGui::Selectable(names[i].c_str(), sel))
                                  handle = assets[i];
                              if (sel)
                                  ImGui::SetItemDefaultFocus();
                          }
                          ImGui::EndCombo();
                      }
                      ImGui::PopItemWidth();
                  });
              };

        // Primitive Type
        labeledField("Primitive", [&] {
            static const char* kPrimitiveNames[]
                = { "Cube", "Sphere", "Plane", "Cylinder", "Model" };
            int current = static_cast<int>(renderer.primitiveType);
            if (ImGui::Combo(
                    "##PrimitiveType", &current, kPrimitiveNames, IM_ARRAYSIZE(kPrimitiveNames))) {
                renderer.primitiveType = static_cast<PrimitiveType>(current);
                renderer.generateModel();
            }
        });

        bool needsRegen = false;

        // Primitive parameters
        switch (renderer.primitiveType) {
            case PrimitiveType::Cube:
                needsRegen |= ImGui::FloatEditor("Size", renderer.params.cube.size, 0.1f);
                break;
            case PrimitiveType::Sphere:
                needsRegen |= ImGui::FloatEditor("Radius", renderer.params.sphere.radius, 0.1f);
                needsRegen |= ImGui::IntEditor("Rings", renderer.params.sphere.rings, 1, 3, 50);
                needsRegen |= ImGui::IntEditor("Slices", renderer.params.sphere.slices, 1, 3, 50);
                break;
            case PrimitiveType::Plane:
                needsRegen |= ImGui::FloatEditor("Width", renderer.params.plane.width, 0.1f);
                needsRegen |= ImGui::FloatEditor("Length", renderer.params.plane.length, 0.1f);
                break;
            case PrimitiveType::Cylinder:
                needsRegen |= ImGui::FloatEditor("Radius", renderer.params.cylinder.radius, 0.1f);
                needsRegen |= ImGui::FloatEditor("Height", renderer.params.cylinder.height, 0.1f);
                needsRegen |= ImGui::IntEditor("Slices", renderer.params.cylinder.slices, 1, 3, 50);
                break;
            case PrimitiveType::Model: {
                if (assetMgr)
                    buildAssetDropdown(renderer.modelHandle,
                                       "Model",
                                       assetMgr->getAllOfType<raylib::Model>(),
                                       [](const AssetMetadata& m) {
                                           return m.path.empty()
                                               ? boost::uuids::to_string(m.id)
                                               : m.path.substr(m.path.find_last_of('/') + 1);
                                       });
                break;
            }
        }

        if (needsRegen && renderer.primitiveType != PrimitiveType::Model)
            renderer.generateModel();

        // Material selection
        if (assetMgr)
            buildAssetDropdown(renderer.materialHandle,
                               "Material",
                               assetMgr->getAllOfType<Material>(),
                               [](const AssetMetadata& m) {
                                   return m.path.empty()
                                       ? boost::uuids::to_string(m.id)
                                       : m.path.substr(m.path.find_last_of('/') + 1);
                               });

        // Model Info
        if (auto* model = renderer.getModel(assetMgr)) {
            labeledField("Model Info", [&] {
                int verts = 0, tris = 0;
                for (int i = 0; i < model->meshCount; ++i) {
                    verts += model->meshes[i].vertexCount;
                    tris += model->meshes[i].triangleCount;
                }
                ImGui::Text("%d vertices, %d triangles", verts, tris);
            });
        }
    }
};

}
