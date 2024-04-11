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

#include "Framebuffer.h"

#include <cassert>

namespace webgpu_engine {

Framebuffer::Framebuffer(WGPUDevice device, const FramebufferFormat& format)
    : m_device { device }
    , m_format { format }
    , m_color_textures(format.color_formats.size())
    , m_color_texture_views(format.color_formats.size())
{
    recreate_all_textures();
}

void Framebuffer::resize(const glm::uvec2& size)
{
    m_format.size = size;
    recreate_all_textures();
}

void Framebuffer::recreate_depth_texture()
{
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = "framebuffer depth texture";
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.format = m_format.depth_format;
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.size = { m_format.size.x, m_format.size.y, 1 };
    texture_desc.usage = WGPUTextureUsage::WGPUTextureUsage_RenderAttachment;
    texture_desc.viewFormatCount = 1;
    texture_desc.viewFormats = &m_format.depth_format;
    m_depth_texture = std::make_unique<raii::Texture>(m_device, texture_desc);

    WGPUTextureViewDescriptor view_desc {};
    view_desc.aspect = WGPUTextureAspect::WGPUTextureAspect_DepthOnly;
    view_desc.arrayLayerCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseMipLevel = 0;
    view_desc.dimension = WGPUTextureViewDimension::WGPUTextureViewDimension_2D;
    view_desc.format = texture_desc.format;
    m_depth_texture_view = m_depth_texture->create_view(view_desc);
}

void Framebuffer::recreate_color_texture(size_t index)
{
    assert(index < m_format.color_formats.size());

    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = "framebuffer color texture";
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.format = m_format.color_formats[index];
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.size = { m_format.size.x, m_format.size.y, 1 };
    texture_desc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    texture_desc.viewFormatCount = 1;
    texture_desc.viewFormats = &m_format.color_formats[index];

    m_color_textures[index] = std::make_unique<raii::Texture>(m_device, texture_desc);
    m_color_texture_views[index] = m_color_textures.back()->create_view();
}

void Framebuffer::recreate_all_textures()
{
    recreate_depth_texture();
    for (size_t i = 0; i < m_format.color_formats.size(); i++) {
        recreate_color_texture(i);
    }
}

glm::uvec2 Framebuffer::size() const { return m_format.size; }

const raii::TextureView& Framebuffer::color_texture_view(size_t index) { return *m_color_texture_views.at(index); }

std::unique_ptr<raii::RenderPassEncoder> Framebuffer::begin_render_pass(WGPUCommandEncoder encoder)
{
    std::vector<WGPURenderPassColorAttachment> render_pass_color_attachments;
    for (const auto& color_texture_view : m_color_texture_views) {
        WGPURenderPassColorAttachment render_pass_color_attachment {};
        render_pass_color_attachment.view = color_texture_view->handle();
        render_pass_color_attachment.resolveTarget = nullptr;
        render_pass_color_attachment.loadOp = WGPULoadOp::WGPULoadOp_Clear;
        render_pass_color_attachment.storeOp = WGPUStoreOp::WGPUStoreOp_Store;
        render_pass_color_attachment.clearValue = WGPUColor { 0.53, 0.81, 0.92, 1.0 };
        // depthSlice field for RenderPassColorAttachment (https://github.com/gpuweb/gpuweb/issues/4251)
        // this field specifies the slice to render to when rendering to a 3d texture (view)
        // passing a valid index but referencing a non-3d texture leads to an error
        // TODO use some constant that represents "undefined" for this value (I couldn't find a constant for this?)
        //     (I just guessed -1 (max unsigned int value) and it worked)
        render_pass_color_attachment.depthSlice = -1;
        render_pass_color_attachments.emplace_back(render_pass_color_attachment);
    }

    WGPURenderPassDepthStencilAttachment depth_stencil_attachment {};
    depth_stencil_attachment.view = m_depth_texture_view->handle();
    depth_stencil_attachment.depthClearValue = 1.0f;
    depth_stencil_attachment.depthLoadOp = WGPULoadOp::WGPULoadOp_Clear;
    depth_stencil_attachment.depthStoreOp = WGPUStoreOp::WGPUStoreOp_Store;
    depth_stencil_attachment.depthReadOnly = false;
    depth_stencil_attachment.stencilClearValue = 0;
    depth_stencil_attachment.stencilLoadOp = WGPULoadOp::WGPULoadOp_Undefined;
    depth_stencil_attachment.stencilStoreOp = WGPUStoreOp::WGPUStoreOp_Undefined;
    depth_stencil_attachment.stencilReadOnly = true;

    WGPURenderPassDescriptor render_pass_desc {};
    render_pass_desc.colorAttachmentCount = render_pass_color_attachments.size();
    render_pass_desc.colorAttachments = render_pass_color_attachments.data();
    render_pass_desc.depthStencilAttachment = &depth_stencil_attachment;
    render_pass_desc.timestampWrites = nullptr;
    return std::make_unique<raii::RenderPassEncoder>(encoder, render_pass_desc);
}

} // namespace webgpu_engine
