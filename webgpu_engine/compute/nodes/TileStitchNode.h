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

#define MAX_STITCHED_IMAGE_SIZE 8192

namespace webgpu_engine::compute::nodes {

class TileStitchNode : public Node {
    Q_OBJECT

public:
    struct StitchSettings {
        // The size of the input tiles (65x65?)
        glm::uvec2 tile_size;

        // If true, the right and bottom 1px wide edge will be ignored when stitching
        bool tile_has_border;

        // For slippyMap tiles this has to be set to true as y starts from the bottom
        bool stitch_inverted_y = true;

        // The format of the output texture
        // IMPORTANT: The caller has to ensure that the format of the input tiles has the same bit depth
        WGPUTextureFormat texture_format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm;

        // The usage flags of the output texture
        WGPUTextureUsage texture_usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    };

    TileStitchNode(const PipelineManager& manager, WGPUDevice device, StitchSettings settings);

public slots:
    void run_impl() override;

private:
    const PipelineManager* m_pipeline_manager;
    WGPUDevice m_device;
    WGPUQueue m_queue;

    StitchSettings m_settings;

    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
};

} // namespace webgpu_engine::compute::nodes
