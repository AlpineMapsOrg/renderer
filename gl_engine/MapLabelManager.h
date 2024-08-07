/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <unordered_map>

#include "Framebuffer.h"
#include "Texture.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/map_label/LabelFactory.h"
#include "nucleus/tile_scheduler/tile_types.h"

#include "nucleus/tile_scheduler/DrawListGenerator.h"

namespace camera {
class Definition;
}

namespace gl_engine {
class ShaderProgram;

struct GPUVectorTile {
    tile::Id id;
    std::unique_ptr<QOpenGLBuffer> vertex_buffer;
    std::unique_ptr<QOpenGLVertexArrayObject> vao;
    size_t instance_count; // how many characters (+1 for icon)
};

class MapLabelManager : public QObject {
    Q_OBJECT
public:
    explicit MapLabelManager(QObject* parent = nullptr);

    void init();
    void draw(Framebuffer* gbuffer, ShaderProgram* shader_program, const nucleus::camera::Definition& camera,
        const nucleus::tile_scheduler::DrawListGenerator::TileSet draw_tiles) const;

    void update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);

    void remove_tile(const tile::Id& tile_id);

private:
    void add_tile(const tile::Id& id, const nucleus::vectortile::FeatureType& type, const nucleus::vectortile::VectorTile& vector_tile);

    std::unique_ptr<Texture> m_font_texture;
    std::unordered_map<nucleus::vectortile::FeatureType, std::unique_ptr<Texture>> m_icon_texture;

    std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_map<tile::Id, std::shared_ptr<GPUVectorTile>, tile::Id::Hasher>> m_gpu_tiles;

    std::unique_ptr<QOpenGLBuffer> m_index_buffer;
    size_t m_indices_count; // how many vertices per character (most likely 6 since quads)

    nucleus::maplabel::LabelFactory m_mapLabelFactory;
};
} // namespace gl_engine
