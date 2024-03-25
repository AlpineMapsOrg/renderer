 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2023 Adam Celarek
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
#include "TileManager.h"

#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

#include "ShaderProgram.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/utils/terrain_mesh_index_generator.h"

using gl_engine::TileManager;
using gl_engine::TileSet;

namespace {
template <typename T>
int bufferLengthInBytes(const std::vector<T>& vec)
{
    return int(vec.size() * sizeof(T));
}
}

TileManager::TileManager(QObject* parent)
    : QObject { parent }
{
}

void TileManager::init()
{
    using nucleus::utils::terrain_mesh_index_generator::surface_quads_with_curtains;
    assert(QOpenGLContext::currentContext());
    const auto indices = surface_quads_with_curtains<uint16_t>(N_EDGE_VERTICES);
    auto index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    index_buffer->create();
    index_buffer->bind();
    index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    index_buffer->allocate(indices.data(), bufferLengthInBytes(indices));
    index_buffer->release();
    m_index_buffer.first = std::move(index_buffer);
    m_index_buffer.second = indices.size();

    m_bounds_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_bounds_buffer->create();
    m_bounds_buffer->bind();
    m_bounds_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_bounds_buffer->allocate(GLsizei(m_loaded_tiles.size() * sizeof(glm::vec4)));

    m_tileset_id_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_tileset_id_buffer->create();
    m_tileset_id_buffer->bind();
    m_tileset_id_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_tileset_id_buffer->allocate(GLsizei(m_loaded_tiles.size() * sizeof(int32_t)));

    m_zoom_level_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_zoom_level_buffer->create();
    m_zoom_level_buffer->bind();
    m_zoom_level_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_zoom_level_buffer->allocate(GLsizei(m_loaded_tiles.size() * sizeof(int32_t)));

    m_texture_layer_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_texture_layer_buffer->create();
    m_texture_layer_buffer->bind();
    m_texture_layer_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_texture_layer_buffer->allocate(GLsizei(m_loaded_tiles.size() * sizeof(int32_t)));

    m_vao = std::make_unique<QOpenGLVertexArrayObject>();
    m_vao->create();
    m_vao->bind();
    m_index_buffer.first->bind();
    m_vao->release();

    m_ortho_textures = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::CompressedRGBA8);
    m_ortho_textures->setParams(Texture::Filter::Linear, Texture::Filter::Linear);
    // TODO: might become larger than GL_MAX_ARRAY_TEXTURE_LAYERS
    m_ortho_textures->allocate_array(ORTHO_RESOLUTION, ORTHO_RESOLUTION, unsigned(m_loaded_tiles.size()));

    m_heightmap_textures = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::R16UI);
    m_heightmap_textures->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);
    m_heightmap_textures->allocate_array(HEIGHTMAP_RESOLUTION, HEIGHTMAP_RESOLUTION, unsigned(m_loaded_tiles.size()));
}

bool compareTileSetPair(std::pair<float, const TileSet*> t1, std::pair<float, const TileSet*> t2)
{
    return (t1.first < t2.first);
}

const nucleus::tile_scheduler::DrawListGenerator::TileSet TileManager::generate_tilelist(const nucleus::camera::Definition& camera) const {
    return m_draw_list_generator.generate_for(camera);
}

const nucleus::tile_scheduler::DrawListGenerator::TileSet TileManager::cull(const nucleus::tile_scheduler::DrawListGenerator::TileSet& tileset, const nucleus::camera::Frustum& frustum) const {
    return m_draw_list_generator.cull(tileset, frustum);
}

void TileManager::draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera,
    const nucleus::tile_scheduler::DrawListGenerator::TileSet& draw_tiles, bool sort_tiles, glm::dvec3 sort_position) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    shader_program->set_uniform("n_edge_vertices", N_EDGE_VERTICES);
    shader_program->set_uniform("ortho_sampler", 2);
    shader_program->set_uniform("height_sampler", 1);

    // Sort depending on distance to sort_position
    std::vector<std::pair<float, const TileSet*>> tile_list;
    for (const auto& tileset : m_gpu_tiles) {
        float dist = 0.0;
        if (!draw_tiles.contains(tileset.tile_id))
            continue;
        if (sort_tiles) {
            glm::vec2 pos_wrt = glm::vec2(tileset.bounds.min.x - sort_position.x, tileset.bounds.min.y - sort_position.y);
            dist = glm::length(pos_wrt);
        }
        tile_list.push_back(std::pair<float, const TileSet*>(dist, &tileset));
    }
    if (sort_tiles) std::sort(tile_list.begin(), tile_list.end(), compareTileSetPair);

    m_ortho_textures->bind(2);
    m_heightmap_textures->bind(1);
    m_vao->bind();

    std::vector<glm::vec4> bounds;
    bounds.reserve(tile_list.size());

    std::vector<int32_t> tileset_id;
    tileset_id.reserve(tile_list.size());

    std::vector<int32_t> zoom_level;
    zoom_level.reserve(tile_list.size());

    std::vector<int32_t> texture_layer;
    texture_layer.reserve(tile_list.size());

    for (const auto& tileset : tile_list) {
        bounds.emplace_back(tileset.second->bounds.min.x - camera.position().x, tileset.second->bounds.min.y - camera.position().y,
            tileset.second->bounds.max.x - camera.position().x, tileset.second->bounds.max.y - camera.position().y);

        tileset_id.emplace_back(tileset.second->tile_id.coords[0] + tileset.second->tile_id.coords[1]);
        zoom_level.emplace_back(tileset.second->tile_id.zoom_level);
        texture_layer.emplace_back(tileset.second->texture_layer);
    }

    m_bounds_buffer->bind();
    m_bounds_buffer->write(0, bounds.data(), GLsizei(bounds.size() * sizeof(decltype(bounds)::value_type)));

    m_tileset_id_buffer->bind();
    m_tileset_id_buffer->write(0, tileset_id.data(), GLsizei(tileset_id.size() * sizeof(decltype(tileset_id)::value_type)));

    m_zoom_level_buffer->bind();
    m_zoom_level_buffer->write(0, zoom_level.data(), GLsizei(zoom_level.size() * sizeof(decltype(zoom_level)::value_type)));

    m_texture_layer_buffer->bind();
    m_texture_layer_buffer->write(0, texture_layer.data(), GLsizei(texture_layer.size() * sizeof(decltype(texture_layer)::value_type)));

    f->glDrawElementsInstanced(GL_TRIANGLE_STRIP, GLsizei(m_index_buffer.second), GL_UNSIGNED_SHORT, nullptr, GLsizei(tile_list.size()));
    f->glBindVertexArray(0);
}

void TileManager::remove_tile(const tile::Id& tile_id)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    const auto t = std::find(m_loaded_tiles.begin(), m_loaded_tiles.end(), tile_id);
    assert(t != m_loaded_tiles.end()); // removing a tile that's not here. likely there is a race.
    *t = tile::Id { unsigned(-1), {} };
    m_draw_list_generator.remove_tile(tile_id);

    // clear slot
    // or remove from list and free resources
    const auto found_tile = std::find_if(m_gpu_tiles.begin(), m_gpu_tiles.end(), [&tile_id](const TileSet& tileset) { return tileset.tile_id == tile_id; });
    if (found_tile != m_gpu_tiles.end())
        m_gpu_tiles.erase(found_tile);

    emit tiles_changed();
}

void TileManager::initilise_attribute_locations(ShaderProgram* program)
{
    int bounds = program->attribute_location("bounds");
    qDebug() << "attrib location for bounds: " << bounds;
    int tileset_id = program->attribute_location("tileset_id");
    qDebug() << "attrib location for tileset_id: " << tileset_id;
    int zoom_level = program->attribute_location("tileset_zoomlevel");
    qDebug() << "attrib location for zoom_level: " << zoom_level;
    int texture_layer = program->attribute_location("texture_layer");
    qDebug() << "attrib location for texture_layer: " << texture_layer;

    m_vao->bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    if (bounds != -1) {
        m_bounds_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(bounds));
        f->glVertexAttribPointer(GLuint(bounds), /*size*/ 4, /*type*/ GL_FLOAT, /*normalised*/ GL_FALSE, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(bounds), 1);
    }
    if (tileset_id != -1) {
        m_tileset_id_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(tileset_id));
        f->glVertexAttribIPointer(GLuint(tileset_id), /*size*/ 1, /*type*/ GL_INT, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(tileset_id), 1);
    }
    if (zoom_level != -1) {
        m_zoom_level_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(zoom_level));
        f->glVertexAttribIPointer(GLuint(zoom_level), /*size*/ 1, /*type*/ GL_INT, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(zoom_level), 1);
    }
    if (texture_layer != -1) {
        m_texture_layer_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(texture_layer));
        f->glVertexAttribIPointer(GLuint(texture_layer), /*size*/ 1, /*type*/ GL_INT, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(texture_layer), 1);
    }
}

void TileManager::set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr& new_aabb_decorator)
{
    m_draw_list_generator.set_aabb_decorator(new_aabb_decorator);
}

void TileManager::set_quad_limit(unsigned int new_limit)
{
    m_loaded_tiles.resize(new_limit * 4);
    std::fill(m_loaded_tiles.begin(), m_loaded_tiles.end(), tile::Id { unsigned(-1), {} });
}

void TileManager::add_tile(
    const tile::Id& id, tile::SrsAndHeightBounds bounds, const nucleus::utils::ColourTexture& ortho_texture, const nucleus::Raster<uint16_t>& height_map)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    TileSet tileset;
    tileset.tile_id = id;
    tileset.bounds = tile::SrsBounds(bounds);

    // find empty spot and upload texture
    const auto t = std::find(m_loaded_tiles.begin(), m_loaded_tiles.end(), tile::Id { unsigned(-1), {} });
    assert(t != m_loaded_tiles.end());
    *t = id;
    const auto layer_index = unsigned(t - m_loaded_tiles.begin());
    tileset.texture_layer = layer_index;
    m_ortho_textures->upload(ortho_texture, layer_index);
    m_heightmap_textures->upload(height_map, layer_index);

    // add to m_gpu_tiles
    m_gpu_tiles.push_back(tileset);
    m_draw_list_generator.add_tile(id);

    emit tiles_changed();
}

void TileManager::set_permissible_screen_space_error(float new_permissible_screen_space_error)
{
    m_draw_list_generator.set_permissible_screen_space_error(new_permissible_screen_space_error);
}

void TileManager::update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    for (const auto& quad : deleted_quads) {
        for (const auto& id : quad.children()) {
            remove_tile(id);
        }
    }
    for (const auto& quad : new_quads) {
        for (const auto& tile : quad.tiles) {
            // test for validity
            assert(tile.id.zoom_level < 100);
            assert(tile.height);
            assert(tile.ortho);
            add_tile(tile.id, tile.bounds, *tile.ortho, *tile.height);
        }
    }
}
