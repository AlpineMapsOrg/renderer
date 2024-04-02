/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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
#include "nucleus/Raster.h"
#include "nucleus/utils/ColourTexture.h"
#include <QImage>
#include <map>
#include <webgpu/webgpu.h>

namespace webgpu_engine::raii {

class TextureView; // forward declaration

/// Represents (web)GPU texture.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
/// Preferably to be used with std::unique_ptr or std::shared_ptr.
class Texture : public GpuResource<WGPUTexture, WGPUTextureDescriptor, WGPUDevice> {
public:
    static const std::map<QImage::Format, WGPUTextureFormat> qimage_to_webgpu_format;

public:
    using GpuResource::GpuResource;

    void write(WGPUQueue queue, QImage image, uint32_t layer = 0);

    // TODO could make this a function template and pass type instead of using uint16_t? but not needed rn
    void write(WGPUQueue queue, const nucleus::Raster<uint16_t>& data, uint32_t layer = 0);

    void write(WGPUQueue queue, const nucleus::utils::ColourTexture& data, uint32_t layer = 0);

    std::unique_ptr<TextureView> create_view(const WGPUTextureViewDescriptor& desc) const;
};

} // namespace webgpu_engine::raii
