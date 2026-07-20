/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "AtmosphereRenderer.h"

#include <glm/glm.hpp>
#include <webgpu/base/Framebuffer.h>
#include <webgpu/base/RenderResourceRegistry.h>
#include <webgpu/base/raii/RenderPassEncoder.h>
#include <webgpu/base/RenderGraph.h>

namespace webgpu_engine {

AtmosphereRenderer::AtmosphereRenderer()
    : QObject { nullptr }
{
}

void AtmosphereRenderer::init(webgpu::Context& ctx)
{
    m_ctx = &ctx;

    auto& reg = ctx.resource_registry();
    reg.register_shader("render_atmosphere", "webgpu_engine::render_atmosphere");
    reg.register_pipeline([this](WGPUDevice dev, const webgpu::RenderResourceRegistry& reg) {
        webgpu::FramebufferFormat format {};
        format.depth_format = WGPUTextureFormat_Undefined;
        format.color_formats.emplace_back(WGPUTextureFormat_RGBA8Unorm);
        m_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(dev,
            reg.shader("render_atmosphere"),
            reg.shader("render_atmosphere"),
            std::vector<webgpu::util::SingleVertexBufferInfo> {},
            format,
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("camera") });
    });
}

void AtmosphereRenderer::resize(int /*w*/, int h)
{
    webgpu::FramebufferFormat format(m_pipeline->framebuffer_format());
    format.size = glm::uvec2(1, h);
    m_atmosphere_framebuffer = std::make_unique<webgpu::Framebuffer>(m_ctx->device(), format);
}

void AtmosphereRenderer::draw(const WGPUCommandEncoder& command_encoder, const WGPUBindGroup& camera_bind_group)
{
    std::unique_ptr<webgpu::raii::RenderPassEncoder> render_pass = m_atmosphere_framebuffer->begin_render_pass(command_encoder);
    wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, camera_bind_group, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(render_pass->handle(), m_pipeline->pipeline().handle());
    wgpuRenderPassEncoderDraw(render_pass->handle(), 3, 1, 0, 0);
}

webgpu::rg::TextureHandle AtmosphereRenderer::draw(webgpu::rg::RenderGraph* rg, const WGPUBindGroup& camera_bind_group)
{
    auto s = m_atmosphere_framebuffer->size();

    auto renderTarget = rg->create_transient_texture("atmosphere_framebuffer", 
        {
            .dimension = WGPUTextureDimension_2D,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .absolute = {s.x, s.y, 1},
        });

    
    rg->add_pass("Atmosphere", webgpu::rg::PassKind::Graphics, 
    [&](webgpu::rg::PassBuilder& b) {
        b.color(renderTarget, 0);
    },
    [camera_bind_group, pipeline = m_pipeline->pipeline().handle()] (webgpu::rg::PassContext& ctx) {
            wgpuRenderPassEncoderSetBindGroup(ctx.render_pass, 0, camera_bind_group, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(ctx.render_pass, pipeline);
            wgpuRenderPassEncoderDraw(ctx.render_pass, 3, 1, 0, 0);
        }
    );

    return renderTarget;
}

const webgpu::raii::TextureView* AtmosphereRenderer::result_view() const { return &m_atmosphere_framebuffer->color_texture_view(0); }

} // namespace webgpu_engine
