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

#include "ComputeReleasePointsNode.h"

namespace webgpu_compute::nodes {

glm::uvec3 ComputeReleasePointsNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

ComputeReleasePointsNode::ComputeReleasePointsNode(webgpu::Context& ctx)
    : ComputeReleasePointsNode(ctx, ReleasePointsSettings())
{
}

ComputeReleasePointsNode::ComputeReleasePointsNode(webgpu::Context& ctx, const ReleasePointsSettings& settings)
    : Node(
          {
              InputSocket(*this, "normal texture", data_type<const webgpu::raii::TextureWithSampler*>()),
          },
          {
              OutputSocket(*this, "release point texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
          })
    , m_ctx(&ctx)
    , m_settings { settings }
    , m_settings_uniform(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform)
{
    auto& reg = ctx.resource_registry();
    reg.register_shader("release_point_compute", "webgpu_compute::compute_release_points");
    reg.register_bind_group_layout("release_point_compute", [](WGPUDevice dev) {
        WGPUBindGroupLayoutEntry e0 {};
        e0.binding = 0;
        e0.visibility = WGPUShaderStage_Compute;
        e0.buffer.type = WGPUBufferBindingType_Uniform;

        WGPUBindGroupLayoutEntry e1 {};
        e1.binding = 1;
        e1.visibility = WGPUShaderStage_Compute;
        e1.texture.sampleType = WGPUTextureSampleType_Float;
        e1.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry e2 {};
        e2.binding = 2;
        e2.visibility = WGPUShaderStage_Compute;
        e2.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
        e2.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
        e2.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

        return std::make_unique<webgpu::raii::BindGroupLayout>(
            dev, std::vector<WGPUBindGroupLayoutEntry> { e0, e1, e2 }, "release point compute bind group layout");
    });
    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        m_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(
            device, reg.shader("release_point_compute"), std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("release_point_compute") });
    });
}

void ComputeReleasePointsNode::run_impl()
{

    const auto& normal_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("normal texture").get_connected_data());

    // create output texture
    m_output_texture = create_release_points_texture(m_ctx->device(),
        uint32_t(normal_texture.texture().width()),
        uint32_t(normal_texture.texture().height()),
        m_settings.texture_format,
        m_settings.texture_usage);

    // update settings on GPU side
    m_settings_uniform.data.min_slope_angle = m_settings.min_slope_angle;
    m_settings_uniform.data.max_slope_angle = m_settings.max_slope_angle;
    m_settings_uniform.data.sampling_interval = m_settings.sampling_interval;
    m_settings_uniform.update_gpu_data(m_ctx->queue());

    // create bind group
    WGPUBindGroupEntry input_settings_buffer_entry = m_settings_uniform.raw_buffer().create_bind_group_entry(0);
    WGPUBindGroupEntry input_normal_texture_entry = normal_texture.texture_view().create_bind_group_entry(1);
    WGPUBindGroupEntry output_texture_entry = m_output_texture->texture_view().create_bind_group_entry(2);

    std::vector<WGPUBindGroupEntry> entries {
        input_settings_buffer_entry,
        input_normal_texture_entry,
        output_texture_entry,
    };
    webgpu::raii::BindGroup compute_bind_group(
        m_ctx->device(), m_ctx->resource_registry().bind_group_layout("release_point_compute"), entries, "release points compute bind group");

    // bind GPU resources and run pipeline
    // the result is a texture with the calculated release points
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "release points compute command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_ctx->device(), descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "release points compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts
                = glm::ceil(glm::vec3(m_output_texture->texture().width(), m_output_texture->texture().height(), 1u) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline->run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "release points compute command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_ctx->queue(), 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              ComputeReleasePointsNode* _this = reinterpret_cast<ComputeReleasePointsNode*>(userdata);
              _this->complete_run();
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = this,
        .userdata2 = nullptr,
    };

    WGPUFuture future = wgpuQueueOnSubmittedWorkDone(m_ctx->queue(), callback_info);
}

std::unique_ptr<webgpu::raii::TextureWithSampler> ComputeReleasePointsNode::create_release_points_texture(
    WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage)
{
    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "release points storage texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = format;
    texture_desc.usage = usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "release points sampler", .length = WGPU_STRLEN };
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
