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

#include "IterativeSimulationNode.h"

namespace webgpu_engine::compute::nodes {

glm::uvec3 IterativeSimulationNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

IterativeSimulationNode::IterativeSimulationNode(const PipelineManager& pipeline_manager, WGPUDevice device)
    : IterativeSimulationNode(pipeline_manager, device, IterativeSimulationSettings())
{
}

IterativeSimulationNode::IterativeSimulationNode(const PipelineManager& pipeline_manager, WGPUDevice device, const IterativeSimulationSettings& settings)
    : Node(
          {
              InputSocket(*this, "height texture", data_type<const webgpu::raii::TextureWithSampler*>()),
              InputSocket(*this, "release point texture", data_type<const webgpu::raii::TextureWithSampler*>()),
          },
          {
              OutputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
          })
    , m_pipeline_manager { &pipeline_manager }
    , m_device { device }
    , m_queue { wgpuDeviceGetQueue(device) }
    , m_settings { settings }
{
}

void IterativeSimulationNode::run_impl()
{
    qDebug() << "running IterativeAvalancheNode ...";
    const auto& input_height_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("height texture").get_connected_data());
    const auto& input_release_point_texture
        = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("release point texture").get_connected_data());

    m_output_texture = create_texture(m_device,
        uint32_t(input_height_texture.texture().width()),
        uint32_t(input_height_texture.texture().height()),
        WGPUTextureFormat_RGBA8Unorm,
        WGPUTextureUsage(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding));

    m_settings_uniform = std::make_unique<Buffer<IterativeSimulationSettingsUniform>>(m_device, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);

    size_t buffer_size = input_height_texture.texture().width() * input_height_texture.texture().height();
    m_input_parent_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, buffer_size, "flow py parents buffer 1");
    m_flux_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, buffer_size, "flow py flow buffer 1");
    m_output_parent_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, buffer_size, "flow py parents buffer 2");

    // create bind group
    webgpu::raii::BindGroup compute_bind_group(m_device, m_pipeline_manager->iterative_simulation_compute_bind_group_layout(),
        std::vector<WGPUBindGroupEntry> {
            m_settings_uniform->raw_buffer().create_bind_group_entry(0),
            input_height_texture.texture_view().create_bind_group_entry(1),
            input_release_point_texture.texture_view().create_bind_group_entry(2),
            m_input_parent_buffer->create_bind_group_entry(3),
            m_flux_buffer->create_bind_group_entry(4),
            m_output_parent_buffer->create_bind_group_entry(5),
            m_output_texture->texture_view().create_bind_group_entry(6),
        },
        "flowpy compute bind group");

    m_settings_uniform->data.num_iteration = 0;
    m_settings_uniform->update_gpu_data(m_queue);

    m_flux_buffer->clear(m_device, m_queue);

    glm::uvec3 workgroup_counts
        = glm::ceil(glm::vec3(input_height_texture.texture().width(), input_height_texture.texture().height(), 1) / glm::vec3(SHADER_WORKGROUP_SIZE));

    for (uint32_t i = 0; i < m_settings.max_num_iterations; i++) {
        qDebug() << "iteration" << i;
        m_settings_uniform->data.num_iteration = i;
        m_settings_uniform->update_gpu_data(m_queue);

        // m_input_parent_buffer->clear(encoder.handle());
        // m_output_parent_buffer->clear(encoder.handle());

        // bind GPU resources and run pipeline
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "flowpy compute command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_device, descriptor);
        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "flowpy compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline_manager->iterative_simulation_compute_pipeline().run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "flowpy compute command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              IterativeSimulationNode* _this = reinterpret_cast<IterativeSimulationNode*>(userdata);
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

std::unique_ptr<webgpu::raii::TextureWithSampler> IterativeSimulationNode::create_texture(
    WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage)
{
    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "iterative avalanche node texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = format;
    texture_desc.usage = usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "iterative avalanche node sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    return std::make_unique<webgpu::raii::TextureWithSampler>(device, texture_desc, sampler_desc);
}

} // namespace webgpu_engine::compute::nodes
