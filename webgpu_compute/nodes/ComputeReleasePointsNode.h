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
#include <webgpu/Buffer.h>
#include <webgpu/Context.h>
#include <webgpu/raii/CombinedComputePipeline.h>

namespace webgpu_compute::nodes {

// TODO doc
class ComputeReleasePointsNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(ComputeReleasePointsNode)

    struct ReleasePointsSettings {
        WGPUTextureFormat texture_format = WGPUTextureFormat_RGBA8Unorm;
        WGPUTextureUsage texture_usage
            = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);
        float min_slope_angle = glm::radians(30.0f); // min slope angle [rad]
        float max_slope_angle = glm::radians(45.0f); // max slope angle [rad]
        glm::uvec2 sampling_interval = glm::uvec2(8); // sampling interval in x and y direction [every sampling_interval texels]
    };

    struct ReleasePointsSettingsUniform {
        float min_slope_angle;
        float max_slope_angle;
        glm::uvec2 sampling_interval;
    };

public:
    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    ComputeReleasePointsNode(webgpu::Context& ctx);
    ComputeReleasePointsNode(webgpu::Context& ctx, const ReleasePointsSettings& settings);

    void set_settings(const ReleasePointsSettings& settings) { m_settings = settings; }
    const ReleasePointsSettings& get_settings() const { return m_settings; }

public slots:
    void run_impl() override;

private:
    static std::unique_ptr<webgpu::raii::TextureWithSampler> create_release_points_texture(
        WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage);

private:
    webgpu::Context* m_ctx;

    ReleasePointsSettings m_settings;
    webgpu::Buffer<ReleasePointsSettingsUniform> m_settings_uniform;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_pipeline;
};

} // namespace webgpu_compute::nodes
