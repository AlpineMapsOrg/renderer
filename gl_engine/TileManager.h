/*****************************************************************************
 * Alpine Terrain Renderer
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

#include <QObject>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include "gl_engine/Texture.h"
#include "gl_engine/TileSet.h"
#include <nucleus/Tile.h>
#include <nucleus/tile_scheduler/DrawListGenerator.h>
#include <nucleus/tile_scheduler/tile_types.h>

namespace camera {
class Definition;
}

class QOpenGLShaderProgram;

namespace gl_engine {
class Atmosphere;
class ShaderProgram;

class TileManager : public QObject {
    Q_OBJECT
public:
    explicit TileManager(QObject* parent = nullptr);
    void init(); // needs OpenGL context
    void draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera, const nucleus::tile_scheduler::DrawListGenerator::TileSet& draw_tiles, bool sort_tiles, glm::dvec3 sort_position) const;

    const nucleus::tile_scheduler::DrawListGenerator::TileSet generate_tilelist(const nucleus::camera::Definition& camera) const;
    const nucleus::tile_scheduler::DrawListGenerator::TileSet cull(const nucleus::tile_scheduler::DrawListGenerator::TileSet& tileset, const nucleus::camera::Frustum& frustum) const;

    void set_permissible_screen_space_error(float new_permissible_screen_space_error);

signals:
    void tiles_changed();

public slots:
    void update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);
    void remove_tile(const tile::Id& tile_id);
    void initilise_attribute_locations(ShaderProgram* program);
    void set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr& new_aabb_decorator);
    void set_quad_limit(unsigned new_limit);

private:
    void add_tile(const tile::Id& id, tile::SrsAndHeightBounds bounds, const nucleus::utils::ColourTexture& ortho, const nucleus::Raster<uint16_t>& heights);

    static constexpr auto N_EDGE_VERTICES = 65;
    static constexpr auto ORTHO_RESOLUTION = 256;
    static constexpr auto HEIGHTMAP_RESOLUTION = 65;

    std::vector<tile::Id> m_loaded_tiles;
    std::unique_ptr<Texture> m_ortho_textures;
    std::unique_ptr<Texture> m_heightmap_textures;
    std::unique_ptr<QOpenGLVertexArrayObject> m_vao;
    std::pair<std::unique_ptr<QOpenGLBuffer>, size_t> m_index_buffer;
    std::unique_ptr<QOpenGLBuffer> m_bounds_buffer;
    std::unique_ptr<QOpenGLBuffer> m_tileset_id_buffer;
    std::unique_ptr<QOpenGLBuffer> m_zoom_level_buffer;
    std::unique_ptr<QOpenGLBuffer> m_texture_layer_buffer;

    std::vector<TileSet> m_gpu_tiles;
    unsigned m_tiles_per_set = 1;
    nucleus::tile_scheduler::DrawListGenerator m_draw_list_generator;
    const nucleus::tile_scheduler::DrawListGenerator::TileSet m_last_draw_list; // buffer last generated draw list
};
}
