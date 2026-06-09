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

#include "AtmospherePanel.h"

#include "../ImGuiManager.h"
#include <IconsFontAwesome5.h>

#include <webgpu_engine/Context.h>

namespace webgpu_app {

AtmospherePanel::AtmospherePanel(webgpu_engine::Context* context)
    : m_context(context)
{
}

void AtmospherePanel::draw()
{
    auto& cfg = m_context->shared_config();
    if (ImGuiManager::FloatingToggleButton("ToggleAtmosphereButton", ICON_FA_GLOBE, "Atmosphere", &cfg.m_atmosphere_enabled))
        m_context->request_redraw();
}

} // namespace webgpu_app
