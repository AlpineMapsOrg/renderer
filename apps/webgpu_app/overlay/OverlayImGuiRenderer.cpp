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

#include "OverlayImGuiRenderer.h"

#include <cstdio>
#include <imgui.h>

namespace webgpu_app {

OverlayImGuiRenderer::OverlayImGuiRenderer(webgpu_engine::Overlay& overlay)
    : m_overlay(&overlay)
{
}

std::string OverlayImGuiRenderer::effective_name() const { return m_overlay->name.empty() ? display_name() : m_overlay->name; }

bool OverlayImGuiRenderer::render_settings()
{
    bool changed = false;

    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", m_overlay->name.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##overlay_name", display_name().c_str(), buf, sizeof(buf))) {
        m_overlay->name = buf;
        changed = true;
    }
    ImGui::Separator();

    changed |= render_custom_settings();
    return changed;
}

} // namespace webgpu_app
