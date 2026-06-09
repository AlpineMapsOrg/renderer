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

#include "UpsampleTexturesNode.h"

namespace webgpu_engine::compute::nodes {

glm::uvec3 UpsampleTexturesNode::SHADER_WORKGROUP_SIZE = { 1, 16, 16 };

UpsampleTexturesNode::UpsampleTexturesNode(webgpu::Context& ctx, glm::uvec2 target_resolution, size_t capacity)
    : Node(
        {
            InputSocket(*this, "source textures", data_type<TileStorageTexture*>()),
        },
        {
            OutputSocket(*this, "output textures", data_type<TileStorageTexture*>(), [this]() { return m_output_storage_texture.get(); }),
        })
    , m_ctx(&ctx)
    , m_target_resolution { target_resolution }
    , m_input_indices(std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
          m_ctx->device(), WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc, capacity, "compute: upsampling texture, index buffer"))
    , m_output_storage_texture(std::make_unique<TileStorageTexture>(m_ctx->device(), m_target_resolution, capacity, WGPUTextureFormat_RGBA8Unorm))
{
    auto& reg = ctx.resource_registry();
    reg.register_shader("upsample_textures_compute", "compute/upsample_textures_compute.wgsl");
    reg.register_bind_group_layout("upsample_textures_compute", [](WGPUDevice dev) {
        WGPUBindGroupLayoutEntry e0 {};
        e0.binding = 0;
        e0.visibility = WGPUShaderStage_Compute;
        e0.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

        WGPUBindGroupLayoutEntry e1 {};
        e1.binding = 1;
        e1.visibility = WGPUShaderStage_Compute;
        e1.texture.sampleType = WGPUTextureSampleType_Float;
        e1.texture.viewDimension = WGPUTextureViewDimension_2DArray;

        WGPUBindGroupLayoutEntry e2 {};
        e2.binding = 2;
        e2.visibility = WGPUShaderStage_Compute;
        e2.sampler.type = WGPUSamplerBindingType_Filtering;

        WGPUBindGroupLayoutEntry e3 {};
        e3.binding = 3;
        e3.visibility = WGPUShaderStage_Compute;
        e3.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
        e3.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
        e3.storageTexture.viewDimension = WGPUTextureViewDimension_2DArray;

        return std::make_unique<webgpu::raii::BindGroupLayout>(
            dev, std::vector<WGPUBindGroupLayoutEntry> { e0, e1, e2, e3 }, "compute: upsample textures bind group layout");
    });
    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        m_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(device,
            reg.shader("upsample_textures_compute"),
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("upsample_textures_compute") });
    });
}

void UpsampleTexturesNode::run_impl()
{

    const auto& input_textures = *std::get<data_type<TileStorageTexture*>()>(input_socket("source textures").get_connected_data());
    const std::vector<uint32_t> input_used_indices = input_textures.used_layer_indices();

    qDebug() << "upsampling " << input_used_indices.size() << " textures from (" << input_textures.width() << "," << input_textures.height() << ") to ("
             << m_output_storage_texture->width() << "," << m_output_storage_texture->height() << ")";

    m_input_indices->write(m_ctx->queue(), input_used_indices.data(), input_used_indices.size());

    m_output_storage_texture->clear();

    // (re)create bind group
    // TODO re-create bind groups only when input handles change
    WGPUBindGroupEntry input_indices_entry = m_input_indices->create_bind_group_entry(0);
    WGPUBindGroupEntry input_textures_entry = input_textures.texture().texture_view().create_bind_group_entry(1);
    WGPUBindGroupEntry input_sampler = input_textures.texture().sampler().create_bind_group_entry(2);
    WGPUBindGroupEntry output_textures_entry = m_output_storage_texture->texture().texture_view().create_bind_group_entry(3);
    std::vector<WGPUBindGroupEntry> entries { input_indices_entry, input_textures_entry, input_sampler, output_textures_entry };

    auto compute_bind_group = std::make_unique<webgpu::raii::BindGroup>(
        m_ctx->device(), m_ctx->resource_registry().bind_group_layout("upsample_textures_compute"), entries, "compute: upsample textures bind group");

    // bind GPU resources and run pipeline
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "compute: upsample texture command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_ctx->device(), descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "compute: upsample texture compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts = glm::ceil(
                glm::vec3(input_used_indices.size(), m_output_storage_texture->width(), m_output_storage_texture->height()) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group->handle(), 0, nullptr);
            m_pipeline->run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "compute: upsampling texture command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_ctx->queue(), 1, &command);
        wgpuCommandBufferRelease(command);
    }

    // mark output texture array slots as used
    for (const auto& index : input_used_indices) {
        m_output_storage_texture->reserve(index);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              UpsampleTexturesNode* _this = reinterpret_cast<UpsampleTexturesNode*>(userdata);
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

} // namespace webgpu_engine::compute::nodes
