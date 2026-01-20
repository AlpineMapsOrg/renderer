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

#include "dark_mode.h"

#if defined(_WIN32) || defined(_WIN64) // Windows
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include <imgui.h>
#endif

namespace webgpu_app::util {

void enable_darkmode_on_windows(SDL_Window* window)
{
    if (!window)
        return;

#if defined(_WIN32) || defined(_WIN64)
    // Windows: Enable dark mode for the title bar
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info)) {
        HWND hwnd = info.info.win.window;
        if (hwnd) {
            BOOL useDarkMode = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

            // Force the window to refresh by changing its size slightly
            RECT rect;
            if (GetWindowRect(hwnd, &rect)) {
                int width = rect.right - rect.left;
                int height = rect.bottom - rect.top;
                SetWindowPos(hwnd, nullptr, rect.left, rect.top, width + 1, height, SWP_NOZORDER | SWP_NOMOVE);
                SetWindowPos(hwnd, nullptr, rect.left, rect.top, width, height, SWP_NOZORDER | SWP_NOMOVE);
            }

            // Alternatively, force redraw using RedrawWindow
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
        }
    }

#endif
}

void setup_darkmode_imgui_style()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.7f, 0.7f, 0.7f, 0.8f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.80f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.80f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.80f);

    colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f); // Hover Accent
    colors[ImGuiCol_FrameBgActive] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.80f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.14f, 0.80f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.14f, 0.80f);

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0 / 255.0f, 101 / 255.0f, 153 / 255.0f, 1.00f); // Checkbox tick
    colors[ImGuiCol_SliderGrab] = ImVec4(0 / 255.0f, 101 / 255.0f, 153 / 255.0f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0 / 255.0f, 101 / 255.0f, 153 / 255.0f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0 / 255.0f, 101 / 255.0f, 153 / 255.0f, 1.00f); // Button color
    colors[ImGuiCol_ButtonHovered] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);

    // Keep Headers Unchanged
    colors[ImGuiCol_Header] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);

    colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);

    colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);

    colors[ImGuiCol_TextSelectedBg] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.80f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.80f);

    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;
#endif
}

} // namespace webgpu_app::util
