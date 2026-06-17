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

#include "OverlayNodeRenderer.h"

#include "compute/OverlayRenderNode.h"
#include <imgui.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

OverlayNodeRenderer::OverlayNodeRenderer(const std::string& name, nodes::OverlayRenderNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
{
}

void OverlayNodeRenderer::render_settings_content()
{
    auto settings = m_node->get_settings();

    const char* mode_items[] = { "Linked", "Copy" };
    int mode_idx = settings.copy ? 1 : 0;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##overlay_mode", &mode_idx, mode_items, 2)) {
        settings.copy = (mode_idx == 1);
        m_node->set_settings(settings);
    }
    ImGui::TextDisabled("Linked: sample source directly. Copy: own a copy (needs CopySrc).");
}

} // namespace webgpu_app
