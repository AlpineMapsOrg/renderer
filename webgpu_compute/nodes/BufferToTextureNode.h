/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#pragma once

#include "Node.h"
#include <array>
#include <webgpu/Buffer.h>
#include <webgpu/Context.h>
#include <webgpu/raii/CombinedComputePipeline.h>

namespace webgpu_compute::nodes {

// Takes a list of tile ids, a tile id-to-index hashmap and a int32 storage buffer.
// The storage buffer contains raster data per tile. The resolution has to be specified as output_resolution on construction.
//
// Data for tile_id, row_index (from top to bottom) and col_index (from left to right) is stored in the buffer at index
//
//     hashmap.get_index(tile_id) * tile_dimensions.x * tile_dimensions.y + row_index * tile_dimensions.x + col_index
//
// where tile_dimensions is the per-tile resolution.
//
// Each entry is expected to be between 0 and 2^32-1 (i.e. using full uint32).
// The entries are mapped to colors based on a color mapping function (defined in shader code).
//
// TODO: add settings struct, be able to change color mapping during runtime (without changing shader source and recompiling)
class BufferToTextureNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(BufferToTextureNode)

    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    static const uint32_t MAX_TEXTURE_RESOLUTION;

    struct BufferToTextureSettings {
        WGPUTextureFormat texture_format = WGPUTextureFormat_RGBA8Unorm;
        WGPUTextureUsage texture_usage
            = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment);
        WGPUFilterMode texture_filter_mode = WGPUFilterMode_Linear;
        WGPUMipmapFilterMode texture_mipmap_filter_mode = WGPUMipmapFilterMode_Linear;
        uint16_t texture_max_aniostropy = 1;

        bool create_mipmaps = true;

        glm::vec2 color_map_bounds = { 0.0f, 40.0f };
        glm::vec2 transparency_map_bounds = { 0.0f, 1.0f }; // x gets mapped to fully invisible, y to fully visible
        bool use_bin_interpolation = false; // if true, use linear interpolation between color bins
        bool use_transparency_buffer = true; // if true, the transparency texture is used to evaluate an alpha factor based on the alpha_remap_bounds
    };

    struct BufferToTextureSettingsUniform {
        glm::uvec2 input_resolution = glm::uvec2(0u); // is set based on input "raster dimensions"
        glm::vec2 color_map_bounds;
        glm::vec2 transparency_map_bounds;
        uint32_t use_bin_interpolation;
        uint32_t use_transparency_buffer;
    };

    BufferToTextureSettings& settings() { return m_settings; }

    BufferToTextureNode(webgpu::Context& ctx);
    BufferToTextureNode(webgpu::Context& ctx, const BufferToTextureSettings& settings);

public slots:
    void run_impl() override;

private:
    void update_gpu_settings();

    std::unique_ptr<webgpu::raii::TextureWithSampler> create_texture(WGPUDevice device, uint32_t width, uint32_t height, BufferToTextureSettings& settings);

private:
    webgpu::Context* m_ctx;

    BufferToTextureSettings m_settings;
    webgpu::Buffer<BufferToTextureSettingsUniform> m_settings_uniform;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_pipeline;

    // IMPORTANT: The output needs to be double-buffered if linked to rendering
    std::array<std::unique_ptr<webgpu::raii::TextureWithSampler>, 2> m_output_textures;
    // NOTE: We need a non default texture view as a storage texture can only have mipLevelCount = 1
    std::array<std::unique_ptr<webgpu::raii::TextureView>, 2> m_output_views;
    int m_pingpong = 0; // current output_texture to write to
};

} // namespace webgpu_compute::nodes
