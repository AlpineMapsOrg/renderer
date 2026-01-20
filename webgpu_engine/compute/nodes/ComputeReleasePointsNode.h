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
#include "PipelineManager.h"

#include <Buffer.h>

namespace webgpu_engine::compute::nodes {

// TODO doc
class ComputeReleasePointsNode : public Node {
    Q_OBJECT

public:
    struct ReleasePointsSettings {
        WGPUTextureFormat texture_format = WGPUTextureFormat_RGBA8Unorm;
        WGPUTextureUsage texture_usage
            = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);
        float min_slope_angle = glm::radians(30.0f); // min slope angle [rad]
        float max_slope_angle = glm::radians(45.0f); // max slope angle [rad]
        glm::uvec2 sampling_interval; // sampling interval in x and y direction [every sampling_interval texels]
    };

    struct ReleasePointsSettingsUniform {
        float min_slope_angle;
        float max_slope_angle;
        glm::uvec2 sampling_interval;
    };

public:
    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    ComputeReleasePointsNode(const PipelineManager& pipeline_manager, WGPUDevice device);
    ComputeReleasePointsNode(const PipelineManager& pipeline_manager, WGPUDevice device, const ReleasePointsSettings& settings);

    void set_settings(const ReleasePointsSettings& settings) { m_settings = settings; }

public slots:
    void run_impl() override;

private:
    static std::unique_ptr<webgpu::raii::TextureWithSampler> create_release_points_texture(
        WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage);

private:
    const PipelineManager* m_pipeline_manager;
    WGPUDevice m_device;
    WGPUQueue m_queue;

    ReleasePointsSettings m_settings;
    webgpu_engine::Buffer<ReleasePointsSettingsUniform> m_settings_uniform;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
};

} // namespace webgpu_engine::compute::nodes
