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

#include "ComputeNormalsNode.h"

#include <QDebug>

namespace webgpu_compute::nodes {

glm::uvec3 ComputeNormalsNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

ComputeNormalsNode::ComputeNormalsNode(webgpu::Context& ctx)
    : Node(
          {
              InputSocket(*this, "bounds", data_type<const radix::geometry::Aabb<2, double>*>()),
              InputSocket(*this, "height texture", data_type<const webgpu::raii::TextureWithSampler*>()),
          },
          {
              OutputSocket(*this, "normal texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
          })
    , m_ctx(&ctx)
    , m_normals_settings_uniform_buffer(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform)
{
    auto& reg = ctx.resource_registry();
    reg.register_shader("normals_compute", "webgpu_compute::normals_compute");
    reg.register_bind_group_layout("normals_compute", [](WGPUDevice dev) {
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
        e2.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
        e2.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
        e2.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

        return std::make_unique<webgpu::raii::BindGroupLayout>(dev, std::vector<WGPUBindGroupLayoutEntry> { e0, e1, e2 }, "normals compute bind group layout");
    });
    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        m_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(
            device, reg.shader("normals_compute"), std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("normals_compute") });
    });
}

void ComputeNormalsNode::set_settings(const NormalSettings& settings) { m_settings = settings; }

void ComputeNormalsNode::run_impl()
{

    const auto& bounds = *std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("bounds").get_connected_data());
    const auto& height_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("height texture").get_connected_data());

    m_output_texture = create_normals_texture(
        m_ctx->device(), uint32_t(height_texture.texture().width()), uint32_t(height_texture.texture().height()), m_settings.format, m_settings.usage);

    m_normals_settings_uniform_buffer.data.aabb_min = glm::fvec2(bounds.min);
    m_normals_settings_uniform_buffer.data.aabb_max = glm::fvec2(bounds.max);
    m_normals_settings_uniform_buffer.update_gpu_data(m_ctx->queue());

    // create bind group
    // TODO re-create bind groups only when input change
    std::vector<WGPUBindGroupEntry> entries {
        m_normals_settings_uniform_buffer.raw_buffer().create_bind_group_entry(0),
        height_texture.texture_view().create_bind_group_entry(1),
        m_output_texture->texture_view().create_bind_group_entry(2),
    };
    webgpu::raii::BindGroup compute_bind_group(
        m_ctx->device(), m_ctx->resource_registry().bind_group_layout("normals_compute"), entries, "compute controller bind group");

    // bind GPU resources and run pipeline
    // the result is a texture array with the calculated overlays, and a hashmap that maps id to texture array index
    // the shader will only writes into texture array, the hashmap is written on cpu side
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "compute controller command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_ctx->device(), descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "compute controller compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts
                = glm::ceil(glm::vec3(m_output_texture->texture().width(), m_output_texture->texture().height(), 1) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline->run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "NormalComputeNode command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_ctx->queue(), 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              ComputeNormalsNode* _this = reinterpret_cast<ComputeNormalsNode*>(userdata);
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
    // emit run_completed();
}

std::unique_ptr<webgpu::raii::TextureWithSampler> ComputeNormalsNode::create_normals_texture(
    WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage)
{
    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "normals storage texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = format;
    texture_desc.usage = usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "normals sampler", .length = WGPU_STRLEN };
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

} // namespace webgpu_compute::nodes
