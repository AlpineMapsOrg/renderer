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

#include "TextureOverlay.h"

#include "webgpu_engine/Context.h"
#include <nucleus/utils/geopng_decoder.h>
#include <nucleus/utils/image_loader.h>
#include <optional>
#include <webgpu/RenderResourceRegistry.h>
#include <webgpu/gpu_utils.h>
#include <webgpu/raii/BindGroup.h>
#include <webgpu/raii/BindGroupLayout.h>
#include <webgpu/raii/RenderPassEncoder.h>
#include <webgpu/raii/Texture.h>
#include <webgpu/raii/base_types.h>

namespace webgpu_engine {

TextureOverlay::TextureOverlay()
    : Overlay()
{
}

void TextureOverlay::load_image(const QString& path)
{
    assert(m_is_ready && "load_image must be called after ready()");
    m_linked_texture = nullptr; // owned source takes over
    const auto image = nucleus::utils::image_loader::rgba8(path).value();
    create_texture(*m_ctx, uint32_t(image.width()), uint32_t(image.height()));
    m_overlay_texture->texture().write(m_ctx->queue(), image);
    if (settings.use_mipmaps)
        webgpu::compute_mipmaps_for_texture(*m_ctx, &m_overlay_texture->texture());
}

void TextureOverlay::link_texture(const webgpu::raii::TextureWithSampler* texture) { m_linked_texture = texture; }

void TextureOverlay::load_texture(const webgpu::raii::TextureWithSampler& source)
{
    assert(m_is_ready && "load_texture must be called after ready()");
    assert(source.texture().descriptor().format == WGPUTextureFormat_RGBA8Unorm && "load_texture requires an RGBA8Unorm source texture");

    m_linked_texture = nullptr; // owned source takes over
    create_texture(*m_ctx, uint32_t(source.texture().width()), uint32_t(source.texture().height()));

    // GPU -> GPU copy of mip 0 into our own texture
    WGPUCommandEncoderDescriptor encoder_desc {};
    webgpu::raii::CommandEncoder encoder(m_ctx->device(), encoder_desc);
    source.texture().copy_to_texture(encoder.handle(), 0, m_overlay_texture->texture(), 0);
    WGPUCommandBufferDescriptor cmd_desc {};
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_desc);
    wgpuQueueSubmit(m_ctx->queue(), 1, &command);
    wgpuCommandBufferRelease(command);

    if (settings.use_mipmaps)
        webgpu::compute_mipmaps_for_texture(*m_ctx, &m_overlay_texture->texture());
}

void TextureOverlay::init(Context& context)
{
    webgpu::Context& ctx = context.webgpu_ctx();
    m_ctx = &ctx;

    auto& reg = ctx.resource_registry();
    if (!reg.has_shader("texture_overlay_render"))
        reg.register_shader("texture_overlay_render", "webgpu_engine::overlays/texture_overlay");
    if (!reg.has_bind_group_layout("texture_overlay"))
        reg.register_bind_group_layout("texture_overlay", [](WGPUDevice device) {
            WGPUBindGroupLayoutEntry position_entry {};
            position_entry.binding = 0;
            position_entry.visibility = WGPUShaderStage_Fragment;
            position_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
            position_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

            WGPUBindGroupLayoutEntry settings_entry {};
            settings_entry.binding = 1;
            settings_entry.visibility = WGPUShaderStage_Fragment;
            settings_entry.buffer.type = WGPUBufferBindingType_Uniform;

            WGPUBindGroupLayoutEntry overlay_texture_entry {};
            overlay_texture_entry.binding = 2;
            overlay_texture_entry.visibility = WGPUShaderStage_Fragment;
            overlay_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
            overlay_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

            WGPUBindGroupLayoutEntry overlay_sampler_entry {};
            overlay_sampler_entry.binding = 3;
            overlay_sampler_entry.visibility = WGPUShaderStage_Fragment;
            overlay_sampler_entry.sampler.type = WGPUSamplerBindingType_Filtering;

            WGPUBindGroupLayoutEntry background_entry {};
            background_entry.binding = 4;
            background_entry.visibility = WGPUShaderStage_Fragment;
            background_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
            background_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

            return std::make_unique<webgpu::raii::BindGroupLayout>(device,
                std::vector<WGPUBindGroupLayoutEntry> { position_entry, settings_entry, overlay_texture_entry, overlay_sampler_entry, background_entry },
                "texture overlay bind group layout");
        });
    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        webgpu::FramebufferFormat format {};
        format.depth_format = WGPUTextureFormat_Undefined;
        format.color_formats = { WGPUTextureFormat_RGBA8Unorm };

        m_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(device,
            reg.shader("texture_overlay_render"),
            reg.shader("texture_overlay_render"),
            std::vector<webgpu::util::SingleVertexBufferInfo> {},
            format,
            std::vector<const webgpu::raii::BindGroupLayout*> {
                &reg.bind_group_layout("shared_config"),
                &reg.bind_group_layout("camera"),
                &reg.bind_group_layout("texture_overlay"),
            });
    });

    m_settings_uniform = std::make_unique<webgpu::Buffer<GpuSettings>>(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    update_gpu_settings();
    create_texture(context.webgpu_ctx(), 1, 1);
}

void TextureOverlay::ready(webgpu::Context& ctx) { m_is_ready = true; }

void TextureOverlay::create_texture(webgpu::Context& ctx, uint32_t width, uint32_t height)
{
    const bool mipmaps = settings.use_mipmaps;
    const auto mip_levels = mipmaps ? webgpu::raii::Texture::max_mip_level_count(glm::uvec2(width, height)) : 1u;

    const auto filter = settings.filter_mode == FilterMode::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
    const auto mip_filter = (mipmaps && settings.filter_mode == FilterMode::Linear) ? WGPUMipmapFilterMode_Linear : WGPUMipmapFilterMode_Nearest;

    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "texture overlay input texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = mip_levels;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat_RGBA8Unorm;
    // CopyDst: write()/copy_to_texture() destination; StorageBinding: mipmap compute shader.
    texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_StorageBinding;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "texture overlay sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = filter;
    sampler_desc.minFilter = filter;
    sampler_desc.mipmapFilter = mip_filter;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = float(mip_levels);
    sampler_desc.compare = WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    m_overlay_texture = std::make_unique<webgpu::raii::TextureWithSampler>(ctx.device(), texture_desc, sampler_desc);
}

void TextureOverlay::update_gpu_settings()
{
    m_settings_uniform->data.aabb_min = glm::vec2(settings.aabb.min);
    m_settings_uniform->data.aabb_size = glm::vec2(settings.aabb.size());
    m_settings_uniform->data.opacity = settings.opacity;
    m_settings_uniform->data.mode = static_cast<uint32_t>(settings.mode);
    m_settings_uniform->data.float_decode_range = settings.float_decode_range;
    m_settings_uniform->data.encoded_float_range = glm::vec2(nucleus::utils::geopng::ENCODED_FLOAT_RANGE_MIN, nucleus::utils::geopng::ENCODED_FLOAT_RANGE_MAX);
    m_settings_uniform->update_gpu_data(m_ctx->queue());
}

void TextureOverlay::draw(const WGPUCommandEncoder& command_encoder,
    const webgpu::raii::TextureView& position_view,
    const webgpu::raii::TextureView& /*normal_view*/,
    const webgpu::raii::TextureView& /*overlay_view*/,
    const WGPUBindGroup& shared_config_bg,
    const WGPUBindGroup& camera_bg,
    const webgpu::raii::TextureWithSampler& current_input,
    webgpu::raii::TextureWithSampler& target_output,
    glm::uvec2 /*output_size*/)
{
    const webgpu::raii::TextureWithSampler* tex = m_linked_texture ? m_linked_texture : m_overlay_texture.get();
    assert(tex && m_pipeline);

    webgpu::raii::BindGroup bind_group(m_ctx->device(),
        m_ctx->resource_registry().bind_group_layout("texture_overlay"),
        std::vector<WGPUBindGroupEntry> {
            position_view.create_bind_group_entry(0),
            m_settings_uniform->raw_buffer().create_bind_group_entry(1),
            tex->texture_view().create_bind_group_entry(2),
            tex->sampler().create_bind_group_entry(3),
            current_input.texture_view().create_bind_group_entry(4),
        },
        "texture overlay bind group");

    // The fullscreen triangle writes every pixel, so loadOp doesnt matter (clear=fastest)
    WGPURenderPassColorAttachment color_attachment {};
    color_attachment.view = target_output.texture_view().handle();
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = { 0.0, 0.0, 0.0, 0.0 };

    WGPURenderPassDescriptor render_pass_desc {};
    render_pass_desc.label = WGPUStringView { .data = "texture overlay render pass", .length = WGPU_STRLEN };
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachment;

    webgpu::raii::RenderPassEncoder render_pass(command_encoder, render_pass_desc);

    wgpuRenderPassEncoderSetPipeline(render_pass.handle(), m_pipeline->pipeline().handle());
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 0, shared_config_bg, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 1, camera_bg, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 2, bind_group.handle(), 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass.handle(), 3, 1, 0, 0);
}

} // namespace webgpu_engine
