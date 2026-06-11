/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
 * Copyright (C) 2025 Patrick Komon
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

#include "OverlayRenderer.h"

#include "webgpu_engine/Context.h"
#include <algorithm>
#include <memory>
#include <webgpu/raii/RenderPassEncoder.h>

namespace webgpu_engine {

OverlayRenderer::OverlayRenderer()
    : QObject { nullptr }
{
}

void OverlayRenderer::add_overlay(std::shared_ptr<Overlay> overlay)
{
    // Auto-assign the highest positive z_index
    int max_z = 0;
    for (const auto& o : m_overlays)
        max_z = std::max(max_z, o->z_index);
    overlay->z_index = max_z + 1;

    if (m_ctx) {
        overlay->init(*m_ctx);
        if (m_is_ready)
            overlay->ready(m_ctx->webgpu_ctx());
    }
    m_overlays.push_back(std::move(overlay));
    rebucket();
}

void OverlayRenderer::remove_overlay(size_t index)
{
    if (index < m_overlays.size()) {
        m_overlays.erase(m_overlays.begin() + static_cast<ptrdiff_t>(index));
        rebucket();
    }
}

const std::vector<std::shared_ptr<Overlay>>& OverlayRenderer::overlays() const { return m_overlays; }

void OverlayRenderer::sort_overlays()
{
    std::sort(m_overlays.begin(), m_overlays.end(), [](const auto& a, const auto& b) { return a->z_index < b->z_index; });
    rebucket();
}

void OverlayRenderer::rebucket()
{
    m_pre_overlays.clear();
    m_post_overlays.clear();
    for (const auto& overlay : m_overlays)
        (overlay->z_index < 0 ? m_pre_overlays : m_post_overlays).push_back(overlay.get());
}

void OverlayRenderer::init(Context& ctx)
{
    m_ctx = &ctx;

    for (auto& overlay : m_overlays)
        overlay->init(ctx);
}

void OverlayRenderer::ready(webgpu::Context& ctx)
{
    m_is_ready = true;
    for (auto& overlay : m_overlays)
        overlay->ready(ctx);
}

std::unique_ptr<webgpu::raii::TextureWithSampler> OverlayRenderer::create_output_texture(int w, int h, const char* label) const
{
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = label, .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension_2D;
    texture_desc.size = { uint32_t(w), uint32_t(h), 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat_RGBA8Unorm;
    texture_desc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_TextureBinding;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = label, .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode_Nearest;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sampler_desc.minFilter = WGPUFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    return std::make_unique<webgpu::raii::TextureWithSampler>(m_ctx->webgpu_ctx().device(), texture_desc, sampler_desc);
}

void OverlayRenderer::resize(int w, int h)
{
    if (!m_ctx)
        return;
    m_pre[0] = create_output_texture(w, h, "overlay pre-shading texture 0");
    m_pre[1] = create_output_texture(w, h, "overlay pre-shading texture 1");
    m_post[0] = create_output_texture(w, h, "overlay post-shading texture 0");
    m_post[1] = create_output_texture(w, h, "overlay post-shading texture 1");
}

void OverlayRenderer::draw(const WGPUCommandEncoder& command_encoder,
    const webgpu::raii::TextureView& position_view,
    const webgpu::raii::TextureView& normal_view,
    const webgpu::raii::TextureView& overlay_view,
    const WGPUBindGroup& shared_config_bg,
    const WGPUBindGroup& camera_bg)
{
    const glm::uvec2 output_size(m_pre[0]->texture().width(), m_pre[0]->texture().height());
    draw_bucket(command_encoder, m_pre_overlays, m_pre, position_view, normal_view, overlay_view, shared_config_bg, camera_bg, output_size);
    draw_bucket(command_encoder, m_post_overlays, m_post, position_view, normal_view, overlay_view, shared_config_bg, camera_bg, output_size);
}

void OverlayRenderer::draw_bucket(const WGPUCommandEncoder& command_encoder,
    const std::vector<Overlay*>& bucket,
    TexturePair& tex,
    const webgpu::raii::TextureView& position_view,
    const webgpu::raii::TextureView& normal_view,
    const webgpu::raii::TextureView& overlay_view,
    const WGPUBindGroup& shared_config_bg,
    const WGPUBindGroup& camera_bg,
    glm::uvec2 output_size)
{
    // Start on T[N % 2] so the final write lands on index 0
    int current = static_cast<int>(bucket.size() % 2);

    // Clear the start texture
    WGPURenderPassColorAttachment clear_attachment {};
    clear_attachment.view = tex[static_cast<size_t>(current)]->texture_view().handle();
    clear_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    clear_attachment.loadOp = WGPULoadOp_Clear;
    clear_attachment.storeOp = WGPUStoreOp_Store;
    clear_attachment.clearValue = { 0.0, 0.0, 0.0, 0.0 };
    WGPURenderPassDescriptor clear_pass_desc {};
    clear_pass_desc.colorAttachmentCount = 1;
    clear_pass_desc.colorAttachments = &clear_attachment;
    {
        webgpu::raii::RenderPassEncoder clear_pass(command_encoder, clear_pass_desc);
    }

    for (auto& overlay : bucket) {
        const int target = current ^ 1;
        overlay->draw(command_encoder,
            position_view,
            normal_view,
            overlay_view,
            shared_config_bg,
            camera_bg,
            *tex[static_cast<size_t>(current)],
            *tex[static_cast<size_t>(target)],
            output_size);
        current = target;
    }
}

const webgpu::raii::TextureView* OverlayRenderer::result_pre_view() const { return m_pre[0] ? &m_pre[0]->texture_view() : nullptr; }

const webgpu::raii::TextureView* OverlayRenderer::result_post_view() const { return m_post[0] ? &m_post[0]->texture_view() : nullptr; }

} // namespace webgpu_engine
