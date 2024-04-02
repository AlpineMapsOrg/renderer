/*****************************************************************************
 * weBIGeo
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

#pragma once

#include "base_types.h"
#include <webgpu/webgpu.h>

namespace webgpu_engine::raii {

/// Represents (web)GPU texture view, that can be used as entry in a bind group.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
/// Preferably to be used with std::unique_ptr or std::shared_ptr.
/// Usually obtained via call to Texture::create_view rather than using constructor directly.
///
/// TODO decide whether RAII is needed for WGPUTextureView objects
class TextureView : public GpuResource<WGPUTextureView, WGPUTextureViewDescriptor, WGPUTexture> {
public:
    using GpuResource::GpuResource;

    WGPUTextureViewDimension dimension() const;
};

} // namespace webgpu_engine::raii
