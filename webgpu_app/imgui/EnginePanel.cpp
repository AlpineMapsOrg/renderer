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

#include "EnginePanel.h"

#include <IconsFontAwesome5.h>
#include <imgui.h>

#include "../TerrainRenderer.h"
#include "webgpu_engine/Window.h"

namespace webgpu_app {

EnginePanel::EnginePanel(TerrainRenderer* terrain_renderer)
    : m_terrain_renderer(terrain_renderer)
{
}

void EnginePanel::draw_panel()
{
    if (ImGui::CollapsingHeader(ICON_FA_COGS "  Engine Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto webgpu_window = m_terrain_renderer->get_webgpu_window();
        if (webgpu_window) {
            webgpu_window->paint_gui();
        }
    }
}

} // namespace webgpu_app
