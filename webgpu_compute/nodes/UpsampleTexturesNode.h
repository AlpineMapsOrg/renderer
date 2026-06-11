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
#include "webgpu/raii/RawBuffer.h"
#include <webgpu/Context.h>
#include <webgpu/raii/CombinedComputePipeline.h>

namespace webgpu_compute::nodes {

class UpsampleTexturesNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(UpsampleTexturesNode)

    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    UpsampleTexturesNode(webgpu::Context& ctx, glm::uvec2 target_resolution, size_t capacity);

public slots:
    void run_impl() override;

private:
    webgpu::Context* m_ctx;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_pipeline;

    glm::uvec2 m_target_resolution;

    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_input_indices;
    std::unique_ptr<TileStorageTexture> m_output_storage_texture; // stores output of downsampling before it is copied back to input hashmap
};

} // namespace webgpu_compute::nodes
