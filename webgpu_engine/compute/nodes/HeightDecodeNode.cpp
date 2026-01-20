/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "HeightDecodeNode.h"

#include <QDebug>

namespace webgpu_engine::compute::nodes {

glm::uvec3 HeightDecodeNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

webgpu_engine::compute::nodes::HeightDecodeNode::HeightDecodeNode(const PipelineManager& manager, WGPUDevice device, HeightDecodeSettings settings)
    : Node(
          {
              InputSocket(*this, "encoded texture", data_type<const webgpu::raii::TextureWithSampler*>()),
              InputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>()),
          },
          {
              OutputSocket(*this, "decoded texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
          })
    , m_pipeline_manager(&manager)
    , m_device(device)
    , m_queue(wgpuDeviceGetQueue(m_device))
    , m_settings(settings)
    , m_settings_uniform(m_device, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst)
{
}

void HeightDecodeNode::run_impl()
{
    qDebug() << "running HeightDecodeNode ...";

    const auto region_aabb = std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("region aabb").get_connected_data());
    const auto& input_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("encoded texture").get_connected_data());

    glm::uvec2 size = glm::uvec2(input_texture.texture().width(), input_texture.texture().height());

    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "decoded height texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { size.x, size.y, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat_R32Float;
    texture_desc.usage = m_settings.texture_usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "decoded height sampler", .length = WGPU_STRLEN };
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

    m_output_texture = std::make_unique<webgpu::raii::TextureWithSampler>(m_device, texture_desc, sampler_desc);

    // update bounding box
    m_settings_uniform.data.aabb_min = glm::uvec2(region_aabb->min);
    m_settings_uniform.data.aabb_max = glm::uvec2(region_aabb->max);
    m_settings_uniform.update_gpu_data(m_queue);

    // create bind group
    // TODO re-create bind groups only when input handles change
    std::vector<WGPUBindGroupEntry> entries {
        m_settings_uniform.raw_buffer().create_bind_group_entry(0),
        input_texture.texture_view().create_bind_group_entry(1),
        m_output_texture->texture_view().create_bind_group_entry(2),
    };
    webgpu::raii::BindGroup compute_bind_group(
        m_device, m_pipeline_manager->height_decode_compute_bind_group_layout(), entries, "compute controller bind group");

    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "compute controller command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_device, descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "compute controller compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts = glm::ceil(glm::vec3(size.x, size.y, 1) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline_manager->height_decode_compute_pipeline().run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "HeightDecode command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    /*const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              //auto* _this = static_cast<HeightDecodeNode*>(userdata);
              // auto& tex = _this->m_output_texture->texture();
              // tex.save_to_file(_this->m_device, "C:\\tmp\\asd2.png");
              //emit _this->run_completed();
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = this,
        .userdata2 = nullptr,
    };

    wgpuQueueOnSubmittedWorkDone(m_queue, callback_info);*/

    // NOTE: Maybe this needs to be inside onsubmittedworkdone callback? But technically
    // I don't think we should wait for the queue...
    emit run_completed();
}

} // namespace webgpu_engine::compute::nodes
