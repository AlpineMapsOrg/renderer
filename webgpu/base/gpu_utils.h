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

#include <cstdint>
#include <webgpu/webgpu.h>

namespace webgpu {

class Context;
namespace raii {
    class Texture;
}

// Computes the mipmap chain for the given RGBA8Unorm texture using a compute shader
void compute_mipmaps_for_texture(Context& ctx, const raii::Texture* texture);

// Async overload: calls on_done after the mipmap work is submitted to the queue.
void compute_mipmaps_for_texture(Context& ctx, const raii::Texture* texture, WGPUQueueWorkDoneCallbackInfo on_done);

WGPUBindGroupEntry bind(uint32_t binding, WGPUTextureView view);
WGPUBindGroupEntry bind(uint32_t binding, WGPUSampler sampler);
WGPUBindGroupEntry bind(uint32_t binding, WGPUBuffer buffer, uint64_t offset, uint64_t size);


// block size in texels (1x1 for uncompressed) plus bytes per block
struct TexelBlock {
    uint32_t w, h, bytes;
};

// texel-block descriptor for a format. { 0, 0, 0 } for an unknown format
TexelBlock format_block(WGPUTextureFormat f);

// exact size in bytes of one texture, summed over its mip chain
uint64_t texture_bytes(WGPUExtent3D size, WGPUTextureFormat format, uint32_t mipLevelCount = 1, uint32_t sampleCount = 1, WGPUTextureDimension dim = WGPUTextureDimension_2D);

// the 256-byte row alignment WebGPU requires for buffer<->texture copies
inline uint32_t aligned_bytes_per_row(uint32_t widthTexels, uint32_t texelBlockBytes)
{
    constexpr uint32_t align = 256u;
    uint32_t raw = widthTexels * texelBlockBytes;
    return (raw + (align - 1u)) & ~(align - 1u);
}

// full format label (e.g. "RGBA8Unorm"), unknown formats return an empty string
const char* format_name(WGPUTextureFormat f);

// abbreviated format label for compact UI (e.g. "RGBA8"), unknown formats fall back to "tex"
const char* format_name_short(WGPUTextureFormat f);

} // namespace webgpu
