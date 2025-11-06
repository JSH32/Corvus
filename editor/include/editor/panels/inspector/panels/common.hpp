#pragma once

#include "IconsFontAwesome6.h"
#include "corvus/asset/material/material.hpp"
#include "corvus/components/entity_info.hpp"
#include "corvus/graphics/graphics.hpp"
#include "editor/imguiutils.hpp"
#include "editor/panels/inspector/inspector_panel.hpp"
#include "imgui_internal.h"
#include <cstring>
#include <imgui.h>
#include <string.h>

namespace Corvus::Editor {

template <>
struct ComponentInfo<Core::Components::EntityInfoComponent> {
    using ComponentType                         = Core::Components::EntityInfoComponent;
    static constexpr std::string_view name      = ICON_FA_CIRCLE_INFO " Entity Info";
    static constexpr bool             removable = false;
    static constexpr bool             flat      = true;

    static void draw(ComponentType& entityInfo, Core::AssetManager*, Graphics::GraphicsContext* _) {
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
struct ComponentInfo<Core::Components::TransformComponent> {
    using ComponentType                         = Core::Components::TransformComponent;
    static constexpr std::string_view name      = ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE " Transform";
    static constexpr bool             removable = false;
    static constexpr bool             flat      = false;

    static void draw(ComponentType& transform, Core::AssetManager*, Graphics::GraphicsContext* _) {
        // Position
        if (glm::vec3 position = transform.position; ImGui::Vector3Editor("Position", position))
            transform.position = position;

        // Scale
        if (glm::vec3 scale = transform.scale; ImGui::Vector3Editor("Scale", scale))
            transform.scale = scale;

        // Rotation
        if (glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(transform.rotation));
            ImGui::Vector3Editor("Rotation", eulerAngles))
            transform.rotation = glm::quat(glm::radians(eulerAngles));
    }
};

template <>
struct ComponentInfo<Core::Components::MeshRendererComponent> {
    using ComponentType                         = Core::Components::MeshRendererComponent;
    static constexpr std::string_view name      = ICON_FA_CUBE " Mesh Renderer";
    static constexpr bool             removable = true;
    static constexpr bool             flat      = false;

    static void draw(Core::Components::MeshRendererComponent& renderer,
                     Core::AssetManager*                      assetMgr,
                     Graphics::GraphicsContext*               ctx) {
        using namespace Corvus::Core;
        using namespace Corvus::Core::Components;

        ImGui::PushID(&renderer);

        // Helper, dropdown builder for assets
        auto buildAssetDropdown
            = [&](const char* label, auto& handle, auto assets, auto&& nameFromMeta) {
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

                  const char* currentLabel
                      = selected >= 0 && selected < static_cast<int>(names.size())
                      ? names[selected].c_str()
                      : "None";

                  ImGui::TextUnformatted(label);
                  ImGui::NextColumn();
                  ImGui::PushID(label);
                  ImGui::PushItemWidth(-1);
                  if (ImGui::BeginCombo("##Combo", currentLabel)) {
                      const bool noneSel = selected == -1;
                      if (ImGui::Selectable("None", noneSel))
                          handle = {};
                      if (noneSel)
                          ImGui::SetItemDefaultFocus();

                      for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                          const bool sel = i == selected;
                          if (ImGui::Selectable(names[i].c_str(), sel))
                              handle = assets[i];
                          if (sel)
                              ImGui::SetItemDefaultFocus();
                      }
                      ImGui::EndCombo();
                  }
                  ImGui::PopItemWidth();
                  ImGui::PopID();
                  ImGui::NextColumn();
              };

        // Start columns
        ImGui::Columns(2, "##MeshColumns", false);
        ImGui::SetColumnWidth(0, 100);

        // Primitive Type
        ImGui::TextUnformatted("Primitive");
        ImGui::NextColumn();
        {
            static const char* primitiveNames[]
                = { "Cube", "Sphere", "Plane", "Cylinder", "Model" };
            int current = static_cast<int>(renderer.primitiveType);
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo(
                    "##PrimitiveType", &current, primitiveNames, IM_ARRAYSIZE(primitiveNames))) {
                renderer.primitiveType = static_cast<PrimitiveType>(current);
                renderer.generateModel(*ctx);
            }
            ImGui::PopItemWidth();
        }
        ImGui::NextColumn();

        bool needsRegen = false;

        // Primitive Parameters
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
                if (assetMgr) {
                    const auto models = assetMgr->getAllOfType<Renderer::Model>();
                    buildAssetDropdown(
                        "Model", renderer.modelHandle, models, [](const AssetMetadata& m) {
                            return m.path.empty() ? boost::uuids::to_string(m.id)
                                                  : m.path.substr(m.path.find_last_of('/') + 1);
                        });
                }
                break;
            }
        }

        ImGui::Columns(1);

        if (needsRegen && renderer.primitiveType != PrimitiveType::Model)
            renderer.generateModel(*ctx);

        // Material Dropdown
        if (assetMgr) {
            ImGui::Columns(2, "##MaterialColumns", false);
            ImGui::SetColumnWidth(0, 100);

            const auto materials = assetMgr->getAllOfType<Corvus::Core::MaterialAsset>();
            buildAssetDropdown(
                "Material", renderer.materialHandle, materials, [](const AssetMetadata& m) {
                    return m.path.empty() ? boost::uuids::to_string(m.id)
                                          : m.path.substr(m.path.find_last_of('/') + 1);
                });

            ImGui::Columns(1);
        }

        // Model Info
        if (auto* model = renderer.getModel(assetMgr, ctx)) {
            ImGui::Columns(2, "##ModelInfo", false);
            ImGui::SetColumnWidth(0, 100);
            ImGui::TextUnformatted("Model Info");
            ImGui::NextColumn();

            // Try to extract model mesh info from your internal mesh data
            int vertices = 0, tris = 0;
            for (const auto& meshPtr : model->getMeshes()) {
                if (!meshPtr)
                    continue;
                vertices += static_cast<int>(meshPtr->getVertexCount());
                tris += static_cast<int>(meshPtr->getIndexCount() / 3);
            }

            ImGui::Text("%d vertices, %d triangles", vertices, tris);
            ImGui::Columns(1);
        }

        ImGui::PopID();
    }
};

}
