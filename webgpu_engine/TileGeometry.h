/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Adam Celerek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include <memory>

#include "Buffer.h"
#include "PipelineManager.h"
#include "webgpu_engine/compute/GpuTileId.h"
#include <QObject>
#include <nucleus/tile/GpuArrayHelper.h>
#include <nucleus/tile/types.h>
#include <webgpu/raii/BindGroup.h>
#include <webgpu/raii/BindGroupLayout.h>
#include <webgpu/raii/TextureWithSampler.h>
#include <webgpu/webgpu.h>

namespace nucleus::camera {
class Definition;
}

namespace webgpu_engine {

class TileGeometry : public QObject {
    Q_OBJECT
public:
    explicit TileGeometry(uint32_t height_resolution, uint32_t ortho_resolution);

    void init(WGPUDevice device);

    void draw(WGPURenderPassEncoder render_pass, const nucleus::camera::Definition& camera, const std::vector<nucleus::tile::TileBounds>& draw_tiles) const;

    void set_pipeline_manager(const PipelineManager& pipeline_manager);

    std::unique_ptr<webgpu::raii::BindGroup> create_bind_group(const webgpu::raii::TextureView& view, const webgpu::raii::Sampler& sampler) const;

    size_t capacity() const;
    void set_tile_limit(unsigned new_limit);

signals:
    void tiles_changed();

public slots:
    void update_gpu_tiles_height(const std::vector<radix::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuGeometryTile>& new_tiles);
    void update_gpu_tiles_ortho(const std::vector<nucleus::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuTextureTile>& new_tiles);

private:
    uint32_t m_height_resolution;
    uint32_t m_ortho_resolution;
    size_t m_num_layers;
    nucleus::tile::GpuArrayHelper m_loaded_height_textures;
    nucleus::tile::GpuArrayHelper m_loaded_ortho_textures;

    WGPUDevice m_device = 0;
    WGPUQueue m_queue = 0;
    const PipelineManager* m_pipeline_manager = nullptr;

    size_t m_index_buffer_size;
    std::unique_ptr<webgpu::raii::RawBuffer<uint16_t>> m_index_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<glm::vec4>> m_bounds_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<int32_t>> m_tileset_id_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<int32_t>> m_height_zoom_level_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<int32_t>> m_height_texture_layer_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<int32_t>> m_ortho_zoom_level_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<int32_t>> m_ortho_texture_layer_buffer;
    std::unique_ptr<Buffer<int32_t>> m_n_edge_vertices_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<compute::GpuTileId>> m_tile_id_buffer;

    std::unique_ptr<webgpu::raii::TextureWithSampler> m_heightmap_textures;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_ortho_textures;
    std::unique_ptr<webgpu::raii::BindGroup> m_tile_bind_group;
};

} // namespace webgpu_engine
