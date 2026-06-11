/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include <webgpu/webgpu.h>

namespace webgpu {

class Context;
namespace raii {
    class Texture;
}

// Computes the mipmap chain for the given RGBA8Unorm texture using a compute shader.
// The required shader (webgpu::mipmap) and its bind group layout are lazily registered in
// the context's resource registry on first use, so callers need no separate setup.
void compute_mipmaps_for_texture(Context& ctx, const raii::Texture* texture);

// Async overload: calls on_done after the mipmap work is submitted to the queue.
void compute_mipmaps_for_texture(Context& ctx, const raii::Texture* texture, WGPUQueueWorkDoneCallbackInfo on_done);

} // namespace webgpu
