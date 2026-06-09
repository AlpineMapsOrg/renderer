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

#include "BufferToTextureNodeRenderer.h"

#include <imgui.h>
#include <webgpu_engine/compute/nodes/BufferToTextureNode.h>

namespace webgpu_app {
namespace nodes = webgpu_engine::compute::nodes;

BufferToTextureNodeRenderer::BufferToTextureNodeRenderer(const std::string& name, nodes::BufferToTextureNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
{
}

void BufferToTextureNodeRenderer::render_settings_content()
{
    auto& s = m_node->settings();
    bool changed = false;

    changed |= ImGui::Checkbox("Alpha Blending", &s.use_transparency_buffer);
    if (s.use_transparency_buffer) {
        ImGui::DragFloat2("Alpha Bounds", &s.transparency_map_bounds.x, 1.0f, 0.0f, 1000.0f, "%.2f");
        if (s.transparency_map_bounds.x > s.transparency_map_bounds.y)
            s.transparency_map_bounds.x = s.transparency_map_bounds.y;
        changed |= ImGui::IsItemDeactivatedAfterEdit();
    }

    bool interpolation = (s.texture_filter_mode == WGPUFilterMode_Linear);
    if (ImGui::Checkbox("Interpolation & MipMaps", &interpolation)) {
        if (interpolation) {
            s.texture_filter_mode = WGPUFilterMode_Linear;
            s.texture_mipmap_filter_mode = WGPUMipmapFilterMode_Linear;
            s.texture_max_aniostropy = 16;
            s.create_mipmaps = true;
        } else {
            s.texture_filter_mode = WGPUFilterMode_Nearest;
            s.texture_mipmap_filter_mode = WGPUMipmapFilterMode_Nearest;
            s.texture_max_aniostropy = 1;
            s.create_mipmaps = false;
        }
        changed = true;
    }

    changed |= ImGui::DragFloat2("Color Bounds", &s.color_map_bounds.x, 1.0f, -10000.0f, 10000.0f, "%.2f");
    changed |= ImGui::Checkbox("Bin Interpolation", &s.use_bin_interpolation);

    if (changed)
        m_node->rerun();
}

} // namespace webgpu_app
