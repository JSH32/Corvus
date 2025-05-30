#include "editor/panels/scene_hierarchy.hpp"

#include "imgui.h"
#include "linp/components/entity_info.hpp"

namespace Linp::Editor {
void SceneHierarchyPanel::drawEntity(Core::Entity entity) const {
    const auto& entityInfo = entity.getComponent<Core::Components::EntityInfoComponent>();

    constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen
        | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_SpanAvailWidth;

    auto treeNode = treeNodeFlags;
    if (entity == selectedEntity)
        treeNode = treeNodeFlags | ImGuiTreeNodeFlags_Selected;

    // Darken if disabled
    if (!entityInfo.enabled)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

    if (ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity,
                          treeNode,
                          ICON_FA_CUBE " %s",
                          entityInfo.tag.c_str())) {
        if (ImGui::IsItemClicked(0))
            selectedEntity = entity;

        ImGui::TreePop();
    }

    if (!entityInfo.enabled)
        ImGui::PopStyleVar();

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            scene.destroyEntity(entity);
            if (selectedEntity == entity)
                selectedEntity = {};
        }

        ImGui::EndPopup();
    }
}

bool SceneHierarchyPanel::isFocused() { return windowFocused; }

void SceneHierarchyPanel::onUpdate() {
    if (ImGui::Begin(ICON_FA_LIST_UL " Hierarchy")) {
        windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        for (auto& entity : scene.getRootOrderedEntities())
            drawEntity(entity);

        if (ImGui::BeginPopupContextWindow(
                nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Create New Entity"))
                scene.createEntity("New Entity");

            ImGui::EndPopup();
        }
    }

    ImGui::End();
}
}
