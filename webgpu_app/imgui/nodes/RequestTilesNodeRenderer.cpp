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

#include "RequestTilesNodeRenderer.h"

#include <imgui.h>

namespace webgpu_app {
namespace nodes = webgpu_engine::compute::nodes;

RequestTilesNodeRenderer::RequestTilesNodeRenderer(const std::string& name, nodes::RequestTilesNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
    , m_options({
          { "DTM tiles", { "https://alpinemaps.cg.tuwien.ac.at/tiles/at_dtm_alpinemaps/", nucleus::tile::TileLoadService::UrlPattern::ZXY, ".png" } },
          { "DSM tiles", { "https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", nucleus::tile::TileLoadService::UrlPattern::ZXY, ".png" } },
      })
{
}

void RequestTilesNodeRenderer::render_settings_content()
{
    std::string combo_items;
    for (const auto& opt : m_options)
        combo_items += opt.name + '\0';
    combo_items += '\0';

    if (ImGui::Combo("Tile source", &m_selected_index, combo_items.c_str())) {
        m_node->set_settings(m_options[m_selected_index].settings);
        m_node->rerun();
    }
}

} // namespace webgpu_app
