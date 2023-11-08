 /*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celerek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include <nucleus/tile_scheduler/tile_types.h>

#include "gl_engine/TileSet.h"
#include "nucleus/Tile.h"
#include "nucleus/tile_scheduler/DrawListGenerator.h"

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

    [[nodiscard]] const std::vector<TileSet>& tiles() const;
    void draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera, const nucleus::tile_scheduler::DrawListGenerator::TileSet draw_tiles, bool sort_tiles, glm::dvec3 sort_position) const;

    const nucleus::tile_scheduler::DrawListGenerator::TileSet generate_tilelist(const nucleus::camera::Definition& camera) const;

    void set_permissible_screen_space_error(float new_permissible_screen_space_error);

signals:
    void tiles_changed();

public slots:
    void update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);
    void remove_tile(const tile::Id& tile_id);
    void initilise_attribute_locations(ShaderProgram* program);
    void set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr& new_aabb_decorator);

private:
    void add_tile(const tile::Id& id, tile::SrsAndHeightBounds bounds, const QImage& ortho, const nucleus::Raster<uint16_t>& heights, const QImage& height_texture);
    struct TileGLAttributeLocations {
        int height = -1;
    };

    static constexpr auto N_EDGE_VERTICES = 65;
    static constexpr auto MAX_TILES_PER_TILESET = 1;
    float m_max_anisotropy = 0;

    std::vector<TileSet> m_gpu_tiles;
    // indexbuffers for 4^index tiles,
    // e.g., for single tile tile sets take index 0
    //       for 4 tiles take index 1, for 16 2..
    // the size_t is the number of indices
    std::vector<std::pair<std::unique_ptr<QOpenGLBuffer>, size_t>> m_index_buffers;
    TileGLAttributeLocations m_attribute_locations;
    unsigned m_tiles_per_set = 1;
    nucleus::tile_scheduler::DrawListGenerator m_draw_list_generator;
    const nucleus::tile_scheduler::DrawListGenerator::TileSet m_last_draw_list; // buffer last generated draw list
};
}
