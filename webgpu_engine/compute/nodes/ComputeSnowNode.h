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

#pragma once

#include "Node.h"
#include "webgpu_engine/Buffer.h"
#include "webgpu_engine/PipelineManager.h"

namespace webgpu_engine::compute::nodes {

/// GPU compute node, calling run executes code on the GPU
class ComputeSnowNode : public Node {
    Q_OBJECT

public:
    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    struct SnowSettings {
        WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
        WGPUTextureUsage usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;

        float min_angle = 0; // slope angle in degrees
        float max_angle = 45; // slope angle in degrees
        float angle_blend = 0; // TODO doc

        float min_altitude = 1000; // minimal altitude in meters
        float altitude_variation = 200; // TODO doc
        float altitude_blend = 200; // TODO doc
    };

    struct SnowSettingsUniform {
        glm::vec4 angle = {
            1, // snow enabled
            0, // angle lower limit// angle lower limit
            30, // angle upper limit
            0, // angle blend
        };

        glm::vec4 alt = {
            1000, // min altitude
            200, // variation
            200, // blend
            1, // specular
        };
    };

    struct RegionBoundsUniform {
        glm::vec2 aabb_min;
        glm::vec2 aabb_max;
    };

    ComputeSnowNode(const PipelineManager& pipeline_manager, WGPUDevice device);
    ComputeSnowNode(const PipelineManager& pipeline_manager, WGPUDevice device, const SnowSettings& settings);

    void set_snow_settings(const SnowSettings& settings) { m_settings = settings; }

public slots:
    void run_impl() override;

private:
    static std::unique_ptr<webgpu::raii::TextureWithSampler> create_snow_texture(
        WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage);

private:
    const PipelineManager* m_pipeline_manager;
    WGPUDevice m_device;
    WGPUQueue m_queue;

    SnowSettings m_settings;
    webgpu_engine::Buffer<SnowSettingsUniform> m_snow_settings_uniform_buffer;
    webgpu_engine::Buffer<RegionBoundsUniform> m_region_bounds_uniform_buffer;

    // input
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_input_normals_texture; // normal texture

    // output
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_snow_texture; // snow texture
};

} // namespace webgpu_engine::compute::nodes
