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

#include "FxaaNode.h"

namespace webgpu_engine::compute::nodes {

glm::uvec3 FxaaNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

FxaaNode::FxaaNode(const PipelineManager& pipeline_manager, WGPUDevice device)
    : FxaaNode(pipeline_manager, device, FxaaSettings())
{
}

FxaaNode::FxaaNode(const PipelineManager& pipeline_manager, WGPUDevice device, const FxaaSettings& settings)
    : Node(
          {
              InputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>()),
          },
          {
              OutputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
          })
    , m_pipeline_manager { &pipeline_manager }
    , m_device { device }
    , m_queue(wgpuDeviceGetQueue(m_device))
    , m_settings { settings }
    , m_input_sampler { create_input_sampler(m_device) }
{
}

void FxaaNode::run_impl()
{
    qDebug() << "running FxaaNode ...";

    assert(input_socket("texture").is_socket_connected()); // run_impl should only be called, if socket is connected, otherwise bug

    const auto& input_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("texture").get_connected_data());

    m_output_texture = create_output_texture(
        m_device, uint32_t(input_texture.texture().width()), uint32_t(input_texture.texture().height()), m_settings.format, m_settings.usage);

    // create bind group
    std::vector<WGPUBindGroupEntry> entries {
        input_texture.texture_view().create_bind_group_entry(0),
        m_input_sampler->create_bind_group_entry(1),
        m_output_texture->texture_view().create_bind_group_entry(2),
    };
    webgpu::raii::BindGroup bind_group(m_device, m_pipeline_manager->fxaa_compute_bind_group_layout(), entries, "fxaa bind group");

    // bind GPU resources and run pipeline
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "fxaa command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_device, descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "fxaa compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts
                = glm::ceil(glm::vec3(m_output_texture->texture().width(), m_output_texture->texture().height(), 1) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, bind_group.handle(), 0, nullptr);
            m_pipeline_manager->fxaa_compute_pipeline().run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "fxaa command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              FxaaNode* _this = reinterpret_cast<FxaaNode*>(userdata);
              emit _this->run_completed(); // emits signal run_finished()
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

std::unique_ptr<webgpu::raii::TextureWithSampler> FxaaNode::create_output_texture(
    WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage)
{
    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "fxaa texture output texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = format;
    texture_desc.usage = usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "fxaa texture output sampler", .length = WGPU_STRLEN };
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

std::unique_ptr<webgpu::raii::Sampler> FxaaNode::create_input_sampler(WGPUDevice device)
{
    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "fxaa input sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;
    return std::make_unique<webgpu::raii::Sampler>(device, sampler_desc);
}

} // namespace webgpu_engine::compute::nodes
