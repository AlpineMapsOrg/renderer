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

#include "Buffer.h"
#include "Node.h"
#include "webgpu_engine/PipelineManager.h"

namespace webgpu_engine::compute::nodes {

/// GPU compute node, calling run executes code on the GPU
class ComputeNormalsNode : public Node {
    Q_OBJECT

public:
    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    struct NormalSettings {
        WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
        WGPUTextureUsage usage = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
    };

    struct NormalsSettingsUniform {
        glm::vec2 aabb_min;
        glm::vec2 aabb_max;
    };

    ComputeNormalsNode(const PipelineManager& pipeline_manager, WGPUDevice device);

    void set_settings(const NormalSettings& settings);

public slots:
    void run_impl() override;

private:
    static std::unique_ptr<webgpu::raii::TextureWithSampler> create_normals_texture(
        WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage);

private:
    const PipelineManager* m_pipeline_manager;
    WGPUDevice m_device;
    WGPUQueue m_queue;

    NormalSettings m_settings;

    // input
    webgpu_engine::Buffer<NormalsSettingsUniform> m_normals_settings_uniform_buffer;

    // output
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture; // normal texture
};

} // namespace webgpu_engine::compute::nodes
