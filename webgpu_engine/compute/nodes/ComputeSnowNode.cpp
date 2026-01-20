/*****************************************************************************
 * weBIGeo
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

#include "ComputeSnowNode.h"

#include <QDebug>
#include <QString>

namespace webgpu_engine::compute::nodes {

glm::uvec3 ComputeSnowNode::SHADER_WORKGROUP_SIZE = { 1, 16, 16 };

ComputeSnowNode::ComputeSnowNode(const PipelineManager& pipeline_manager, WGPUDevice device)
    : ComputeSnowNode(pipeline_manager, device, SnowSettings())
{
}

ComputeSnowNode::ComputeSnowNode(const PipelineManager& pipeline_manager, WGPUDevice device, const SnowSettings& settings)
    : Node(
          {
              InputSocket(*this, "bounds", data_type<const radix::geometry::Aabb<2, double>*>()),
              InputSocket(*this, "normal texture", data_type<const webgpu::raii::TextureWithSampler*>()),
              InputSocket(*this, "height texture", data_type<const webgpu::raii::TextureWithSampler*>()),
          },
          {
              OutputSocket(*this, "snow texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_snow_texture.get(); }),
          })
    , m_pipeline_manager { &pipeline_manager }
    , m_device { device }
    , m_queue(wgpuDeviceGetQueue(m_device))
    , m_settings { settings }
    , m_snow_settings_uniform_buffer(device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform)
    , m_region_bounds_uniform_buffer(device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform)
{
}

void ComputeSnowNode::run_impl()
{
    qDebug() << "running SnowComputeNode ...";

    // read input data from input sockets
    const auto& bounds = *std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("bounds").get_connected_data());
    const auto& heights_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("height texture").get_connected_data());
    const auto& normals_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("normal texture").get_connected_data());

    // create output texture
    m_output_snow_texture = create_snow_texture(
        m_device, uint32_t(heights_texture.texture().width()), uint32_t(heights_texture.texture().height()), m_settings.format, m_settings.usage);

    // update uniform buffer
    m_snow_settings_uniform_buffer.data.angle.x = 1.0f; // always enabled, does not matter for compute
    m_snow_settings_uniform_buffer.data.angle.y = m_settings.min_angle;
    m_snow_settings_uniform_buffer.data.angle.z = m_settings.max_angle;
    m_snow_settings_uniform_buffer.data.angle.w = m_settings.angle_blend;
    m_snow_settings_uniform_buffer.data.alt.x = m_settings.min_altitude;
    m_snow_settings_uniform_buffer.data.alt.y = m_settings.altitude_variation;
    m_snow_settings_uniform_buffer.data.alt.z = m_settings.altitude_blend;
    m_snow_settings_uniform_buffer.data.alt.w = 0.0f; // specular, does not matter for compute
    m_snow_settings_uniform_buffer.update_gpu_data(m_queue);

    m_region_bounds_uniform_buffer.data.aabb_min = glm::fvec2(bounds.min);
    m_region_bounds_uniform_buffer.data.aabb_max = glm::fvec2(bounds.max);
    m_region_bounds_uniform_buffer.update_gpu_data(m_queue);

    // create bind group
    // TODO re-create bind groups only when input handles change
    // TODO adapter shader code
    // TODO compute bounds in other node!
    std::vector<WGPUBindGroupEntry> entries {
        m_snow_settings_uniform_buffer.raw_buffer().create_bind_group_entry(0),
        m_region_bounds_uniform_buffer.raw_buffer().create_bind_group_entry(1),
        normals_texture.texture_view().create_bind_group_entry(2),
        heights_texture.texture_view().create_bind_group_entry(3),
        m_output_snow_texture->texture_view().create_bind_group_entry(4),
    };
    webgpu::raii::BindGroup compute_bind_group(m_device, m_pipeline_manager->snow_compute_bind_group_layout(), entries, "snow compute bind group");

    // bind GPU resources and run pipeline
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "snow compute command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_device, descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "snow compute compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts = glm::ceil(
                glm::vec3(m_output_snow_texture->texture().width(), m_output_snow_texture->texture().height(), 1) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline_manager->snow_compute_pipeline().run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "SnowComputeNode command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              ComputeSnowNode* _this = reinterpret_cast<ComputeSnowNode*>(userdata);
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

std::unique_ptr<webgpu::raii::TextureWithSampler> ComputeSnowNode::create_snow_texture(
    WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage)
{
    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "snow storage texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = format;
    texture_desc.usage = usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "snow sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Linear;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    return std::make_unique<webgpu::raii::TextureWithSampler>(device, texture_desc, sampler_desc);
}

} // namespace webgpu_engine::compute::nodes
