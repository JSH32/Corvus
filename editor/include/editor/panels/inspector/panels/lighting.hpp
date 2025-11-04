#pragma once

#include "IconsFontAwesome6.h"
#include "corvus/components/light.hpp"
#include "editor/imguiutils.hpp"
#include "editor/panels/inspector/inspector_panel.hpp"
#include "imgui_internal.h"
#include <imgui.h>

namespace Corvus::Editor {

template <>
struct ComponentInfo<Corvus::Core::Components::LightComponent> {
    using ComponentType                         = Corvus::Core::Components::LightComponent;
    static constexpr std::string_view name      = ICON_FA_LIGHTBULB " Light";
    static constexpr bool             removable = true;
    static constexpr bool             flat      = false;

    static void draw(Corvus::Core::Components::LightComponent& light,
                     Corvus::Core::AssetManager*,
                     Graphics::GraphicsContext* ctx) {
        ImGui::PushID(&light);

        // Enabled checkbox
        ImGui::Checkbox("Enabled", &light.enabled);
        ImGui::Spacing();

        // Start persistent columns for all labeled fields
        ImGui::Columns(2, "##LightColumns", false);
        ImGui::SetColumnWidth(0, 100);

        // Light Type
        ImGui::TextUnformatted("Type");
        ImGui::NextColumn();
        {
            static const char* lightTypeNames[] = { "Directional", "Point", "Spot" };
            int                currentType      = static_cast<int>(light.type);
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo(
                    "##LightType", &currentType, lightTypeNames, IM_ARRAYSIZE(lightTypeNames))) {
                light.type = static_cast<Corvus::Core::Components::LightType>(currentType);
            }
            ImGui::PopItemWidth();
        }
        ImGui::NextColumn();

        // Color picker
        ImGui::TextUnformatted("Color");
        ImGui::NextColumn();
        {
            float color[4] = { light.color.r / 255.0f,
                               light.color.g / 255.0f,
                               light.color.b / 255.0f,
                               light.color.a / 255.0f };
            ImGui::PushItemWidth(-1);
            if (ImGui::ColorEdit3("##LightColor", color, ImGuiColorEditFlags_NoInputs)) {
                light.color.r = static_cast<unsigned char>(color[0] * 255);
                light.color.g = static_cast<unsigned char>(color[1] * 255);
                light.color.b = static_cast<unsigned char>(color[2] * 255);
            }
            ImGui::PopItemWidth();
        }
        ImGui::NextColumn();

        // End columns before non-columned content
        ImGui::Columns(1);

        // Intensity
        ImGui::FloatEditor("Intensity", light.intensity, 0.1f, 0.0f, 10.0f);

        // Type-specific properties
        switch (light.type) {
            case Corvus::Core::Components::LightType::Directional: {
                ImGui::Spacing();
                ImGui::TextDisabled("Direction is controlled by Transform rotation");
                break;
            }

            case Corvus::Core::Components::LightType::Point: {
                ImGui::Separator();
                ImGui::Text("Point Light Settings");

                ImGui::FloatEditor("Range", light.range, 0.5f, 0.1f, 100.0f);
                ImGui::FloatEditor("Attenuation", light.attenuation, 0.1f, 0.0f, 10.0f);

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Controls how quickly light fades with distance");
                }
                break;
            }

            case Corvus::Core::Components::LightType::Spot: {
                ImGui::Separator();
                ImGui::Text("Spot Light Settings");

                ImGui::FloatEditor("Range", light.range, 0.5f, 0.1f, 100.0f);
                ImGui::FloatEditor("Attenuation", light.attenuation, 0.1f, 0.0f, 10.0f);

                ImGui::Spacing();
                ImGui::Text("Cone Shape");

                ImGui::FloatEditor("Inner Cutoff", light.innerCutoff, 1.0f, 0.0f, 89.0f);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Inner cone angle (degrees) - full brightness");
                }

                ImGui::FloatEditor("Outer Cutoff", light.outerCutoff, 1.0f, 0.0f, 90.0f);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Outer cone angle (degrees) - edge falloff");
                }

                // Ensure outer is always >= inner
                if (light.outerCutoff < light.innerCutoff) {
                    light.outerCutoff = light.innerCutoff;
                }

                ImGui::Spacing();
                ImGui::TextDisabled("Direction is controlled by Transform rotation");
                break;
            }
        }

        // Shadow casting
        ImGui::Separator();
        // ImGui::BeginDisabled(true);
        ImGui::Checkbox("Cast Shadows", &light.castShadows);
        // ImGui::EndDisabled();

        // Shadow quality settings when shadows are enabled
        if (light.castShadows) {
            ImGui::Indent();

            // Continue columns for shadow settings
            ImGui::Columns(2, "##ShadowColumns", false);
            ImGui::SetColumnWidth(0, 100);

            // Shadow Resolution
            ImGui::TextUnformatted("Shadow Res");
            ImGui::NextColumn();
            {
                static const char* resolutions[] = { "512", "1024", "2048", "4096" };
                static const int   resValues[]   = { 512, 1024, 2048, 4096 };
                int                currentIdx    = 1; // default 1024
                for (int i = 0; i < 4; ++i) {
                    if (light.shadowMapResolution == resValues[i]) {
                        currentIdx = i;
                        break;
                    }
                }
                ImGui::PushItemWidth(-1);
                if (ImGui::Combo("##ShadowRes", &currentIdx, resolutions, 4)) {
                    light.shadowMapResolution = resValues[currentIdx];
                }
                ImGui::PopItemWidth();
            }
            ImGui::NextColumn();

            ImGui::Columns(1);

            ImGui::FloatEditor("Shadow Bias", light.shadowBias, 0.001f, 0.0f, 0.1f);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Prevents shadow acne artifacts");
            }

            ImGui::FloatEditor("Shadow Strength", light.shadowStrength, 0.05f, 0.0f, 1.0f);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("How dark the shadows are");
            }

            if (light.type == Corvus::Core::Components::LightType::Directional) {
                ImGui::FloatEditor("Shadow Distance", light.shadowDistance, 5.0f, 1.0f, 200.0f);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("How far shadows render from center");
                }
            }

            ImGui::Unindent();
        }

        // Visual preview helper
        ImGui::Separator();
        ImGui::Spacing();

        // Draw a small preview of the light
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2      p        = ImGui::GetCursorScreenPos();
        float       radius   = 20.0f;
        ImVec2      center   = ImVec2(p.x + radius + 5, p.y + radius + 5);

        // Clamp color values
        int   r             = std::min(255, static_cast<int>(light.color.r * light.intensity));
        int   g             = std::min(255, static_cast<int>(light.color.g * light.intensity));
        int   b             = std::min(255, static_cast<int>(light.color.b * light.intensity));
        ImU32 lightColorU32 = IM_COL32(r, g, b, 255);

        switch (light.type) {
            case Corvus::Core::Components::LightType::Directional:
                // Draw sun icon
                drawList->AddCircleFilled(center, radius * 0.6f, lightColorU32);
                for (int i = 0; i < 8; ++i) {
                    float  angle = (i / 8.0f) * 2.0f * 3.14159f;
                    ImVec2 start = ImVec2(center.x + cosf(angle) * radius * 0.7f,
                                          center.y + sinf(angle) * radius * 0.7f);
                    ImVec2 end
                        = ImVec2(center.x + cosf(angle) * radius, center.y + sinf(angle) * radius);
                    drawList->AddLine(start, end, lightColorU32, 2.0f);
                }
                break;

            case Corvus::Core::Components::LightType::Point:
                // Draw point light with falloff
                drawList->AddCircleFilled(center, radius * 0.4f, lightColorU32);
                drawList->AddCircle(
                    center, radius * 0.7f, IM_COL32(r / 2, g / 2, b / 2, 128), 0, 2.0f);
                drawList->AddCircle(center, radius, IM_COL32(r / 4, g / 4, b / 4, 64), 0, 1.0f);
                break;

            case Corvus::Core::Components::LightType::Spot:
                // Draw cone shape
                drawList->AddCircleFilled(center, radius * 0.3f, lightColorU32);
                ImVec2 p1 = ImVec2(center.x - radius * 0.5f, center.y + radius);
                ImVec2 p2 = ImVec2(center.x + radius * 0.5f, center.y + radius);
                drawList->AddTriangleFilled(center, p1, p2, IM_COL32(r / 2, g / 2, b / 2, 128));
                break;
        }

        ImGui::Dummy(ImVec2(radius * 2 + 10, radius * 2 + 10));

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("Light Preview");
        ImGui::Text("%.1f%% brightness", light.intensity * 100.0f);
        if (light.castShadows) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Shadows: ON");
        }
        ImGui::EndGroup();

        ImGui::PopID();
    }
};

}
