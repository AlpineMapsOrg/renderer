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

#include "PipelineManager.h"
#include "webgpu/raii/BindGroup.h"
#include "webgpu/raii/Texture.h"
#include "webgpu/raii/TextureView.h"
#include "webgpu/raii/base_types.h"

#include <QDebug>
#include <glm/glm.hpp>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_interface.hpp>

namespace webgpu_engine {

void compute_mipmaps_for_texture(WGPUDevice device, WGPUQueue queue, const PipelineManager& pipeline_manager, const webgpu::raii::Texture* texture)
{
    glm::uvec2 baseSize = { texture->width(), texture->height() };
    uint32_t mipLevelCount = texture->mip_level_count();

    if (mipLevelCount == 1) {
        qDebug() << "No mipmaps to compute";
        return;
    } else {
        qDebug() << "Computing" << mipLevelCount << "mipmaps for texture";
    }

    std::vector<std::unique_ptr<webgpu::raii::TextureView>> textureMipViews;
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
        textureMipViews.push_back(std::make_unique<webgpu::raii::TextureView>(texture->handle(), viewDesc));

        mipSizes[i].width = std::max(1u, baseSize.x >> i);
        mipSizes[i].height = std::max(1u, baseSize.y >> i);
        mipSizes[i].depthOrArrayLayers = 1;
    }

    std::vector<std::unique_ptr<webgpu::raii::BindGroup>> bindGroups;
    for (uint32_t i = 0; i < mipLevelCount - 1; i++) {
        std::vector<WGPUBindGroupEntry> bgEntries {
            textureMipViews[i]->create_bind_group_entry(0),
            textureMipViews[i + 1]->create_bind_group_entry(1),
        };
        bindGroups.push_back(std::make_unique<webgpu::raii::BindGroup>(device, pipeline_manager.mipmap_creation_bind_group_layout(), bgEntries, "mipmap creation bindgroup"));
    }

    constexpr glm::uvec3 SHADER_WORKGROUP_SIZE = { 8, 8, 1 };
    {
        WGPUCommandEncoderDescriptor descriptor {};
        webgpu::raii::CommandEncoder encoder(device, descriptor);

        for (uint32_t i = 0; i < mipLevelCount - 1; i++) {
            WGPUComputePassDescriptor compute_pass_desc {};
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts = glm::ceil(glm::vec3(mipSizes[i + 1].width, mipSizes[i + 1].height, 1) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, bindGroups[i]->handle(), 0, nullptr);
            pipeline_manager.mipmap_creation_pipeline().run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "MipMap command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }
}

} // namespace webgpu_engine
