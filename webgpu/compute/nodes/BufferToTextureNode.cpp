/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "BufferToTextureNode.h"
#include "util.h"
#include "webgpu/base/raii/Texture.h"
#include <webgpu/base/gpu_utils.h>

namespace webgpu_compute::nodes {

glm::uvec3 BufferToTextureNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

const uint32_t BufferToTextureNode::MAX_TEXTURE_RESOLUTION = 8192;

BufferToTextureNode::BufferToTextureNode(webgpu::Context& ctx)
    : BufferToTextureNode(ctx, BufferToTextureSettings())
{
}

BufferToTextureNode::BufferToTextureNode(webgpu::Context& ctx, const BufferToTextureSettings& settings)
    : Node({ InputSocket(*this, "raster dimensions", data_type<glm::uvec2>()),
               InputSocket(*this, "storage buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>()),
               InputSocket(*this, "transparency buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>()) },
          {
              OutputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_textures[m_pingpong].get(); }),
          })
    , m_ctx(&ctx)
    , m_settings { settings }
    , m_settings_uniform(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform)
{
    auto& reg = ctx.resource_registry();
    reg.register_shader("buffer_to_texture_compute", "webgpu_compute::buffer_to_texture_compute");
    reg.register_bind_group_layout("buffer_to_texture_compute", [](WGPUDevice dev) {
        WGPUBindGroupLayoutEntry e0 {};
        e0.binding = 0;
        e0.visibility = WGPUShaderStage_Compute;
        e0.buffer.type = WGPUBufferBindingType_Uniform;

        WGPUBindGroupLayoutEntry e1 {};
        e1.binding = 1;
        e1.visibility = WGPUShaderStage_Compute;
        e1.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

        WGPUBindGroupLayoutEntry e2 {};
        e2.binding = 2;
        e2.visibility = WGPUShaderStage_Compute;
        e2.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

        WGPUBindGroupLayoutEntry e5 {};
        e5.binding = 5;
        e5.visibility = WGPUShaderStage_Compute;
        e5.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
        e5.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
        e5.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

        return std::make_unique<webgpu::raii::BindGroupLayout>(
            dev, std::vector<WGPUBindGroupLayoutEntry> { e0, e1, e2, e5 }, "buffer to texture compute bind group layout");
    });
    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        m_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(device,
            reg.shader("buffer_to_texture_compute"),
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("buffer_to_texture_compute") });
    });
}

void BufferToTextureNode::run_impl()
{

    const auto input_raster_dimensions = std::get<data_type<glm::uvec2>()>(input_socket("raster dimensions").get_connected_data());
    const auto& input_storage_buffer = *std::get<data_type<webgpu::raii::RawBuffer<uint32_t>*>()>(input_socket("storage buffer").get_connected_data());
    const auto& input_transparency_buffer
        = *std::get<data_type<webgpu::raii::RawBuffer<uint32_t>*>()>(input_socket("transparency buffer").get_connected_data());

    m_settings_uniform.data.input_resolution = input_raster_dimensions;
    update_gpu_settings();

    // assert input textures have same size, otherwise fail run
    if (input_raster_dimensions.x > MAX_TEXTURE_RESOLUTION || input_raster_dimensions.y > MAX_TEXTURE_RESOLUTION) {
        fail_run("cannot create texture: texture dimensions (" + std::to_string(input_raster_dimensions.x) + "x"
            + std::to_string(input_raster_dimensions.y) + ") exceed " + std::to_string(MAX_TEXTURE_RESOLUTION));
        return;
    }

    m_pingpong = 1 - m_pingpong; // write the other slot; the previously produced texture stays alive for rendering
    m_output_textures[m_pingpong] = create_texture(m_ctx->device(), input_raster_dimensions.x, input_raster_dimensions.y, m_settings);
    // create bind group
    std::vector<WGPUBindGroupEntry> entries {
        m_settings_uniform.raw_buffer().create_bind_group_entry(0),
        input_storage_buffer.create_bind_group_entry(1),
        input_transparency_buffer.create_bind_group_entry(2),
        m_output_views[m_pingpong]->create_bind_group_entry(5),
    };
    webgpu::raii::BindGroup compute_bind_group(
        m_ctx->device(), m_ctx->resource_registry().bind_group_layout("buffer_to_texture_compute"), entries, "buffer to texture compute bind group");
    // bind GPU resources and run pipeline
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "buffer to texture compute command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_ctx->device(), descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "buffer to texture compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts = glm::ceil(glm::vec3(input_raster_dimensions.x, input_raster_dimensions.y, 1) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline->run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "buffer to texture compute command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_ctx->queue(), 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              auto* node = reinterpret_cast<BufferToTextureNode*>(userdata);
              if (node->m_settings.create_mipmaps) {
                  const auto on_mipmaps_done = []([[maybe_unused]] WGPUQueueWorkDoneStatus status,
                                                   [[maybe_unused]] WGPUStringView message,
                                                   void* userdata,
                                                   [[maybe_unused]] void* userdata2) { reinterpret_cast<BufferToTextureNode*>(userdata)->complete_run(); };
                  webgpu::compute_mipmaps_for_texture(*node->m_ctx,
                      &node->m_output_textures[node->m_pingpong]->texture(),
                      WGPUQueueWorkDoneCallbackInfo {
                          .nextInChain = nullptr,
                          .mode = WGPUCallbackMode_AllowProcessEvents,
                          .callback = on_mipmaps_done,
                          .userdata1 = node,
                          .userdata2 = nullptr,
                      });
              } else {
                  node->complete_run();
              }
          };

    wgpuQueueOnSubmittedWorkDone(m_ctx->queue(),
        WGPUQueueWorkDoneCallbackInfo {
            .nextInChain = nullptr,
            .mode = WGPUCallbackMode_AllowProcessEvents,
            .callback = on_work_done,
            .userdata1 = this,
            .userdata2 = nullptr,
        });
}

void BufferToTextureNode::update_gpu_settings()
{
    m_settings_uniform.data.color_map_bounds = m_settings.color_map_bounds;
    m_settings_uniform.data.transparency_map_bounds = m_settings.transparency_map_bounds;
    m_settings_uniform.data.use_bin_interpolation = m_settings.use_bin_interpolation;
    m_settings_uniform.data.use_transparency_buffer = m_settings.use_transparency_buffer;
    m_settings_uniform.update_gpu_data(m_ctx->queue());
}

std::unique_ptr<webgpu::raii::TextureWithSampler> BufferToTextureNode::create_texture(
    WGPUDevice device, uint32_t width, uint32_t height, BufferToTextureSettings& settings)
{
    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "buffer to texture output texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = settings.create_mipmaps ? webgpu::raii::Texture::max_mip_level_count(glm::uvec2(width, height)) : 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = settings.texture_format;
    texture_desc.usage = settings.texture_usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "buffer to texture sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = settings.texture_filter_mode;
    sampler_desc.minFilter = settings.texture_filter_mode;
    sampler_desc.mipmapFilter = settings.texture_mipmap_filter_mode;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = float(texture_desc.mipLevelCount);
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = settings.texture_max_aniostropy;

    auto texture_with_sampler = std::make_unique<webgpu::raii::TextureWithSampler>(device, texture_desc, sampler_desc);

    WGPUTextureViewDescriptor desc = texture_with_sampler->texture().default_texture_view_descriptor();
    desc.mipLevelCount = 1;
    m_output_views[m_pingpong] = texture_with_sampler->texture().create_view(desc);

    return texture_with_sampler;
}

void BufferToTextureNode::serialize_settings(QJsonObject& out) const
{
    out["texture_format"] = wgpu_format_to_string(m_settings.texture_format);
    out["texture_usage"] = wgpu_usage_to_json(m_settings.texture_usage);
    out["texture_filter_mode"] = wgpu_filter_mode_to_string(m_settings.texture_filter_mode);
    out["texture_mipmap_filter_mode"] = wgpu_mipmap_filter_mode_to_string(m_settings.texture_mipmap_filter_mode);
    out["texture_max_aniostropy"] = static_cast<int>(m_settings.texture_max_aniostropy);
    out["create_mipmaps"] = m_settings.create_mipmaps;
    out["color_map_bounds"] = vec2_to_json(m_settings.color_map_bounds);
    out["transparency_map_bounds"] = vec2_to_json(m_settings.transparency_map_bounds);
    out["use_bin_interpolation"] = m_settings.use_bin_interpolation;
    out["use_transparency_buffer"] = m_settings.use_transparency_buffer;
}

void BufferToTextureNode::deserialize_settings(const QJsonObject& in)
{
    if (in.contains("texture_format"))
        m_settings.texture_format = wgpu_format_from_string(in["texture_format"].toString(), m_settings.texture_format);
    if (in.contains("texture_usage"))
        m_settings.texture_usage = wgpu_usage_from_json(in["texture_usage"].toArray(), m_settings.texture_usage);
    if (in.contains("texture_filter_mode"))
        m_settings.texture_filter_mode = wgpu_filter_mode_from_string(in["texture_filter_mode"].toString(), m_settings.texture_filter_mode);
    if (in.contains("texture_mipmap_filter_mode"))
        m_settings.texture_mipmap_filter_mode
            = wgpu_mipmap_filter_mode_from_string(in["texture_mipmap_filter_mode"].toString(), m_settings.texture_mipmap_filter_mode);
    if (in.contains("texture_max_aniostropy"))
        m_settings.texture_max_aniostropy = static_cast<uint16_t>(in["texture_max_aniostropy"].toInt(m_settings.texture_max_aniostropy));
    if (in.contains("create_mipmaps"))
        m_settings.create_mipmaps = in["create_mipmaps"].toBool(m_settings.create_mipmaps);
    if (in.contains("color_map_bounds"))
        m_settings.color_map_bounds = vec2_from_json(in["color_map_bounds"].toArray(), m_settings.color_map_bounds);
    if (in.contains("transparency_map_bounds"))
        m_settings.transparency_map_bounds = vec2_from_json(in["transparency_map_bounds"].toArray(), m_settings.transparency_map_bounds);
    if (in.contains("use_bin_interpolation"))
        m_settings.use_bin_interpolation = in["use_bin_interpolation"].toBool(m_settings.use_bin_interpolation);
    if (in.contains("use_transparency_buffer"))
        m_settings.use_transparency_buffer = in["use_transparency_buffer"].toBool(m_settings.use_transparency_buffer);
}

} // namespace webgpu_compute::nodes
