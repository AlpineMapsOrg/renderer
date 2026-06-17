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

#include "TileDebugOverlayImGuiRenderer.h"

#include <imgui.h>

namespace webgpu_app {

TileDebugOverlayImGuiRenderer::TileDebugOverlayImGuiRenderer(webgpu_engine::TileDebugOverlay& overlay)
    : OverlayImGuiRenderer(overlay)
    , m_tile_debug_overlay(&overlay)
{
}

bool TileDebugOverlayImGuiRenderer::render_custom_settings()
{
    auto& s = m_tile_debug_overlay->settings;
    bool changed = false;

    // Combo order must match TileDebugOverlay::Mode
    static const char* mode_items[] = { "Normals", "Tiles", "Zoomlevel", "Vertex-ID" };
    int current = s.mode - 1;
    if (ImGui::Combo("Mode", &current, mode_items, IM_ARRAYSIZE(mode_items))) {
        s.mode = current + 1;
        m_tile_debug_overlay->update_settings(); // pushes the new mode into shared_config
        changed = true;
    }

    if (ImGui::SliderFloat("Strength", &s.strength, 0.0f, 1.0f)) {
        m_tile_debug_overlay->update_settings();
        changed = true;
    }

    return changed;
}

} // namespace webgpu_app
