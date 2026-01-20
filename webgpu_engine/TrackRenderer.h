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
#include "PipelineManager.h"
#include "webgpu/raii/BindGroup.h"
#include "webgpu/raii/RawBuffer.h"
#include <glm/glm.hpp>
#include <vector>

namespace webgpu_engine {

using Coordinates = glm::dvec3;
using Track = std::vector<Coordinates>;

class TrackRenderer {
public:
    struct LineConfig {
        glm::vec4 line_color = { 1.0f, 0.0, 0.0, 1.0f };
    };

public:
    TrackRenderer(WGPUDevice device, const PipelineManager& pipeline_manager);

    void add_track(const Track& track, const glm::vec4& color = { 78.0 / 255.0f, 163.0 / 255.0f, 196.0 / 255.0f, 1.0f });
    void add_world_positions(const std::vector<glm::vec4>& world_positions, const glm::vec4& color = { 1.0f, 0.0f, 0.0f, 1.0f });

    void render(WGPUCommandEncoder command_encoder, const webgpu::raii::BindGroup& shared_config, const webgpu::raii::BindGroup& camera_config,
        const webgpu::raii::BindGroup& depth_texture, const webgpu::raii::TextureView& color_texture);

private:
    WGPUDevice m_device;
    WGPUQueue m_queue;
    const PipelineManager* m_pipeline_manager;

    std::vector<std::unique_ptr<webgpu::raii::RawBuffer<glm::fvec4>>> m_position_buffers;
    std::vector<std::unique_ptr<webgpu_engine::Buffer<TrackRenderer::LineConfig>>> m_line_config_buffers;
    std::vector<std::unique_ptr<webgpu::raii::BindGroup>> m_bind_groups;
};

} // namespace webgpu_engine
