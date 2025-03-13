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

public:
    explicit TileGeometry(unsigned texture_resolution = 65);
    void init(); // needs OpenGL context
    void draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera, const std::vector<nucleus::tile::TileBounds>& draw_list) const;

    unsigned int tile_count() const;

public slots:
    void update_gpu_tiles(const std::vector<nucleus::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuGeometryTile>& new_tiles);
    void set_aabb_decorator(const nucleus::tile::utils::AabbDecoratorPtr& new_aabb_decorator);
    /// must be called before init
    void set_tile_limit(unsigned new_limit);

private:
    const unsigned m_texture_resolution;

    std::unique_ptr<Texture> m_dtm_textures;
    std::unique_ptr<QOpenGLVertexArrayObject> m_vao;
    std::pair<std::unique_ptr<QOpenGLBuffer>, size_t> m_index_buffer;
    std::unique_ptr<QOpenGLBuffer> m_instance_bounds_buffer;
    std::unique_ptr<QOpenGLBuffer> m_instance_tile_id_buffer;
    std::unique_ptr<QOpenGLBuffer> m_dtm_array_index_buffer;
    std::unique_ptr<QOpenGLBuffer> m_dtm_zoom_buffer;

    nucleus::tile::GpuArrayHelper m_gpu_array_helper;
    nucleus::tile::utils::AabbDecoratorPtr m_aabb_decorator;
};
} // namespace gl_engine
