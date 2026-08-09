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

#include <webgpu/base/Context.h>
#include <webgpu/base/RenderResourceRegistry.h>
#include <webgpu/base/raii/BindGroup.h>
#include <webgpu/base/raii/BindGroupLayout.h>
#include <webgpu/base/raii/CombinedComputePipeline.h>
#include <webgpu/base/raii/Texture.h>
#include <webgpu/base/raii/TextureView.h>
#include <webgpu/base/raii/base_types.h>

#include <QDebug>
#include <glm/glm.hpp>
#include <webgpu/webgpu.h>
#include <webgpu/base/webgpu_interface.hpp>

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

WGPUBindGroupEntry bind(uint32_t binding, WGPUTextureView view)
{
    WGPUBindGroupEntry e {};
    e.binding = binding;
    e.textureView = view;
    return e;
}

WGPUBindGroupEntry bind(uint32_t binding, WGPUSampler sampler)
{
    WGPUBindGroupEntry e {};
    e.binding = binding;
    e.sampler = sampler;
    return e;
}

WGPUBindGroupEntry bind(uint32_t binding, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WGPUBindGroupEntry e {};
    e.binding = binding;
    e.buffer = buffer;
    e.offset = offset;
    e.size = size;
    return e;
}

// ---- WebGPU texture-format math ------------------------------------------------------------------

// block size in texels (1x1 for uncompressed) plus bytes per block
TexelBlock format_block(WGPUTextureFormat f)
{
    switch (f)
    {
    // 8-bit
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_Stencil8:
        return { 1, 1, 1 };

    // 16-bit
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:

    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:

    case WGPUTextureFormat_Depth16Unorm:
        return { 1, 1, 2 };

    // 32-bit
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:

    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:

    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:

    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:

    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:

    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth32Float:
        return { 1, 1, 4 };

    // 64-bit
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:

    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:

    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return { 1, 1, 8 };

    // 128-bit
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return { 1, 1, 16 };


    // BC1 / BC4: 8 bytes per 4x4 block
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
        return { 4, 4, 8 };

    // BC2/3/5/6H/7: 16 bytes per 4x4 block
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return { 4, 4, 16 };


    // ETC2 RGB8 / RGB8A1 and EAC R11: 8 bytes per 4x4 block
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
        return { 4, 4, 8 };

    // ETC2 RGBA8 and EAC RG11: 16 bytes per 4x4 block
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
        return { 4, 4, 16 };


    // ASTC: always 16 bytes per block, block footprint read straight off the enum name
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:   return { 4, 4, 16 };
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:   return { 5, 4, 16 };
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:   return { 5, 5, 16 };
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:   return { 6, 5, 16 };
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:   return { 6, 6, 16 };
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:   return { 8, 5, 16 };
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:   return { 8, 6, 16 };
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:   return { 8, 8, 16 };
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:  return { 10, 5, 16 };
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:  return { 10, 6, 16 };
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:  return { 10, 8, 16 };
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb: return { 10, 10, 16 };
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb: return { 12, 10, 16 };
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb: return { 12, 12, 16 };

    default:
        break;
    }

    return { 0, 0, 0 };
}

uint64_t texture_bytes(WGPUExtent3D size, WGPUTextureFormat format, uint32_t mipLevelCount, uint32_t sampleCount, WGPUTextureDimension dim)
{
    const TexelBlock b = format_block(format);
    if (!b.bytes) return 0;
    const bool is3D = dim == WGPUTextureDimension_3D;
    const uint32_t layers = is3D ? 1u : (size.depthOrArrayLayers ? size.depthOrArrayLayers : 1);
    const uint32_t samples = sampleCount ? sampleCount : 1;
    uint64_t total = 0;
    for (uint32_t m = 0; m < mipLevelCount; ++m) {
        const uint32_t w = (size.width  >> m) ? (size.width  >> m) : 1u;
        const uint32_t h = (size.height >> m) ? (size.height >> m) : 1u;
        const uint32_t d = is3D ? ((size.depthOrArrayLayers >> m) ? (size.depthOrArrayLayers >> m) : 1u) : 1u;
        const uint32_t bw = (w + b.w - 1) / b.w;
        const uint32_t bh = (h + b.h - 1) / b.h;
        total += (uint64_t)bw * bh * d * layers * samples * b.bytes;
    }
    return total;
}

const char* format_name(WGPUTextureFormat f)
{
    switch (f)
    {
    // 8-bit formats
    case WGPUTextureFormat_R8Unorm:              return "R8Unorm";
    case WGPUTextureFormat_R8Snorm:              return "R8Snorm";
    case WGPUTextureFormat_R8Uint:               return "R8Uint";
    case WGPUTextureFormat_R8Sint:               return "R8Sint";

    // 16-bit formats
    case WGPUTextureFormat_R16Uint:              return "R16Uint";
    case WGPUTextureFormat_R16Sint:              return "R16Sint";
    case WGPUTextureFormat_R16Float:             return "R16Float";

    case WGPUTextureFormat_RG8Unorm:             return "RG8Unorm";
    case WGPUTextureFormat_RG8Snorm:             return "RG8Snorm";
    case WGPUTextureFormat_RG8Uint:              return "RG8Uint";
    case WGPUTextureFormat_RG8Sint:              return "RG8Sint";

    // 32-bit formats
    case WGPUTextureFormat_R32Float:             return "R32Float";
    case WGPUTextureFormat_R32Uint:              return "R32Uint";
    case WGPUTextureFormat_R32Sint:              return "R32Sint";

    case WGPUTextureFormat_RG16Uint:             return "RG16Uint";
    case WGPUTextureFormat_RG16Sint:             return "RG16Sint";
    case WGPUTextureFormat_RG16Float:            return "RG16Float";

    case WGPUTextureFormat_RGBA8Unorm:           return "RGBA8Unorm";
    case WGPUTextureFormat_RGBA8UnormSrgb:       return "RGBA8UnormSrgb";
    case WGPUTextureFormat_RGBA8Snorm:           return "RGBA8Snorm";
    case WGPUTextureFormat_RGBA8Uint:            return "RGBA8Uint";
    case WGPUTextureFormat_RGBA8Sint:            return "RGBA8Sint";

    case WGPUTextureFormat_BGRA8Unorm:           return "BGRA8Unorm";
    case WGPUTextureFormat_BGRA8UnormSrgb:       return "BGRA8UnormSrgb";

    case WGPUTextureFormat_RGB10A2Uint:          return "RGB10A2Uint";
    case WGPUTextureFormat_RGB10A2Unorm:         return "RGB10A2Unorm";
    case WGPUTextureFormat_RG11B10Ufloat:        return "RG11B10Ufloat";

    // 64-bit formats
    case WGPUTextureFormat_RG32Float:            return "RG32Float";
    case WGPUTextureFormat_RG32Uint:             return "RG32Uint";
    case WGPUTextureFormat_RG32Sint:             return "RG32Sint";

    case WGPUTextureFormat_RGBA16Uint:           return "RGBA16Uint";
    case WGPUTextureFormat_RGBA16Sint:           return "RGBA16Sint";
    case WGPUTextureFormat_RGBA16Float:          return "RGBA16Float";

    // 128-bit formats
    case WGPUTextureFormat_RGBA32Float:          return "RGBA32Float";
    case WGPUTextureFormat_RGBA32Uint:           return "RGBA32Uint";
    case WGPUTextureFormat_RGBA32Sint:           return "RGBA32Sint";

    // depth / stencil
    case WGPUTextureFormat_Stencil8:             return "Stencil8";
    case WGPUTextureFormat_Depth16Unorm:         return "Depth16Unorm";
    case WGPUTextureFormat_Depth24Plus:          return "Depth24Plus";
    case WGPUTextureFormat_Depth24PlusStencil8: return "Depth24PlusStencil8";
    case WGPUTextureFormat_Depth32Float:         return "Depth32Float";
    case WGPUTextureFormat_Depth32FloatStencil8:return "Depth32FloatStencil8";

    // BC compressed
    case WGPUTextureFormat_BC1RGBAUnorm:         return "BC1RGBAUnorm";
    case WGPUTextureFormat_BC1RGBAUnormSrgb:     return "BC1RGBAUnormSrgb";
    case WGPUTextureFormat_BC2RGBAUnorm:         return "BC2RGBAUnorm";
    case WGPUTextureFormat_BC2RGBAUnormSrgb:     return "BC2RGBAUnormSrgb";
    case WGPUTextureFormat_BC3RGBAUnorm:         return "BC3RGBAUnorm";
    case WGPUTextureFormat_BC3RGBAUnormSrgb:     return "BC3RGBAUnormSrgb";
    case WGPUTextureFormat_BC4RUnorm:            return "BC4RUnorm";
    case WGPUTextureFormat_BC4RSnorm:            return "BC4RSnorm";
    case WGPUTextureFormat_BC5RGUnorm:           return "BC5RGUnorm";
    case WGPUTextureFormat_BC5RGSnorm:           return "BC5RGSnorm";
    case WGPUTextureFormat_BC6HRGBUfloat:        return "BC6HRGBUfloat";
    case WGPUTextureFormat_BC6HRGBFloat:         return "BC6HRGBFloat";
    case WGPUTextureFormat_BC7RGBAUnorm:         return "BC7RGBAUnorm";
    case WGPUTextureFormat_BC7RGBAUnormSrgb:     return "BC7RGBAUnormSrgb";

    // ETC2
    case WGPUTextureFormat_ETC2RGB8Unorm:             return "ETC2RGB8Unorm";
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:         return "ETC2RGB8UnormSrgb";
    case WGPUTextureFormat_ETC2RGB8A1Unorm:           return "ETC2RGB8A1Unorm";
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:       return "ETC2RGB8A1UnormSrgb";
    case WGPUTextureFormat_ETC2RGBA8Unorm:            return "ETC2RGBA8Unorm";
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:        return "ETC2RGBA8UnormSrgb";
    case WGPUTextureFormat_EACR11Unorm:               return "EACR11Unorm";
    case WGPUTextureFormat_EACR11Snorm:               return "EACR11Snorm";
    case WGPUTextureFormat_EACRG11Unorm:              return "EACRG11Unorm";
    case WGPUTextureFormat_EACRG11Snorm:              return "EACRG11Snorm";

    // ASTC
    case WGPUTextureFormat_ASTC4x4Unorm:              return "ASTC4x4Unorm";
    case WGPUTextureFormat_ASTC4x4UnormSrgb:          return "ASTC4x4UnormSrgb";
    case WGPUTextureFormat_ASTC5x4Unorm:              return "ASTC5x4Unorm";
    case WGPUTextureFormat_ASTC5x4UnormSrgb:          return "ASTC5x4UnormSrgb";
    case WGPUTextureFormat_ASTC5x5Unorm:              return "ASTC5x5Unorm";
    case WGPUTextureFormat_ASTC5x5UnormSrgb:          return "ASTC5x5UnormSrgb";
    case WGPUTextureFormat_ASTC6x5Unorm:              return "ASTC6x5Unorm";
    case WGPUTextureFormat_ASTC6x5UnormSrgb:          return "ASTC6x5UnormSrgb";
    case WGPUTextureFormat_ASTC6x6Unorm:              return "ASTC6x6Unorm";
    case WGPUTextureFormat_ASTC6x6UnormSrgb:          return "ASTC6x6UnormSrgb";
    case WGPUTextureFormat_ASTC8x5Unorm:              return "ASTC8x5Unorm";
    case WGPUTextureFormat_ASTC8x5UnormSrgb:          return "ASTC8x5UnormSrgb";
    case WGPUTextureFormat_ASTC8x6Unorm:              return "ASTC8x6Unorm";
    case WGPUTextureFormat_ASTC8x6UnormSrgb:          return "ASTC8x6UnormSrgb";
    case WGPUTextureFormat_ASTC8x8Unorm:              return "ASTC8x8Unorm";
    case WGPUTextureFormat_ASTC8x8UnormSrgb:          return "ASTC8x8UnormSrgb";
    case WGPUTextureFormat_ASTC10x5Unorm:             return "ASTC10x5Unorm";
    case WGPUTextureFormat_ASTC10x5UnormSrgb:         return "ASTC10x5UnormSrgb";
    case WGPUTextureFormat_ASTC10x6Unorm:             return "ASTC10x6Unorm";
    case WGPUTextureFormat_ASTC10x6UnormSrgb:         return "ASTC10x6UnormSrgb";
    case WGPUTextureFormat_ASTC10x8Unorm:             return "ASTC10x8Unorm";
    case WGPUTextureFormat_ASTC10x8UnormSrgb:         return "ASTC10x8UnormSrgb";
    case WGPUTextureFormat_ASTC10x10Unorm:            return "ASTC10x10Unorm";
    case WGPUTextureFormat_ASTC10x10UnormSrgb:        return "ASTC10x10UnormSrgb";
    case WGPUTextureFormat_ASTC12x10Unorm:            return "ASTC12x10Unorm";
    case WGPUTextureFormat_ASTC12x10UnormSrgb:        return "ASTC12x10UnormSrgb";
    case WGPUTextureFormat_ASTC12x12Unorm:            return "ASTC12x12Unorm";
    case WGPUTextureFormat_ASTC12x12UnormSrgb:        return "ASTC12x12UnormSrgb";

    default:
        break;
    }

    return ""; 
}


const char* format_name_short(WGPUTextureFormat f)
{
    switch (f) {
      case WGPUTextureFormat_RGBA8Unorm:   return "RGBA8";
      case WGPUTextureFormat_BGRA8Unorm:   return "BGRA8";
      case WGPUTextureFormat_RGBA16Float:  return "RGBA16F";
      case WGPUTextureFormat_R32Float:     return "R32F";
      case WGPUTextureFormat_Depth32Float: return "D32F";
      case WGPUTextureFormat_Depth24Plus:  return "D24+";
      case WGPUTextureFormat_R8Unorm:      return "R8";
      default:                             return "tex";
    }
}

} // namespace webgpu
