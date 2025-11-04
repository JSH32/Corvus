#pragma once

#include "corvus/graphics/graphics.hpp"
#include <imgui.h>
#include <vector>

/**
 * Things inserted into the ImGui namespace, addon functions or utilities
 */
namespace ImGui {

// void TwoColumnBegin(const std::string_view& label, const float labelWidth);
bool Vector3Editor(const std::string& label, glm::vec3& vec, const float labelWidth = 100.0f);

bool FloatEditor(const std::string& label,
                 float&             value,
                 float              speed      = 0.1f,
                 float              min        = 0.0f,
                 float              max        = 0.0f,
                 float              resetValue = 0.0f,
                 const float        labelWidth = 100.0f);
bool IntEditor(const std::string& label,
               int&               value,
               int                speed      = 1,
               int                min        = 0,
               int                max        = 0,
               int                resetValue = 0,
               const float        labelWidth = 100.0f);

/**
 * @brief Renders a framebuffer's color texture to ImGui.
 *
 * @param framebuffer The framebuffer to render
 * @param colorTexture The color texture attached to the framebuffer
 * @param size Size to display the texture
 * @param flipY Whether to flip the Y axis
 * @return true if rendered successfully, false if framebuffer is invalid
 */
bool RenderFramebuffer(const Corvus::Graphics::Framebuffer& fb,
                       const Corvus::Graphics::Texture2D&   colorTex,
                       const ImVec2&                        size,
                       bool                                 flipY = true);

/**
 * @brief Renders a raw Texture2D to ImGui.
 *
 * @param tex The texture to render
 * @param size The display size in ImGui space
 * @param flipY Whether to flip vertically
 * @return true if rendered successfully, false otherwise
 */
bool RenderTexture(const Corvus::Graphics::Texture2D& tex, const ImVec2& size, bool flipY = true);
}
