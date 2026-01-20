/*****************************************************************************
 * weBIGeo
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

#pragma once

#include "Node.h"
#include "PipelineManager.h"
#include "webgpu_engine/Buffer.h"

namespace webgpu_engine::compute::nodes {

class HeightDecodeNode : public Node {
    Q_OBJECT

public:
    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    struct HeightDecodeSettings {
        // The usage flags of the output texture
        WGPUTextureUsage texture_usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    };

    struct HeightDecodeSettingsUniform {
        glm::vec2 aabb_min;
        glm::vec2 aabb_max;
    };

    HeightDecodeNode(const PipelineManager& manager, WGPUDevice device, HeightDecodeSettings settings);

public slots:
    void run_impl() override;

private:
    const PipelineManager* m_pipeline_manager;
    WGPUDevice m_device;
    WGPUQueue m_queue;

    HeightDecodeSettings m_settings;
    webgpu_engine::Buffer<HeightDecodeSettingsUniform> m_settings_uniform;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
};

} // namespace webgpu_engine::compute::nodes
