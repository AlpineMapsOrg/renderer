/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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

#include <QObject>
#include <nucleus/Raster.h>
#include <nucleus/tile/DrawListGenerator.h>
#include <nucleus/tile/GpuArrayHelper.h>
#include <nucleus/tile/types.h>

namespace camera {
class Definition;
}

class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;

namespace gl_engine {
class ShaderRegistry;
class ShaderProgram;
class Texture;

class TileGeometry : public QObject {
    Q_OBJECT

    struct TileInfo {
        nucleus::tile::Id tile_id = {};
        nucleus::tile::SrsBounds bounds = {};
        unsigned height_texture_layer = unsigned(-1);
    };

public:
    using TileSet = std::unordered_set<nucleus::tile::Id, nucleus::tile::Id::Hasher>;

    explicit TileGeometry(QObject* parent = nullptr);
    void init(); // needs OpenGL context
    void draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera, const TileSet& draw_tiles, bool sort_tiles, glm::dvec3 sort_position) const;

    const TileSet generate_tilelist(const nucleus::camera::Definition& camera) const;
    const TileSet cull(const TileSet& tileset, const nucleus::camera::Frustum& frustum) const;

    void set_permissible_screen_space_error(float new_permissible_screen_space_error);

    unsigned int tile_count() const;

public slots:
    void update_gpu_quads(const std::vector<nucleus::tile::GpuGeometryQuad>& new_quads, const std::vector<nucleus::tile::Id>& deleted_quads);
    void set_aabb_decorator(const nucleus::tile::utils::AabbDecoratorPtr& new_aabb_decorator);
    void set_quad_limit(unsigned new_limit);

private:
    void remove_tile(const nucleus::tile::Id& tile_id);
    void add_tile(const nucleus::tile::Id& id, nucleus::tile::SrsAndHeightBounds bounds, const nucleus::Raster<uint16_t>& heights);
    void update_gpu_id_map();

    static constexpr auto N_EDGE_VERTICES = 65;
    static constexpr auto HEIGHTMAP_RESOLUTION = 65;

    std::unique_ptr<Texture> m_heightmap_textures;
    std::unique_ptr<Texture> m_tile_id_texture;
    std::unique_ptr<Texture> m_array_index_texture;
    std::unique_ptr<QOpenGLVertexArrayObject> m_vao;
    std::pair<std::unique_ptr<QOpenGLBuffer>, size_t> m_index_buffer;
    std::unique_ptr<QOpenGLBuffer> m_bounds_buffer;
    std::unique_ptr<QOpenGLBuffer> m_draw_tile_id_buffer;
    std::unique_ptr<QOpenGLBuffer> m_height_texture_layer_buffer;

    std::vector<TileInfo> m_gpu_tiles;
    nucleus::tile::DrawListGenerator m_draw_list_generator;
    nucleus::tile::GpuArrayHelper m_gpu_array_helper;
};
} // namespace gl_engine
