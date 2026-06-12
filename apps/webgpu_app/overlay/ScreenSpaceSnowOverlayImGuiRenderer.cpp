/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
 * Copyright (C) 2024 Patrick Komon
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

#include "ScreenSpaceSnowOverlayImGuiRenderer.h"

#include <imgui.h>

namespace webgpu_app {

ScreenSpaceSnowOverlayImGuiRenderer::ScreenSpaceSnowOverlayImGuiRenderer(webgpu_engine::ScreenSpaceSnowOverlay& overlay)
    : OverlayImGuiRenderer(overlay)
    , m_snow_overlay(&overlay)
{
}

bool ScreenSpaceSnowOverlayImGuiRenderer::render_custom_settings()
{
    auto& s = m_snow_overlay->settings;
    bool changed = false;

    changed |= ImGui::DragFloatRange2("Angle limit", &s.angle_min, &s.angle_max, 0.1f, 0.0f, 90.0f, "Min: %.1f°", "Max: %.1f°", ImGuiSliderFlags_AlwaysClamp);
    changed |= ImGui::SliderFloat("Angle blend", &s.angle_blend, 0.0f, 90.0f, "%.1f°");
    changed |= ImGui::SliderFloat("Altitude limit", &s.altitude_limit, 0.0f, 4000.0f, "%.1fm");
    changed |= ImGui::SliderFloat("Altitude variation", &s.altitude_variation, 0.0f, 1000.0f, "%.1fm");
    changed |= ImGui::SliderFloat("Altitude blend", &s.altitude_blend, 0.0f, 1000.0f, "%.1fm");
    changed |= ImGui::SliderFloat("Transparency", &s.transparency, 0.0f, 1.0f);

    if (changed)
        m_snow_overlay->update_settings();

    return changed;
}

} // namespace webgpu_app
