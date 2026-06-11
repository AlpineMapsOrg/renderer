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

#include "ImGuiPanel.h"
#include <string>

namespace webgpu_engine {
class Context;
class TrackRenderer;
} // namespace webgpu_engine

namespace webgpu_app {

class TerrainRenderer;

class TrackPanel : public ImGuiPanel {
public:
    TrackPanel(webgpu_engine::Context* context, TerrainRenderer* terrain_renderer);
    void ready() override;
    void draw_panel() override;

private:
    // Loads the track for rendering and points the camera down at its bounding box.
    void load_track_and_focus(const std::string& path);

    webgpu_engine::Context* m_context;
    TerrainRenderer* m_terrain_renderer = nullptr;
    webgpu_engine::TrackRenderer* m_track_renderer = nullptr;
    std::string m_last_dialog_directory = ".";
    std::vector<std::string> m_picked_files;
};

} // namespace webgpu_app
