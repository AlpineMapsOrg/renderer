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

#include "RenderPassEncoder.h"

namespace webgpu::raii {

WGPURenderPassDescriptor RenderPassEncoder::create_render_pass_descriptor(
    WGPUTextureView color_attachment, WGPUTextureView depth_attachment, WGPUPassTimestampWrites* timestamp_writes)
{
    m_color_attachment = {};
    m_color_attachment.view = color_attachment;
    m_color_attachment.resolveTarget = nullptr;
    m_color_attachment.loadOp = WGPULoadOp::WGPULoadOp_Clear;
    m_color_attachment.storeOp = WGPUStoreOp::WGPUStoreOp_Store;
    m_color_attachment.clearValue = WGPUColor { 0.0, 0.0, 0.0, 0.0 };
    m_color_attachment.depthSlice = UINT32_MAX;

    WGPURenderPassDescriptor render_pass_desc {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &m_color_attachment;

    if (depth_attachment != nullptr) {
        m_depth_stencil_attachment = {};
        m_depth_stencil_attachment.view = depth_attachment;
        m_depth_stencil_attachment.depthClearValue = 1.0f;
        m_depth_stencil_attachment.depthLoadOp = WGPULoadOp::WGPULoadOp_Clear;
        m_depth_stencil_attachment.depthStoreOp = WGPUStoreOp::WGPUStoreOp_Store;
        m_depth_stencil_attachment.depthReadOnly = false;
        m_depth_stencil_attachment.stencilClearValue = 0;
        m_depth_stencil_attachment.stencilLoadOp = WGPULoadOp::WGPULoadOp_Undefined;
        m_depth_stencil_attachment.stencilStoreOp = WGPUStoreOp::WGPUStoreOp_Undefined;
        m_depth_stencil_attachment.stencilReadOnly = true;
        render_pass_desc.depthStencilAttachment = &m_depth_stencil_attachment;
    }

    render_pass_desc.timestampWrites = timestamp_writes;

    return render_pass_desc;
}

} // namespace webgpu::raii
