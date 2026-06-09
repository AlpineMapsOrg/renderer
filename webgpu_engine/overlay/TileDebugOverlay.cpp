/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "TileDebugOverlay.h"

#include "webgpu_engine/Context.h"
#include <webgpu/RenderResourceRegistry.h>
#include <webgpu/raii/BindGroup.h>
#include <webgpu/raii/BindGroupLayout.h>

namespace webgpu_engine {

TileDebugOverlay::TileDebugOverlay()
    : Overlay()
{
}

TileDebugOverlay::~TileDebugOverlay()
{
    if (m_engine_ctx)
        m_engine_ctx->shared_config().m_overlay_mode = 0;
}

void TileDebugOverlay::init(Context& context)
{
    webgpu::Context& ctx = context.webgpu_ctx();
    m_ctx = &ctx;
    m_engine_ctx = &context;

    auto& reg = ctx.resource_registry();
    if (!reg.has_shader("gbuffer_debug_compute"))
        reg.register_shader("gbuffer_debug_compute", "overlays/gbuffer_debug.wgsl");
    if (!reg.has_bind_group_layout("tile_debug_overlay"))
        reg.register_bind_group_layout("tile_debug_overlay", [](WGPUDevice device) {
            WGPUBindGroupLayoutEntry overlay_entry {};
            overlay_entry.binding = 0;
            overlay_entry.visibility = WGPUShaderStage_Compute;
            overlay_entry.texture.sampleType = WGPUTextureSampleType_Uint;
            overlay_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

            WGPUBindGroupLayoutEntry settings_entry {};
            settings_entry.binding = 1;
            settings_entry.visibility = WGPUShaderStage_Compute;
            settings_entry.buffer.type = WGPUBufferBindingType_Uniform;

            WGPUBindGroupLayoutEntry output_entry {};
            output_entry.binding = 2;
            output_entry.visibility = WGPUShaderStage_Compute;
            output_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
            output_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
            output_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

            WGPUBindGroupLayoutEntry prev_output_entry {};
            prev_output_entry.binding = 3;
            prev_output_entry.visibility = WGPUShaderStage_Compute;
            prev_output_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
            prev_output_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

            return std::make_unique<webgpu::raii::BindGroupLayout>(device,
                std::vector<WGPUBindGroupLayoutEntry> { overlay_entry, settings_entry, output_entry, prev_output_entry },
                "tile debug overlay bind group layout");
        });
    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        m_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(device,
            reg.shader("gbuffer_debug_compute"),
            std::vector<const webgpu::raii::BindGroupLayout*> {
                &reg.bind_group_layout("tile_debug_overlay"),
            },
            "tile debug compute pipeline");
    });

    m_settings_uniform = std::make_unique<webgpu::Buffer<GpuSettings>>(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    update_settings();
}

void TileDebugOverlay::update_settings()
{
    if (!m_settings_uniform)
        return;
    m_settings_uniform->data.strength = settings.strength;
    m_settings_uniform->update_gpu_data(m_ctx->queue());

    // NOTE: Only one TileDebugOverlay possible due to GBuffer layout
    if (m_engine_ctx)
        m_engine_ctx->shared_config().m_overlay_mode = static_cast<uint32_t>(settings.mode);
}

void TileDebugOverlay::draw(const WGPUCommandEncoder& command_encoder,
    const webgpu::raii::TextureView& /*position_view*/,
    const webgpu::raii::TextureView& /*normal_view*/,
    const webgpu::raii::TextureView& overlay_view,
    const WGPUBindGroup& /*shared_config_bg*/,
    const WGPUBindGroup& /*camera_bg*/,
    const webgpu::raii::TextureWithSampler& current_input,
    webgpu::raii::TextureWithSampler& target_output,
    glm::uvec2 output_size)
{
    if (!m_pipeline)
        return;

    webgpu::raii::BindGroup bind_group(m_ctx->device(),
        m_ctx->resource_registry().bind_group_layout("tile_debug_overlay"),
        std::vector<WGPUBindGroupEntry> {
            overlay_view.create_bind_group_entry(0),
            m_settings_uniform->raw_buffer().create_bind_group_entry(1),
            target_output.texture_view().create_bind_group_entry(2),
            current_input.texture_view().create_bind_group_entry(3),
        },
        "tile debug overlay bind group");

    WGPUComputePassDescriptor compute_pass_desc {};
    compute_pass_desc.label = WGPUStringView { .data = "tile debug compute pass", .length = WGPU_STRLEN };
    webgpu::raii::ComputePassEncoder compute_pass(command_encoder, compute_pass_desc);

    wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, bind_group.handle(), 0, nullptr);

    const glm::uvec3 workgroup_counts = glm::ceil(glm::vec3(float(output_size.x), float(output_size.y), 1.0f) / glm::vec3(16.0f, 16.0f, 1.0f));
    m_pipeline->run(compute_pass, workgroup_counts);
}

} // namespace webgpu_engine
