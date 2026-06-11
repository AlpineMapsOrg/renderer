/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "gpu_utils.h"

#include <webgpu/Context.h>
#include <webgpu/RenderResourceRegistry.h>
#include <webgpu/raii/BindGroup.h>
#include <webgpu/raii/BindGroupLayout.h>
#include <webgpu/raii/CombinedComputePipeline.h>
#include <webgpu/raii/Texture.h>
#include <webgpu/raii/TextureView.h>
#include <webgpu/raii/base_types.h>

#include <QDebug>
#include <glm/glm.hpp>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_interface.hpp>

namespace webgpu {

namespace {
    // Registers the mipmap-creation shader and bind group layout once (idempotent), so the
    // utility is self-contained and does not depend on any higher-level target initialising it.
    void ensure_mipmap_resources(RenderResourceRegistry& reg)
    {
        if (!reg.has_shader("mipmap_creation"))
            reg.register_shader("mipmap_creation", "webgpu::mipmap");

        if (!reg.has_bind_group_layout("mipmap_creation"))
            reg.register_bind_group_layout("mipmap_creation", [](WGPUDevice device) {
                WGPUBindGroupLayoutEntry input_entry {};
                input_entry.binding = 0;
                input_entry.visibility = WGPUShaderStage_Compute;
                input_entry.texture.sampleType = WGPUTextureSampleType_Float;
                input_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

                WGPUBindGroupLayoutEntry output_entry {};
                output_entry.binding = 1;
                output_entry.visibility = WGPUShaderStage_Compute;
                output_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
                output_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
                output_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

                return std::make_unique<raii::BindGroupLayout>(
                    device, std::vector<WGPUBindGroupLayoutEntry> { input_entry, output_entry }, "mipmap creation bind group layout");
            });
    }
} // namespace

void compute_mipmaps_for_texture(Context& ctx, const raii::Texture* texture) { compute_mipmaps_for_texture(ctx, texture, {}); }

void compute_mipmaps_for_texture(Context& ctx, const raii::Texture* texture, WGPUQueueWorkDoneCallbackInfo on_done)
{
    WGPUDevice device = ctx.device();
    WGPUQueue queue = ctx.queue();
    auto& reg = ctx.resource_registry();
    ensure_mipmap_resources(reg);

    glm::uvec2 baseSize = { texture->width(), texture->height() };
    uint32_t mipLevelCount = texture->mip_level_count();

    if (mipLevelCount == 1) {
        qDebug() << "No mipmaps to compute";
        return;
    } else {
        qDebug() << "Computing" << mipLevelCount << "mipmaps for texture";
    }

    raii::CombinedComputePipeline pipeline(device,
        reg.shader("mipmap_creation"),
        std::vector<const raii::BindGroupLayout*> { &reg.bind_group_layout("mipmap_creation") },
        "mipmap creation compute pipeline");

    std::vector<std::unique_ptr<raii::TextureView>> textureMipViews;
    std::vector<WGPUExtent3D> mipSizes(mipLevelCount);

    for (uint32_t i = 0; i < mipLevelCount; i++) {
        WGPUTextureViewDescriptor viewDesc {};
        viewDesc.dimension = WGPUTextureViewDimension::WGPUTextureViewDimension_2D;
        viewDesc.format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm;
        viewDesc.baseMipLevel = i;
        viewDesc.mipLevelCount = 1;
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1;
        viewDesc.aspect = WGPUTextureAspect::WGPUTextureAspect_All;
        textureMipViews.push_back(std::make_unique<raii::TextureView>(texture->handle(), viewDesc));

        mipSizes[i].width = std::max(1u, baseSize.x >> i);
        mipSizes[i].height = std::max(1u, baseSize.y >> i);
        mipSizes[i].depthOrArrayLayers = 1;
    }

    std::vector<std::unique_ptr<raii::BindGroup>> bindGroups;
    for (uint32_t i = 0; i < mipLevelCount - 1; i++) {
        std::vector<WGPUBindGroupEntry> bgEntries {
            textureMipViews[i]->create_bind_group_entry(0),
            textureMipViews[i + 1]->create_bind_group_entry(1),
        };
        bindGroups.push_back(std::make_unique<raii::BindGroup>(device, reg.bind_group_layout("mipmap_creation"), bgEntries, "mipmap creation bindgroup"));
    }

    constexpr glm::uvec3 SHADER_WORKGROUP_SIZE = { 8, 8, 1 };
    {
        WGPUCommandEncoderDescriptor descriptor {};
        raii::CommandEncoder encoder(device, descriptor);

        for (uint32_t i = 0; i < mipLevelCount - 1; i++) {
            WGPUComputePassDescriptor compute_pass_desc {};
            raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts = glm::ceil(glm::vec3(mipSizes[i + 1].width, mipSizes[i + 1].height, 1) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, bindGroups[i]->handle(), 0, nullptr);
            pipeline.run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "MipMap command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    if (on_done.callback)
        wgpuQueueOnSubmittedWorkDone(queue, on_done);
}

} // namespace webgpu
