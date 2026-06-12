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

#include "HeightLinesOverlayImGuiRenderer.h"

#include <imgui.h>

namespace webgpu_app {

HeightLinesOverlayImGuiRenderer::HeightLinesOverlayImGuiRenderer(webgpu_engine::HeightLinesOverlay& overlay)
    : OverlayImGuiRenderer(overlay)
    , m_height_lines_overlay(&overlay)
{
}

bool HeightLinesOverlayImGuiRenderer::render_custom_settings()
{
    auto& s = m_height_lines_overlay->settings;
    bool changed = false;

    changed |= ImGui::SliderFloat("Primary Interval (m)", &s.primary_interval, 10.0f, 1000.0f);
    changed |= ImGui::SliderFloat("Secondary Interval (m)", &s.secondary_interval, 5.0f, 500.0f);
    changed |= ImGui::SliderFloat("Base Width", &s.base_width, 0.5f, 10.0f);
    changed |= ImGui::SliderFloat("Minor Opacity", &s.minor_opacity, 0.0f, 1.0f);
    changed |= ImGui::ColorEdit4("Line Color", (float*)&s.line_color);

    if (changed)
        m_height_lines_overlay->update_settings();

    return changed;
}

} // namespace webgpu_app
