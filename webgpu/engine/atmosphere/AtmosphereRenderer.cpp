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

void AtmosphereRenderer::resize(int /*w*/, int h) { m_height = uint32_t(h); }

webgpu::rg::TextureHandle AtmosphereRenderer::draw(webgpu::rg::RenderGraph* rg, const WGPUBindGroup& camera_bind_group)
{
    auto renderTarget = rg->create_transient_texture("atmosphere_framebuffer",
        webgpu::rg::texture_2d(WGPUTextureFormat_RGBA8Unorm, 1, m_height));

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

} // namespace webgpu_engine
