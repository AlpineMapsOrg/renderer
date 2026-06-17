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

namespace webgpu::raii {

class RenderPassEncoder : public GpuResource<WGPURenderPassEncoder, WGPURenderPassDescriptor, WGPUCommandEncoder> {
public:
    // Default constructor
    RenderPassEncoder(WGPUCommandEncoder context, const WGPURenderPassDescriptor& descriptor)
        : GpuResource(context, descriptor)
    {
    }

    // Creates a default render pass for a given color and depth attachment
    // The render pass will clear the color and depth attachments
    RenderPassEncoder(
        WGPUCommandEncoder encoder, WGPUTextureView color_attachment, WGPUTextureView depth_attachment, WGPUPassTimestampWrites* timestamp_writes = nullptr)
        : GpuResource(encoder, create_render_pass_descriptor(color_attachment, depth_attachment, timestamp_writes))
    {
    }

private:
    // We need those to live outside the scope of create_render_pass_descriptor
    // Better solutions welcome!
    WGPURenderPassColorAttachment m_color_attachment;
    WGPURenderPassDepthStencilAttachment m_depth_stencil_attachment;

    WGPURenderPassDescriptor create_render_pass_descriptor(
        WGPUTextureView color_attachment, WGPUTextureView depth_attachment, WGPUPassTimestampWrites* timestamp_writes = nullptr);
};

} // namespace webgpu::raii
