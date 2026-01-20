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

class IterativeSimulationNode : public Node {
    Q_OBJECT

public:
    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    struct IterativeSimulationSettings {
        uint32_t max_num_iterations = 16;
    };

    struct IterativeSimulationSettingsUniform {
        uint32_t num_iteration;
        uint32_t padding1;
        uint32_t padding2;
        uint32_t padding3;
    };

    IterativeSimulationNode(const PipelineManager& pipeline_manager, WGPUDevice device);
    IterativeSimulationNode(const PipelineManager& pipeline_manager, WGPUDevice device, const IterativeSimulationSettings& settings);

public slots:
    void run_impl() override;

private:
    static std::unique_ptr<webgpu::raii::TextureWithSampler> create_texture(
        WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage);

private:
    const PipelineManager* m_pipeline_manager;
    WGPUDevice m_device;
    WGPUQueue m_queue;

    IterativeSimulationSettings m_settings;

    std::unique_ptr<Buffer<IterativeSimulationSettingsUniform>> m_settings_uniform;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_flux_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_input_parent_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_output_parent_buffer;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
};

} // namespace webgpu_engine::compute::nodes
