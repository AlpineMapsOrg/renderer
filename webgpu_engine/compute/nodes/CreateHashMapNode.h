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

namespace webgpu_engine::compute::nodes {

class CreateHashMapNode : public Node {
    Q_OBJECT

public:
    CreateHashMapNode(WGPUDevice device, const glm::uvec2& resolution, size_t capacity, WGPUTextureFormat format);

public slots:
    void run_impl() override;

private:
    WGPUDevice m_device;
    WGPUQueue m_queue;
    GpuHashMap<radix::tile::Id, uint32_t, GpuTileId> m_output_tile_id_to_index; // for looking up index for tile id
    TileStorageTexture m_output_tile_textures; // height texture per tile
};

} // namespace webgpu_engine::compute::nodes
