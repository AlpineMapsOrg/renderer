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

namespace webgpu_app {

class ImGuiPanel {
public:
    virtual ~ImGuiPanel() = default;

    // Called once on the first rendered frame
    virtual void on_first_frame() {}

    // Called outside the main sidebar window
    virtual void draw() {}

    // Called inside the main sidebar ImGui::Begin/End block
    virtual void draw_panel() {}
};

} // namespace webgpu_app
