/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Adam Celerek
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
#include "nucleus/map_label/Factory.h"
#include "nucleus/map_label/FilterDefinitions.h"

#include "nucleus/tile/DrawListGenerator.h"

using namespace nucleus::vector_tile;

namespace gl_engine {
class ShaderProgram;
class ShaderRegistry;

struct GPUVectorTile {
    nucleus::tile::Id id;
    std::unique_ptr<QOpenGLBuffer> vertex_buffer;
    std::unique_ptr<QOpenGLVertexArrayObject> vao;
    size_t instance_count; // how many characters (+1 for icon)
    glm::dvec3 reference_point = {};
};

class MapLabels : public QObject {
    Q_OBJECT

public:
    using TileSet = nucleus::tile::DrawListGenerator::TileSet;
    using TileId = nucleus::tile::Id;
    explicit MapLabels(const nucleus::tile::utils::AabbDecoratorPtr& aabb_decorator, QObject* parent = nullptr);

    void init(ShaderRegistry* shader_registry);
    void draw(Framebuffer* gbuffer, const nucleus::camera::Definition& camera, const TileSet& draw_tiles) const;
    void draw_picker(Framebuffer* gbuffer, const nucleus::camera::Definition& camera, const TileSet& draw_tiles) const;
    TileSet generate_draw_list(const nucleus::camera::Definition& camera) const;

    void update_labels(const std::vector<nucleus::vector_tile::PoiTile>& updated_tiles, const std::vector<TileId>& removed_tiles);

    unsigned int tile_count() const;

private:
    void upload_to_gpu(const TileId& id, const PointOfInterestCollection& features);
    void remove_tile(const TileId& tile_id);

    std::shared_ptr<ShaderProgram> m_label_shader;
    std::shared_ptr<ShaderProgram> m_picker_shader;

    std::unique_ptr<Texture> m_font_texture;
    std::unique_ptr<Texture> m_icon_texture;

    std::unique_ptr<QOpenGLBuffer> m_index_buffer;
    size_t m_indices_count; // how many vertices per character (most likely 6 since quads)

    nucleus::map_label::Factory m_mapLabelFactory;

    nucleus::tile::DrawListGenerator m_draw_list_generator;
    std::unordered_map<TileId, std::shared_ptr<GPUVectorTile>, TileId::Hasher> m_gpu_tiles;
};
} // namespace gl_engine
