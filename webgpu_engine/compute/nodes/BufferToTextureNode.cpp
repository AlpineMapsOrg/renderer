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

namespace webgpu_engine::compute::nodes {

glm::uvec3 BufferToTextureNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

const uint32_t BufferToTextureNode::MAX_TEXTURE_RESOLUTION = 8192;

BufferToTextureNode::BufferToTextureNode(const PipelineManager& pipeline_manager, WGPUDevice device)
    : BufferToTextureNode(pipeline_manager, device, BufferToTextureSettings())
{
}

BufferToTextureNode::BufferToTextureNode(const PipelineManager& pipeline_manager, WGPUDevice device, const BufferToTextureSettings& settings)
    : Node({ InputSocket(*this, "raster dimensions", data_type<glm::uvec2>()),
               InputSocket(*this, "storage buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>()),
               InputSocket(*this, "transparency buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>()) },
          {
              OutputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
          })
    , m_pipeline_manager { &pipeline_manager }
    , m_device { device }
    , m_queue { wgpuDeviceGetQueue(device) }
    , m_settings { settings }
    , m_settings_uniform(device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform)
{
}

void BufferToTextureNode::run_impl()
{
    qDebug() << "running BufferToTextureNode ...";
    const auto input_raster_dimensions = std::get<data_type<glm::uvec2>()>(input_socket("raster dimensions").get_connected_data());
    const auto& input_storage_buffer = *std::get<data_type<webgpu::raii::RawBuffer<uint32_t>*>()>(input_socket("storage buffer").get_connected_data());
    const auto& input_transparency_buffer = *std::get<data_type<webgpu::raii::RawBuffer<uint32_t>*>()>(input_socket("transparency buffer").get_connected_data());

    m_settings_uniform.data.input_resolution = input_raster_dimensions;
    update_gpu_settings();

    // assert input textures have same size, otherwise fail run
    if (input_raster_dimensions.x > MAX_TEXTURE_RESOLUTION || input_raster_dimensions.y > MAX_TEXTURE_RESOLUTION) {
        emit run_failed(NodeRunFailureInfo(*this,
            std::format(
                "cannot create texture: texture dimensions ({}x{}) exceed {}", input_raster_dimensions.x, input_raster_dimensions.y, MAX_TEXTURE_RESOLUTION)));
        return;
    }

    m_output_texture = create_texture(m_device, input_raster_dimensions.x, input_raster_dimensions.y, m_settings);

    // create bind group
    std::vector<WGPUBindGroupEntry> entries {
        m_settings_uniform.raw_buffer().create_bind_group_entry(0),
        input_storage_buffer.create_bind_group_entry(1),
        input_transparency_buffer.create_bind_group_entry(2),
        m_output_view->create_bind_group_entry(5),
    };
    webgpu::raii::BindGroup compute_bind_group(
        m_device, m_pipeline_manager->buffer_to_texture_bind_group_layout(), entries, "buffer to texture compute bind group");

    // bind GPU resources and run pipeline
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "buffer to texture compute command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_device, descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "buffer to texture compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts = glm::ceil(glm::vec3(input_raster_dimensions.x, input_raster_dimensions.y, 1) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline_manager->buffer_to_texture_compute_pipeline().run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "buffer to texture compute command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              BufferToTextureNode* _this = reinterpret_cast<BufferToTextureNode*>(userdata);
              emit _this->run_completed();
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = this,
        .userdata2 = nullptr,
    };

    wgpuQueueOnSubmittedWorkDone(m_queue, callback_info);
}

void BufferToTextureNode::update_gpu_settings()
{
    m_settings_uniform.data.color_map_bounds = m_settings.color_map_bounds;
    m_settings_uniform.data.transparency_map_bounds = m_settings.transparency_map_bounds;
    m_settings_uniform.data.use_bin_interpolation = m_settings.use_bin_interpolation;
    m_settings_uniform.data.use_transparency_buffer = m_settings.use_transparency_buffer;
    m_settings_uniform.update_gpu_data(m_queue);
}

uint32_t bit_width(uint32_t m)
{
    if (m == 0)
        return 0;
    else {
        uint32_t w = 0;
        while (m >>= 1)
            ++w;
        return w;
    }
}

uint32_t getMaxMipLevelCount(const glm::uvec2 textureSize) { return std::max(1u, bit_width(std::max(textureSize.x, textureSize.y))); }

std::unique_ptr<webgpu::raii::TextureWithSampler> BufferToTextureNode::create_texture(WGPUDevice device, uint32_t width, uint32_t height, BufferToTextureSettings& settings)
{
    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "buffer to texture output texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = settings.create_mipmaps ? getMaxMipLevelCount(glm::uvec2(width, height)) : 1;
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
    m_output_view = texture_with_sampler->texture().create_view(desc);

    return texture_with_sampler;
}

} // namespace webgpu_engine::compute::nodes
