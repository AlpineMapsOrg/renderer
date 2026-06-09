/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Wendelin Muth
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

#include <memory>

#include <QObject>
#include <glm/glm.hpp>
#include <mutex>
#include <nucleus/tile/GpuArrayHelper.h>
#include <nucleus/tile/types.h>
#include <webgpu/Buffer.h>
#include <webgpu/Context.h>
#include <webgpu/raii/BindGroup.h>
#include <webgpu/raii/BindGroupLayout.h>
#include <webgpu/raii/CombinedComputePipeline.h>
#include <webgpu/raii/TextureWithSampler.h>
#include <webgpu/webgpu.h>

namespace nucleus::camera {
class Definition;
}

namespace webgpu_engine::clouds {
static constexpr uint32_t ZOOM_MAX = 10;
static constexpr glm::vec2 BOUNDS_MIN = { 46.2, 9.4 };
static constexpr glm::vec2 BOUNDS_MAX = { 49.2, 17.4 };
static constexpr glm::uvec2 TILE_COUNTS = { 24, 14 };
static constexpr uint32_t TILE_COUNT_TOTAL = TILE_COUNTS.x * TILE_COUNTS.y;
static constexpr uint32_t TILE_RESOLUTION_XY = 256;
static constexpr uint32_t TILE_RESOLUTION_Z = 64;
static constexpr float MAX_ALTITUDE = 14000.0;
static constexpr uint32_t ATLAS_BITS_XY = 2;
static constexpr uint32_t ATLAS_SCALE_XY = 1 << ATLAS_BITS_XY;
static constexpr uint32_t ATLAS_MASK_XY = ATLAS_SCALE_XY - 1;
static constexpr uint32_t ATLAS_BITS_Z = 5;
static constexpr uint32_t ATLAS_SCALE_Z = 1 << ATLAS_BITS_Z;
static constexpr uint32_t ATLAS_MASK_Z = ATLAS_SCALE_Z - 1;
static constexpr uint32_t LOADED_TILE_LIMIT = ATLAS_SCALE_XY * ATLAS_SCALE_XY * ATLAS_SCALE_Z;
} // namespace webgpu_engine::clouds

namespace webgpu_engine {

class CloudRenderer : public QObject {
    Q_OBJECT
public:
    // Public shader parameters
    struct ShaderParameters {
        float step_size_min = 125.0f;
        float step_size_distance_factor = 1.0f / 50.0f;
        float step_size_horizon_factor = 1000.0f;
        float scattering_coeff = 0.7f;
        float extinction_coeff = 1.0f;
        float albedo = 0.99f;
        float sun_light_scale = 800.0f;
        float ambient_light_scale = 1.0f;
        float atmospheric_light_scale = 1.0f;
        float shadow_extinction_scale = 0.5f;
        float powder_scale = 0.9f;
        float fade_factor = 0.0f;
        int stable_frames_limit = 1; // originally 64, but not necessary anymore due to improvements Wendelin made
    };

    ShaderParameters shader_params = {};

    explicit CloudRenderer();

    void init(webgpu::Context& ctx);

    void resize(int w, int h);

    void draw(const WGPUCommandEncoder& command_encoder,
        const WGPUBindGroup& depth_texture_bind_group,
        const WGPUBindGroup& shared_config_bind_group,
        const nucleus::camera::Definition& camera,
        uint32_t frame_number);

    [[nodiscard]] bool needs_redraw() const { return m_stable_frames <= static_cast<uint32_t>(shader_params.stable_frames_limit); }

    void set_tile_limit(unsigned new_limit);

    [[nodiscard]] webgpu::raii::TextureView* result_color_view(int frame) const
    {
        if (frame % 2 == 0)
            return m_clouds_hi_color_texture_view_b.get();
        return m_clouds_hi_color_texture_view_a.get();
    }

    [[nodiscard]] webgpu::raii::TextureView* result_depth_view() const { return m_clouds_lo_depth_texture_view.get(); }

signals:
    void tiles_changed();

public slots:
    void update_gpu_tiles_cloud(const std::vector<nucleus::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuTexture3DTile>& new_tiles);

private:
    struct alignas(16) CameraConfig {
        glm::mat4 view_matrix;
        glm::mat4 proj_matrix;
        glm::mat4 inv_view_matrix;
        glm::mat4 inv_proj_matrix;
        glm::vec4 position;
    };

    struct alignas(16) ShaderParamsRender {
        CameraConfig camera;
        glm::vec4 bounds_min;
        glm::vec4 bounds_max;

        uint32_t frame_index;
        float scattering_coeff;
        float extinction_coeff;
        float albedo;

        float step_size_min;
        float step_size_distance_factor;
        float step_size_horizon_factor;
        float fade_factor;

        float sun_light_scale;
        float ambient_light_scale;
        float atm_light_scale;
        float shadow_extinction_scale;

        glm::vec2 jitter;
        float powder_scale;
        float _padding0;
    };

    struct alignas(16) ShaderParamsUpscale {
        CameraConfig current_camera;
        CameraConfig previous_camera;
        glm::vec2 jitter;
        glm::vec2 prev_jitter;
        glm::vec2 low_res_texel_size;
        glm::vec2 high_res_texel_size;
        glm::vec2 resolution_scale;
        glm::vec2 _padding0;
    };

    struct TileInfo {
        glm::uint32 index;
        glm::uint32 zoom;
    };

    // tile coordinates of the bounds min corner at max zoom level
    glm::uvec2 m_tile_coords_offset = {};

    nucleus::tile::GpuArrayHelper m_loaded_cloud_textures;

    webgpu::Context* m_ctx = nullptr;

    std::unique_ptr<webgpu::Buffer<ShaderParamsRender>> m_render_shader_params_ubo;
    std::unique_ptr<webgpu::Buffer<ShaderParamsUpscale>> m_upscale_shader_params_ubo;

    std::unique_ptr<webgpu::raii::RawBuffer<TileInfo>> m_cloud_tile_info_buffer;
    std::vector<TileInfo> m_tile_infos;

    std::unique_ptr<webgpu::raii::Texture> m_cloud_atlas_texture;
    std::unique_ptr<webgpu::raii::TextureView> m_cloud_atlas_view;
    std::unique_ptr<webgpu::raii::Sampler> m_cloud_linear_sampler;

    glm::uvec2 m_output_lo_resolution = {};
    glm::uvec2 m_output_hi_resolution = {};
    std::unique_ptr<webgpu::raii::Texture> m_clouds_lo_color_texture;
    std::unique_ptr<webgpu::raii::TextureView> m_clouds_lo_color_texture_view;
    std::unique_ptr<webgpu::raii::Texture> m_clouds_lo_depth_texture;
    std::unique_ptr<webgpu::raii::TextureView> m_clouds_lo_depth_texture_view;
    std::unique_ptr<webgpu::raii::Texture> m_clouds_hi_color_texture_a;
    std::unique_ptr<webgpu::raii::TextureView> m_clouds_hi_color_texture_view_a;
    std::unique_ptr<webgpu::raii::Texture> m_clouds_hi_color_texture_b;
    std::unique_ptr<webgpu::raii::TextureView> m_clouds_hi_color_texture_view_b;
    std::unique_ptr<webgpu::raii::Sampler> m_linear_sampler;

    std::unique_ptr<webgpu::raii::BindGroup> m_render_clouds_bind_group;
    std::unique_ptr<webgpu::raii::BindGroup> m_upscale_clouds_bind_group_a;
    std::unique_ptr<webgpu::raii::BindGroup> m_upscale_clouds_bind_group_b;
    std::unique_ptr<webgpu::raii::BindGroup> m_camera_bind_group;

    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_render_clouds_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_upscale_clouds_pipeline;

    uint32_t m_stable_frames = 0;

    std::mutex m_mutex = {};
};

} // namespace webgpu_engine
