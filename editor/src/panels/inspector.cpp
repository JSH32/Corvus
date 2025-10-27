#include "editor/panels/inspector/inspector.hpp"
#include "IconsFontAwesome6.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "linp/asset/asset_manager.hpp"
#include "linp/log.hpp"

namespace Linp::Editor {
void InspectorPanel::onUpdate() {
    ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector");

    if (sceneHierarchy->selectedEntity) {
        drawAllComponents(sceneHierarchy->selectedEntity, assetManager);

        // Add Component section
        ImGui::Separator();
        if (ImGui::Button(ICON_FA_PLUS " Add Component",
                          ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
            ImGui::OpenPopup("AddComponent");
        }

        if (ImGui::BeginPopup("AddComponent")) {
            drawAddComponentMenu(sceneHierarchy->selectedEntity);
            ImGui::EndPopup();
        }
    } else {
        ImGui::TextDisabled("No entity selected");
    }

    ImGui::End();
}

template <std::size_t I>
void InspectorPanel::drawAllComponents(Core::Entity entity, Core::AssetManager* assetManager) {
    if constexpr (I < std::tuple_size_v<DrawableComponents>) {
        using ComponentType = std::tuple_element_t<I, DrawableComponents>;

        if constexpr (HasComponentInfo<ComponentType>) {
            if (entity.hasComponent<ComponentType>()) {
                auto& component = entity.getComponent<ComponentType>();
                drawComponentImpl<ComponentType>(entity,
                                                 assetManager,
                                                 std::string(ComponentInfo<ComponentType>::name),
                                                 ComponentInfo<ComponentType>::removable,
                                                 ComponentInfo<ComponentType>::flat);
            }
        }

        // Recurse to next component
        drawAllComponents<I + 1>(entity, assetManager);
    }
}

template <typename T>
void InspectorPanel::drawComponentImpl(Core::Entity        entity,
                                       Core::AssetManager* assetManager,
                                       const std::string&  name,
                                       bool                removable,
                                       bool                flat) {
    auto& component = entity.getComponent<T>();

    if (flat) {
        // Still show settings button if removable
        if (removable) {
            ImGui::SameLine();
            const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
            const auto  contentRegionAvailable = ImGui::GetContentRegionAvail();

            ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));
            if (ImGui::Button(ICON_FA_GEAR, ImVec2 { lineHeight, lineHeight })) {
                ImGui::OpenPopup("ComponentSettings");
            }
            ImGui::PopStyleColor(3);

            bool removeComponent = false;
            if (ImGui::BeginPopup("ComponentSettings")) {
                if (ImGui::MenuItem(ICON_FA_TRASH " Remove component")) {
                    removeComponent = true;
                }
                ImGui::EndPopup();
            }

            if (removeComponent) {
                entity.removeComponent<T>();
                return; // Early return to avoid drawing
            }
        }

        // Draw component content directly
        ImGui::PushID(reinterpret_cast<void*>(typeid(T).hash_code()));
        ComponentInfo<T>::draw(component, assetManager);
        ImGui::PopID();
    } else {
        constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen
            | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
        const auto contentRegionAvailable = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2 { 4, 4 });
        const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
        ImGui::Separator();
        const bool open = ImGui::TreeNodeEx(
            reinterpret_cast<void*>(typeid(T).hash_code()), treeNodeFlags, "%s", name.c_str());
        ImGui::PopStyleVar();

        // Settings button
        ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));
        if (ImGui::Button(ICON_FA_GEAR, ImVec2 { lineHeight, lineHeight })) {
            ImGui::OpenPopup("ComponentSettings");
        }
        ImGui::PopStyleColor(3);

        bool removeComponent = false;
        if (ImGui::BeginPopup("ComponentSettings")) {
            if (!removable)
                ImGui::BeginDisabled();
            if (ImGui::MenuItem(ICON_FA_TRASH " Remove component")) {
                removeComponent = true;
            }
            if (!removable)
                ImGui::EndDisabled();
            ImGui::EndPopup();
        }

        if (open) {
            ImGui::PushID(reinterpret_cast<void*>(typeid(T).hash_code()));
            ComponentInfo<T>::draw(component, assetManager);
            ImGui::PopID();
            ImGui::TreePop();
        }

        if (removeComponent) {
            entity.removeComponent<T>();
        }
    }
}

template <std::size_t I>
void InspectorPanel::drawAddComponentMenu(Core::Entity entity) {
    if constexpr (I < std::tuple_size_v<DrawableComponents>) {
        using ComponentType = std::tuple_element_t<I, DrawableComponents>;

        if constexpr (HasComponentInfo<ComponentType>) {
            // Only show components that can be added (removable) and aren't already present
            bool shouldShow
                = (ComponentInfo<ComponentType>::removable || !entity.hasComponent<ComponentType>())
                && !entity.hasComponent<ComponentType>();

            if (shouldShow) {
                std::string componentName = std::string(ComponentInfo<ComponentType>::name);

                if (ImGui::MenuItem(componentName.c_str())) {
                    entity.addComponent<ComponentType>();
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        // Recurse to next component
        drawAddComponentMenu<I + 1>(entity);
    }
}
}
