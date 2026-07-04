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
#include <vector>

namespace webgpu_app {

class App;

class CameraPanel : public ImGuiPanel {
public:
    explicit CameraPanel(App* terrain_renderer);

    void draw_panel() override;

private:
    App* m_terrain_renderer;
    std::vector<std::string> m_camera_preset_names;
    int m_selected_camera_preset = 0;
};

} // namespace webgpu_app
