/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include <QObject>
#include <SDL2/SDL.h>
#include <cstdint>
#include <webgpu/webgpu.h>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include "imgui/ImGuiPanel.h"
#include <memory>
#include <vector>
#endif

namespace webgpu_app {

class TerrainRenderer;

class ImGuiManager : public QObject {
    Q_OBJECT
public:
    explicit ImGuiManager(TerrainRenderer* terrain_renderer);

    void init(SDL_Window* window, WGPUDevice device, WGPUTextureFormat swapchainFormat, WGPUTextureFormat depthTextureFormat);
    void ready();
    void render(WGPURenderPassEncoder renderPass);
    void shutdown();

    bool want_capture_keyboard();
    bool want_capture_mouse();
    void on_sdl_event(SDL_Event& event);

    void set_gui_visibility(bool visible);
    bool get_gui_visibility() const;

    // Top-left Y for the next floating tool button
    static float s_tool_button_y;

    // Draws a floating 48x48 icon tool button at the next bottom-left stack slot (claims s_tool_button_y).
    static bool FloatingToggleButton(const char* id, const char* icon, const char* tooltip, uint32_t* enabled);

private:
    SDL_Window* m_window = nullptr;
    WGPUDevice m_device = {};
    TerrainRenderer* m_terrain_renderer = nullptr;
    bool m_gui_visible = true;

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    std::vector<std::unique_ptr<ImGuiPanel>> m_panels;
#endif

    void draw();
    void install_fonts();
};

} // namespace webgpu_app
