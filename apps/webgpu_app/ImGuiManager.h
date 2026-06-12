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

struct ImFont;
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <webgpu/webgpu.h>

#include "ui/ImGuiPanel.h"

namespace webgpu_app {

class App;

class ImGuiManager : public QObject {
    Q_OBJECT
public:
    explicit ImGuiManager(App* terrain_renderer);

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

    // Smaller font for compact UIs like the node graph editor (12 px)
    static ImFont* s_node_font;

    // Draws a floating 48x48 icon tool button at the next bottom-left stack slot (claims s_tool_button_y).
    static bool FloatingToggleButton(const char* id, const char* icon, const char* tooltip, uint32_t* enabled);

    enum class FilePickerMode { Open, Save };

    // ImGui-style file picker. Call every frame inside an ImGui frame. wants_open=true triggers the dialog to open.
    // Returns true (once) when the user has confirmed a selection; out_paths is filled with selected paths.
    // In Save mode on web, returns true immediately with a synthetic /download/<default_filename> path.
    // NOTE: Uses WebInterop to be platform independent
    static bool FilePicker(const char* dialog_id,
        const char* title,
        const char* filters,
        bool wants_open,
        std::vector<std::string>& out_paths,
        bool allow_multiple = false,
        const char* initial_path = ".",
        FilePickerMode mode = FilePickerMode::Open,
        const char* default_filename = "");

    // No-op on native; triggers a browser file download on web for the given MEMFS path.
    static void finalize_save(const std::string& path);

#ifdef __EMSCRIPTEN__
private slots:
    void on_file_uploaded(const std::string& filename, const std::string& tag);
#endif

private:
    struct FilePickerState {
        bool is_open = false;
        std::vector<std::string> pending;
    };
    static std::unordered_map<std::string, FilePickerState> s_picker_states;

    SDL_Window* m_window = nullptr;
    WGPUDevice m_device = {};
    App* m_terrain_renderer = nullptr;
    bool m_gui_visible = true;

    std::vector<std::unique_ptr<ImGuiPanel>> m_panels;

    void draw();
    void install_fonts();
};

} // namespace webgpu_app
