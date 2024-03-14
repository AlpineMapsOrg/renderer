/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#pragma once

#include <imgui.h>
#include <cstdint>

static inline ImVec4 rgba8(uint8_t r, uint8_t g, uint8_t b, float a) {
    return ImVec4(
        r / (float)UINT8_MAX,
        g / (float)UINT8_MAX,
        b / (float)UINT8_MAX,
        a
        );
}

/// <summary>
/// Needs to be called after the setup of the dear gui and changes the color theme.
/// WARNING: lightMode is buggy!
/// </summary>
/// <param name="darkMode">dark/light mode flag</param>
/// <param name="alpha">The overall alpha level of the theme</param>
static inline void activateImGuiStyle(bool darkMode = true, float alpha = 0.2F) {
    ImGuiStyle& style = ImGui::GetStyle();

    auto COL_BACKGROUND = rgba8(255, 255, 255, 0.8f); // Pure white background with slight transparency
    auto COL_ACCENT = rgba8(235, 235, 245, 0.9f); // Very light gray with a hint of blue
    auto COL_ACCENT_DARKER = rgba8(200, 200, 210, 0.9f); // Light gray with a subtle blue tint
    auto COL_ACCENT_EVEN_DARKER = rgba8(150, 150, 160, 0.9f); // Medium gray with a fine blue tint


    style.Alpha = 1.0f;
    style.FrameRounding = 3.0f;
    style.WindowRounding = 0.0f;

           // Increase padding for ImGui windows
    style.WindowPadding = ImVec2(10.0f, 10.0f); // Adjust the values as needed

           // Increase padding for buttons (FramePadding is used for buttons and other framed widgets)
    style.FramePadding = ImVec2(5.0f, 5.0f); // Adjust the values as needed

           //style.ScrollbarRounding = 0;
    style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = COL_BACKGROUND;
    style.Colors[ImGuiCol_PopupBg] = COL_BACKGROUND;
    style.Colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_FrameBg] = COL_BACKGROUND;
    style.Colors[ImGuiCol_FrameBgHovered] = COL_ACCENT;
    style.Colors[ImGuiCol_FrameBgActive] = COL_ACCENT_DARKER;
    style.Colors[ImGuiCol_TitleBg] = COL_ACCENT_DARKER;
    style.Colors[ImGuiCol_TitleBgCollapsed] = COL_ACCENT;
    style.Colors[ImGuiCol_TitleBgActive] = COL_ACCENT_EVEN_DARKER;
    style.Colors[ImGuiCol_MenuBarBg] = COL_ACCENT_EVEN_DARKER;
    style.Colors[ImGuiCol_ScrollbarBg] = COL_BACKGROUND;
    style.Colors[ImGuiCol_ScrollbarGrab] = COL_ACCENT;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = COL_ACCENT_EVEN_DARKER;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = COL_ACCENT_DARKER;
    style.Colors[ImGuiCol_CheckMark] = COL_ACCENT_EVEN_DARKER;
    style.Colors[ImGuiCol_SliderGrab] = COL_ACCENT_EVEN_DARKER;
    style.Colors[ImGuiCol_SliderGrabActive] = COL_ACCENT_DARKER;
    //style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_Button] = COL_ACCENT;
    style.Colors[ImGuiCol_ButtonHovered] = COL_ACCENT_EVEN_DARKER;
    style.Colors[ImGuiCol_ButtonActive] = COL_ACCENT_DARKER;
    style.Colors[ImGuiCol_Header] = COL_ACCENT;
    style.Colors[ImGuiCol_HeaderHovered] = COL_ACCENT_EVEN_DARKER;
    style.Colors[ImGuiCol_HeaderActive] = COL_ACCENT_DARKER;
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

    if (darkMode)
    {
        for (int i = 0; i <= ImGuiCol_COUNT; i++)
        {
            ImVec4& col = style.Colors[i];
            float H, S, V;
            ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

            if (S < 0.1f)
            {
                V = 1.0f - V;
            }
            ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
            if (col.w < 1.00f)
            {
                col.w *= alpha;
            }
        }
    }
    else
    {
        for (int i = 0; i <= ImGuiCol_COUNT; i++)
        {
            ImVec4& col = style.Colors[i];
            if (col.w < 1.00f)
            {
                col.x *= alpha;
                col.y *= alpha;
                col.z *= alpha;
                col.w *= alpha;
            }
        }
    }
}
