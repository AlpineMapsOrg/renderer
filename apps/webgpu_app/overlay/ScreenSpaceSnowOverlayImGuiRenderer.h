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

#include "OverlayImGuiRenderer.h"
#include <webgpu/engine/overlay/ScreenSpaceSnowOverlay.h>

namespace webgpu_app {

class ScreenSpaceSnowOverlayImGuiRenderer : public OverlayImGuiRenderer {
public:
    explicit ScreenSpaceSnowOverlayImGuiRenderer(webgpu_engine::ScreenSpaceSnowOverlay& overlay);

    std::string display_name() const override { return "Screen-Space Snow"; }
    bool render_custom_settings() override;

private:
    webgpu_engine::ScreenSpaceSnowOverlay* m_snow_overlay;
};

} // namespace webgpu_app
