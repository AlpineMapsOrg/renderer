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
#include <set>

namespace webgpu_app {

class TerrainRenderer;

class TimingPanel : public ImGuiPanel {
public:
    explicit TimingPanel(TerrainRenderer* terrain_renderer);

    void draw_panel() override;

private:
    TerrainRenderer* m_terrain_renderer;
    std::set<uint32_t> m_selected_timer = {};

    void toggle_timer(uint32_t timer_id);
    bool is_timer_selected(uint32_t timer_id) const;
};

} // namespace webgpu_app
