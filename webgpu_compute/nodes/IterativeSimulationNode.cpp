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

namespace webgpu_compute::nodes {

glm::uvec3 IterativeSimulationNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

IterativeSimulationNode::IterativeSimulationNode(webgpu::Context& ctx)
    : IterativeSimulationNode(ctx, IterativeSimulationSettings())
{
}

IterativeSimulationNode::IterativeSimulationNode(webgpu::Context& ctx, const IterativeSimulationSettings& settings)
    : Node(
          {
              InputSocket(*this, "height texture", data_type<const webgpu::raii::TextureWithSampler*>()),
              InputSocket(*this, "release point texture", data_type<const webgpu::raii::TextureWithSampler*>()),
          },
          {
              OutputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
          })
    , m_ctx(&ctx)
    , m_settings { settings }
{
    auto& reg = ctx.resource_registry();
    reg.register_shader("iterative_simulation_compute", "webgpu_compute::iterative_simulation_compute");
    reg.register_bind_group_layout("iterative_simulation_compute", [](WGPUDevice dev) {
        WGPUBindGroupLayoutEntry e0 {};
        e0.binding = 0;
        e0.visibility = WGPUShaderStage_Compute;
        e0.buffer.type = WGPUBufferBindingType_Uniform;

        WGPUBindGroupLayoutEntry e1 {};
        e1.binding = 1;
        e1.visibility = WGPUShaderStage_Compute;
        e1.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        e1.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry e2 {};
        e2.binding = 2;
        e2.visibility = WGPUShaderStage_Compute;
        e2.texture.sampleType = WGPUTextureSampleType_Float;
        e2.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry e3 {};
        e3.binding = 3;
        e3.visibility = WGPUShaderStage_Compute;
        e3.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

        WGPUBindGroupLayoutEntry e4 {};
        e4.binding = 4;
        e4.visibility = WGPUShaderStage_Compute;
        e4.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutEntry e5 {};
        e5.binding = 5;
        e5.visibility = WGPUShaderStage_Compute;
        e5.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutEntry e6 {};
        e6.binding = 6;
        e6.visibility = WGPUShaderStage_Compute;
        e6.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
        e6.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
        e6.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

        return std::make_unique<webgpu::raii::BindGroupLayout>(
            dev, std::vector<WGPUBindGroupLayoutEntry> { e0, e1, e2, e3, e4, e5, e6 }, "iterative simulation bind group layout");
    });
    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        m_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(device,
            reg.shader("iterative_simulation_compute"),
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("iterative_simulation_compute") });
    });
}

void IterativeSimulationNode::run_impl()
{

    const auto& input_height_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("height texture").get_connected_data());
    const auto& input_release_point_texture
        = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("release point texture").get_connected_data());

    m_output_texture = create_texture(m_ctx->device(),
        uint32_t(input_height_texture.texture().width()),
        uint32_t(input_height_texture.texture().height()),
        WGPUTextureFormat_RGBA8Unorm,
        WGPUTextureUsage(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding));

    m_settings_uniform
        = std::make_unique<webgpu::Buffer<IterativeSimulationSettingsUniform>>(m_ctx->device(), WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);

    size_t buffer_size = input_height_texture.texture().width() * input_height_texture.texture().height();
    m_input_parent_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, buffer_size, "flow py parents buffer 1");
    m_flux_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, buffer_size, "flow py flow buffer 1");
    m_output_parent_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, buffer_size, "flow py parents buffer 2");

    // create bind group
    webgpu::raii::BindGroup compute_bind_group(m_ctx->device(),
        m_ctx->resource_registry().bind_group_layout("iterative_simulation_compute"),
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
    m_settings_uniform->update_gpu_data(m_ctx->queue());

    m_flux_buffer->clear(m_ctx->device(), m_ctx->queue());

    glm::uvec3 workgroup_counts
        = glm::ceil(glm::vec3(input_height_texture.texture().width(), input_height_texture.texture().height(), 1) / glm::vec3(SHADER_WORKGROUP_SIZE));

    for (uint32_t i = 0; i < m_settings.max_num_iterations; i++) {
        qDebug() << "iteration" << i;
        m_settings_uniform->data.num_iteration = i;
        m_settings_uniform->update_gpu_data(m_ctx->queue());

        // m_input_parent_buffer->clear(encoder.handle());
        // m_output_parent_buffer->clear(encoder.handle());

        // bind GPU resources and run pipeline
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "flowpy compute command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_ctx->device(), descriptor);
        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "flowpy compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline->run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "flowpy compute command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_ctx->queue(), 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              IterativeSimulationNode* _this = reinterpret_cast<IterativeSimulationNode*>(userdata);
              _this->complete_run();
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = this,
        .userdata2 = nullptr,
    };

    wgpuQueueOnSubmittedWorkDone(m_ctx->queue(), callback_info);
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

} // namespace webgpu_compute::nodes
