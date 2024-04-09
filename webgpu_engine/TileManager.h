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

#include "TileSet.h"
#include "raii/BindGroupWithLayout.h"
#include "raii/Buffer.h"
#include "raii/TextureWithSampler.h"
#include "util/BindGroupWithLayoutInfo.h"
#include <QObject>
#include <nucleus/Tile.h>
#include <nucleus/tile_scheduler/DrawListGenerator.h>
#include <nucleus/tile_scheduler/tile_types.h>
#include <webgpu/webgpu.h>

namespace camera {
class Definition;
}

namespace webgpu_engine {

class Atmosphere;
class ShaderProgram;

/// Abstract base class for GPU specific parts of tile management
/// Interface supports writing tiles and drawing tiles
class TileRenderer {
public:
    virtual ~TileRenderer() = default;
    virtual void init(glm::uvec2 height_resolution, glm::uvec2 ortho_resolution, size_t num_layers, size_t n_edge_vertices) = 0;
    virtual void write_tile(const nucleus::utils::ColourTexture& ortho_texture, const nucleus::Raster<uint16_t>& height_map, size_t layer) = 0;
    virtual void draw(WGPURenderPipeline render_pipeline, WGPURenderPassEncoder render_pass, const nucleus::camera::Definition& camera,
        const std::vector<const TileSet*>& tile_list)
        = 0;

    virtual const raii::BindGroupWithLayout& tile_bind_group() const = 0; // TODO remove from interface, this is an implementation detail
};

/// Draws tiles by instancing with a single draw call.
/// Stores heightmaps and ortho photos for all tiles in a single 2d texture array each.
class TileRendererInstancedSingleArray : public TileRenderer {
public:
    TileRendererInstancedSingleArray(WGPUDevice device, WGPUQueue queue);
    void init(glm::uvec2 height_resolution, glm::uvec2 ortho_resolution, size_t num_layers, size_t n_edge_vertices) override;
    void write_tile(const nucleus::utils::ColourTexture& ortho_texture, const nucleus::Raster<uint16_t>& height_map, size_t layer) override;
    void draw(WGPURenderPipeline render_pipeline, WGPURenderPassEncoder render_pass, const nucleus::camera::Definition& camera,
        const std::vector<const TileSet*>& tile_list) override;

    virtual const raii::BindGroupWithLayout& tile_bind_group() const override;

private:
    size_t m_index_buffer_size;
    std::unique_ptr<raii::RawBuffer<uint16_t>> m_index_buffer;
    std::unique_ptr<raii::RawBuffer<glm::vec4>> m_bounds_buffer;
    std::unique_ptr<raii::RawBuffer<int32_t>> m_tileset_id_buffer;
    std::unique_ptr<raii::RawBuffer<int32_t>> m_zoom_level_buffer;
    std::unique_ptr<raii::RawBuffer<int32_t>> m_texture_layer_buffer;
    std::unique_ptr<raii::Buffer<int32_t>> m_n_edge_vertices_buffer;

    std::unique_ptr<raii::TextureWithSampler> m_ortho_textures;
    std::unique_ptr<raii::TextureWithSampler> m_heightmap_textures;

    util::BindGroupWithLayoutInfo m_tile_bind_group_info;
    std::unique_ptr<raii::BindGroupWithLayout> m_tile_bind_group;

    WGPUDevice m_device = 0;
    WGPUQueue m_queue = 0;
};

/// Stores ortho photos and heightmaps in multiple texture arrays.
/// This is useful, if the number of elements in a texture array is limited.
/// Draws tiles by instancing but uses multiple draw calls (one for each texture array).
class TileRendererInstancedSingleArrayMultiCall : public TileRenderer {
public:
    // uses device limit for number of array layers
    TileRendererInstancedSingleArrayMultiCall(WGPUDevice device, WGPUQueue queue);
    TileRendererInstancedSingleArrayMultiCall(WGPUDevice device, WGPUQueue queue, size_t num_layers_per_texture);

    void init(glm::uvec2 height_resolution, glm::uvec2 ortho_resolution, size_t num_layers, size_t n_edge_vertices) override;
    void write_tile(const nucleus::utils::ColourTexture& ortho_texture, const nucleus::Raster<uint16_t>& height_map, size_t layer) override;
    void draw(WGPURenderPipeline render_pipeline, WGPURenderPassEncoder render_pass, const nucleus::camera::Definition& camera,
        const std::vector<const TileSet*>& tile_list) override;

    virtual const raii::BindGroupWithLayout& tile_bind_group() const override;

private:
    size_t m_index_buffer_size;
    std::unique_ptr<raii::RawBuffer<uint16_t>> m_index_buffer;
    std::unique_ptr<raii::RawBuffer<glm::vec4>> m_bounds_buffer;
    std::unique_ptr<raii::RawBuffer<int32_t>> m_tileset_id_buffer;
    std::unique_ptr<raii::RawBuffer<int32_t>> m_zoom_level_buffer;
    std::unique_ptr<raii::RawBuffer<int32_t>> m_texture_layer_buffer;
    std::unique_ptr<raii::Buffer<int32_t>> m_n_edge_vertices_buffer;

    std::vector<std::unique_ptr<raii::TextureWithSampler>> m_ortho_textures;
    std::vector<std::unique_ptr<raii::TextureWithSampler>> m_heightmap_textures;
    std::vector<std::unique_ptr<raii::BindGroupWithLayout>> m_tile_bind_group;

    WGPUDevice m_device = 0;
    WGPUQueue m_queue = 0;

    size_t m_num_layers_per_texture;
};

class TileManager : public QObject {
    Q_OBJECT
public:
    explicit TileManager(QObject* parent = nullptr);
    void init(WGPUDevice device, WGPUQueue queue); // needs OpenGL context
    [[nodiscard]] const std::vector<TileSet>& tiles() const;
    void draw(WGPURenderPipeline render_pipeline, WGPURenderPassEncoder render_pass, const nucleus::camera::Definition& camera,
        const nucleus::tile_scheduler::DrawListGenerator::TileSet& draw_tiles, bool sort_tiles, glm::dvec3 sort_position) const;

    const nucleus::tile_scheduler::DrawListGenerator::TileSet generate_tilelist(const nucleus::camera::Definition& camera) const;
    const nucleus::tile_scheduler::DrawListGenerator::TileSet cull(const nucleus::tile_scheduler::DrawListGenerator::TileSet& tileset, const nucleus::camera::Frustum& frustum) const;

    void set_permissible_screen_space_error(float new_permissible_screen_space_error);

    const raii::BindGroupWithLayout& tile_bind_group() const;

signals:
    void tiles_changed();

public slots:
    void update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);
    void remove_tile(const tile::Id& tile_id);
    void set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr& new_aabb_decorator);
    void set_quad_limit(unsigned new_limit);

private:
    void add_tile(const tile::Id& id, tile::SrsAndHeightBounds bounds, const nucleus::utils::ColourTexture& ortho, const nucleus::Raster<uint16_t>& heights);

    static constexpr auto N_EDGE_VERTICES = 65;
    static constexpr auto ORTHO_RESOLUTION = 256;
    static constexpr auto HEIGHTMAP_RESOLUTION = 65;

    std::vector<tile::Id> m_loaded_tiles;

    std::vector<TileSet> m_gpu_tiles;
    unsigned m_tiles_per_set = 1;
    nucleus::tile_scheduler::DrawListGenerator m_draw_list_generator;
    const nucleus::tile_scheduler::DrawListGenerator::TileSet m_last_draw_list; // buffer last generated draw list

    std::unique_ptr<TileRenderer> m_renderer;
};

} // namespace webgpu_engine
