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

#include "OverlayImGuiRendererFactory.h"

#include "HeightLinesOverlayImGuiRenderer.h"
#include "ScreenSpaceSnowOverlayImGuiRenderer.h"
#include "TextureOverlayImGuiRenderer.h"
#include "TileDebugOverlayImGuiRenderer.h"

#include <webgpu/engine/overlay/HeightLinesOverlay.h>
#include <webgpu/engine/overlay/ScreenSpaceSnowOverlay.h>
#include <webgpu/engine/overlay/TextureOverlay.h>
#include <webgpu/engine/overlay/TileDebugOverlay.h>

namespace webgpu_app {

std::unique_ptr<OverlayImGuiRenderer> OverlayImGuiRendererFactory::create(webgpu_engine::Overlay& overlay)
{
    if (auto* o = dynamic_cast<webgpu_engine::HeightLinesOverlay*>(&overlay))
        return std::make_unique<HeightLinesOverlayImGuiRenderer>(*o);
    if (auto* o = dynamic_cast<webgpu_engine::ScreenSpaceSnowOverlay*>(&overlay))
        return std::make_unique<ScreenSpaceSnowOverlayImGuiRenderer>(*o);
    if (auto* o = dynamic_cast<webgpu_engine::TextureOverlay*>(&overlay))
        return std::make_unique<TextureOverlayImGuiRenderer>(*o);
    if (auto* o = dynamic_cast<webgpu_engine::TileDebugOverlay*>(&overlay))
        return std::make_unique<TileDebugOverlayImGuiRenderer>(*o);
    return std::make_unique<OverlayImGuiRenderer>(overlay);
}

} // namespace webgpu_app
