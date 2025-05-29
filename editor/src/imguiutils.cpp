#include "editor/imguiutils.hpp"
#include "imgui.h"
#include "imgui_internal.h"

namespace ImGui {

// std::tuple<ImVec2, ImVec2> GetContentArea() {
//     return {
//         { ImGui::GetWindowContentRegionMin().x + ImGui::GetWindowPos().x,
//             ImGui::GetWindowContentRegionMax().y + ImGui::GetWindowPos().y },
//         { ImGui::GetWindowContentRegionMax().x + ImGui::GetWindowPos().x,
//             ImGui::GetWindowContentRegionMin().y + ImGui::GetWindowPos().y }
//     };
// }

// raylib::Vector2 CalculateImOffset(const raylib::RenderTexture& target,
//     const raylib::Vector2 mousePos,
//     const ImVec2 contentStartPos) {
//     // Calculate offset position relative to the content area
//     float offsetX = mousePos.x - contentStartPos.x;
//     float offsetY = mousePos.y - contentStartPos.y;

//     return raylib::Vector2 {
//         offsetX,
//         target.texture.height - offsetY // Flip Y coordinate
//     };
// }

// void TwoColumnBegin(const std::string_view& label, const float labelWidth) {
//     ImGui::Columns(2);
//     ImGui::SetColumnWidth(0, labelWidth);
//     ImGui::Text("%s", label.data());
//     ImGui::NextColumn();
//     ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
// }

bool Vector3Editor(const std::string& label, raylib::Vector3& vec, const float labelWidth) {
    bool changed = false;
    ImGui::PushID(label.c_str());

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, labelWidth);
    ImGui::Text("%s", label.c_str());
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::GetContentRegionAvail().x - 33);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { 2, 5 });

    // X component
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 { 0.9f, 0.2f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 { 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::SetNextItemWidth(7);
    if (ImGui::Button("X")) {
        vec.x   = 0.0f;
        changed = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &vec.x, 0.1f, 0.0f, 0.0f, "%.2f"))
        changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Y component
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 { 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 { 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::SetNextItemWidth(7);
    if (ImGui::Button("Y")) {
        vec.y   = 0.0f;
        changed = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &vec.y, 0.1f, 0.0f, 0.0f, "%.2f"))
        changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Z component
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 { 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 { 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::SetNextItemWidth(7);
    if (ImGui::Button("Z")) {
        vec.z   = 0.0f;
        changed = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    if (ImGui::DragFloat("##Z", &vec.z, 0.1f, 0.0f, 0.0f, "%.2f"))
        changed = true;
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();
    ImGui::EndColumns();
    ImGui::PopID();
    return changed;
}

bool FloatEditor(const std::string& label, float& value, float speed, float min, float max, float resetValue, const float labelWidth) {
    bool changed = false;
    ImGui::PushID(label.c_str());

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, labelWidth);
    ImGui::Text("%s", label.c_str());
    ImGui::NextColumn();

    // Calculate widths - leave space for reset button
    float resetButtonWidth = 20.0f;
    float availableWidth   = ImGui::GetContentRegionAvail().x - resetButtonWidth - ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushItemWidth(availableWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { 2, 5 });

    // Reset button with orange color
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.8f, 0.5f, 0.1f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 { 0.9f, 0.6f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 { 0.8f, 0.5f, 0.1f, 1.0f });
    if (ImGui::Button("R", ImVec2(resetButtonWidth, 0))) {
        value   = resetValue;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Value", &value, speed, min, max, "%.2f"))
        changed = true;

    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::EndColumns();
    ImGui::PopID();
    return changed;
}

bool IntEditor(const std::string& label, int& value, int speed, int min, int max, int resetValue, const float labelWidth) {
    bool changed = false;
    ImGui::PushID(label.c_str());

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, labelWidth);
    ImGui::Text("%s", label.c_str());
    ImGui::NextColumn();

    // Calculate widths - leave space for reset button
    float resetButtonWidth = 20.0f;
    float availableWidth   = ImGui::GetContentRegionAvail().x - resetButtonWidth - ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushItemWidth(availableWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { 2, 5 });

    // Reset button with purple color
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.6f, 0.3f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 { 0.7f, 0.4f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 { 0.6f, 0.3f, 0.8f, 1.0f });
    if (ImGui::Button("R", ImVec2(resetButtonWidth, 0))) {
        value   = resetValue;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::DragInt("##Value", &value, speed, min, max))
        changed = true;

    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::EndColumns();
    ImGui::PopID();
    return changed;
}

}
