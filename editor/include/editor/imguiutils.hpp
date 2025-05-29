#pragma once

#include <imgui.h>
#include <raylib-cpp.hpp>
#include <vector>

/**
 * Things inserted into the ImGui namespace, addon functions or utilities
 */
namespace ImGui {
// /**
//  * Get the position of the content area top and bottom corner
//  */
// std::tuple<ImVec2, ImVec2> GetContentArea();

// /**
//  * Calculate position on RenderTarget based on offset
//  *
//  * @param target RenderTarget to map to coordinate point
//  * @param pos Position relative to window
//  * @param contentStartPos Starting content position, click position will be relative to this point at (0, 0)
//  */
// raylib::Vector2 CalculateImOffset(const raylib::RenderTexture& renderTexture,
//     raylib::Vector2 mouseScreenPos,
//     ImVec2 windowTopCorner);

// void TwoColumnBegin(const std::string_view& label, const float labelWidth);
bool Vector3Editor(const std::string& label, raylib::Vector3& vec, const float labelWidth = 100.f);

bool FloatEditor(const std::string& label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f, float resetValue = 0.0f, const float labelWidth = 100.0f);
bool IntEditor(const std::string& label, int& value, int speed = 1, int min = 0, int max = 0, int resetValue = 0, const float labelWidth = 100.0f);
}
