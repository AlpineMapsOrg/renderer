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

 namespace {
 template <typename T> int bufferLengthInBytes(const std::vector<T>& vec) { return int(vec.size() * sizeof(T)); }
 } // namespace

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

    m_draw_tile_id_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_draw_tile_id_buffer->create();
    m_draw_tile_id_buffer->bind();
    m_draw_tile_id_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_draw_tile_id_buffer->allocate(GLsizei(m_loaded_tiles.size() * sizeof(glm::u32vec2)));

    m_height_texture_layer_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_height_texture_layer_buffer->create();
    m_height_texture_layer_buffer->bind();
    m_height_texture_layer_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_height_texture_layer_buffer->allocate(GLsizei(m_loaded_tiles.size() * sizeof(int32_t)));

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

    m_tile_id_map_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::RG32UI);
    m_tile_id_map_texture->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    m_texture_id_map_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R16UI);
    m_texture_id_map_texture->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);
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
    shader_program->set_uniform("height_texture_layer_map_sampler", 3);
    shader_program->set_uniform("tile_id_map_sampler", 4);

    // Sort depending on distance to sort_position
    std::vector<std::pair<float, const TileInfo*>> tile_list;
    for (const auto& tileset : m_gpu_tiles) {
        float dist = 0.0;
        if (!draw_tiles.contains(tileset.tile_id))
            continue;
        if (sort_tiles) {
            glm::vec2 pos_wrt = glm::vec2(tileset.bounds.min.x - sort_position.x, tileset.bounds.min.y - sort_position.y);
            dist = glm::length(pos_wrt);
        }
        tile_list.push_back(std::pair<float, const TileInfo*>(dist, &tileset));
    }
    if (sort_tiles)
        std::sort(tile_list.begin(), tile_list.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    m_ortho_textures->bind(2);
    m_heightmap_textures->bind(1);
    m_texture_id_map_texture->bind(3);
    m_tile_id_map_texture->bind(4);
    m_vao->bind();

    std::vector<glm::vec4> bounds;
    bounds.reserve(tile_list.size());

    std::vector<glm::u32vec2> packed_id;
    packed_id.reserve(tile_list.size());

    std::vector<int32_t> height_texture_layer;
    height_texture_layer.reserve(tile_list.size());

    for (const auto& tileset : tile_list) {
        bounds.emplace_back(tileset.second->bounds.min.x - camera.position().x, tileset.second->bounds.min.y - camera.position().y,
            tileset.second->bounds.max.x - camera.position().x, tileset.second->bounds.max.y - camera.position().y);

        packed_id.emplace_back(nucleus::srs::pack(tileset.second->tile_id));
        height_texture_layer.emplace_back(tileset.second->height_texture_layer);
    }

    m_bounds_buffer->bind();
    m_bounds_buffer->write(0, bounds.data(), GLsizei(bounds.size() * sizeof(decltype(bounds)::value_type)));

    m_draw_tile_id_buffer->bind();
    m_draw_tile_id_buffer->write(0, packed_id.data(), GLsizei(packed_id.size() * sizeof(decltype(packed_id)::value_type)));

    m_height_texture_layer_buffer->bind();
    m_height_texture_layer_buffer->write(
        0, height_texture_layer.data(), GLsizei(height_texture_layer.size() * sizeof(decltype(height_texture_layer)::value_type)));

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
    const auto found_tile = std::find_if(m_gpu_tiles.begin(), m_gpu_tiles.end(), [&tile_id](const TileInfo& tileset) { return tileset.tile_id == tile_id; });
    if (found_tile != m_gpu_tiles.end())
        m_gpu_tiles.erase(found_tile);
}

void TileManager::initilise_attribute_locations(ShaderProgram* program)
{
    int bounds = program->attribute_location("bounds");
    qDebug() << "attrib location for bounds: " << bounds;
    int packed_tile_id = program->attribute_location("packed_tile_id");
    qDebug() << "attrib location for packed_tile_id: " << packed_tile_id;
    int height_texture_layer = program->attribute_location("height_texture_layer");
    qDebug() << "attrib location for height_texture_layer: " << height_texture_layer;

    m_vao->bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    if (bounds != -1) {
        m_bounds_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(bounds));
        f->glVertexAttribPointer(GLuint(bounds), /*size*/ 4, /*type*/ GL_FLOAT, /*normalised*/ GL_FALSE, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(bounds), 1);
    }
    if (packed_tile_id != -1) {
        m_draw_tile_id_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(packed_tile_id));
        f->glVertexAttribIPointer(GLuint(packed_tile_id), /*size*/ 2, /*type*/ GL_UNSIGNED_INT, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(packed_tile_id), 1);
    }
    if (height_texture_layer != -1) {
        m_height_texture_layer_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(height_texture_layer));
        f->glVertexAttribIPointer(GLuint(height_texture_layer), /*size*/ 1, /*type*/ GL_INT, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(height_texture_layer), 1);
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

    TileInfo tileinfo;
    tileinfo.tile_id = id;
    tileinfo.bounds = tile::SrsBounds(bounds);

    // find empty spot and upload texture
    const auto t = std::find(m_loaded_tiles.begin(), m_loaded_tiles.end(), tile::Id { unsigned(-1), {} });
    assert(t != m_loaded_tiles.end());
    *t = id;
    const auto layer_index = unsigned(t - m_loaded_tiles.begin());
    tileinfo.height_texture_layer = layer_index;
    m_ortho_textures->upload(ortho_texture, layer_index);
    m_heightmap_textures->upload(height_map, layer_index);

    // add to m_gpu_tiles
    m_gpu_tiles.push_back(tileinfo);
    m_draw_list_generator.add_tile(id);
}

void TileManager::update_gpu_id_map()
{
    const auto hash_to_pixel = [](uint16_t hash) { return glm::uvec2(hash & 255, hash >> 8); };
    nucleus::Raster<glm::u32vec2> packed_ids({ 256, 256 }, glm::u32vec2(-1, -1));
    nucleus::Raster<uint16_t> texture_ids({ 256, 256 }, 0);
    for (const auto tile : m_gpu_tiles) {
        auto hash = nucleus::srs::hash_uint16(tile.tile_id);
        while (packed_ids.pixel(hash_to_pixel(hash)) != glm::u32vec2(-1, -1))
            hash++;

        packed_ids.pixel(hash_to_pixel(hash)) = nucleus::srs::pack(tile.tile_id);
        texture_ids.pixel(hash_to_pixel(hash)) = tile.height_texture_layer;
    }
    m_texture_id_map_texture->upload(texture_ids);
    m_tile_id_map_texture->upload(packed_ids);
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
    update_gpu_id_map();
}
