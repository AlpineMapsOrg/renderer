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

#include "UniformBuffer.h"
#include <QObject>
#include <nucleus/Raster.h>
#include <nucleus/tile/DrawListGenerator.h>
#include <nucleus/tile/GpuArrayHelper.h>
#include <nucleus/tile/types.h>

namespace nucleus::camera {
class Definition;
}

class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;

namespace gl_engine {
class ShaderRegistry;
class ShaderProgram;
class Texture;
class TileGeometry;

class TextureLayer : public QObject {
    Q_OBJECT
public:
    explicit TextureLayer(unsigned resolution = 256, QObject* parent = nullptr);
    void init(ShaderRegistry* shader_registry); // needs OpenGL context
    void draw(const TileGeometry& tile_geometry, const nucleus::camera::Definition& camera, const std::vector<nucleus::tile::TileBounds>& draw_list) const;

    unsigned int tile_count() const;

public slots:
    void update_gpu_tiles(const std::vector<nucleus::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuTextureTile>& new_tiles);

    /// must be called before init!
    void set_tile_limit(unsigned new_limit);

private:
    const unsigned m_resolution = 256u;

    std::shared_ptr<ShaderProgram> m_shader;
    std::unique_ptr<Texture> m_texture_array;
    std::unique_ptr<Texture> m_instanced_zoom;
    std::unique_ptr<Texture> m_instanced_array_index;
    nucleus::tile::GpuArrayHelper m_gpu_array_helper;
};
} // namespace gl_engine
