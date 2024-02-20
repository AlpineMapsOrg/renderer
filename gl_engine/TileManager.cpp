 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2023 Adam Celarek
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

std::vector<glm::vec4> boundsArray(const TileSet& tileset, const glm::dvec3& camera_position)
{
    std::vector<glm::vec4> ret;
    ret.reserve(tileset.tiles.size());
    for (const auto& tile : tileset.tiles) {
        ret.emplace_back(tile.second.min.x - camera_position.x,
            tile.second.min.y - camera_position.y,
            tile.second.max.x - camera_position.x,
            tile.second.max.y - camera_position.y);
    }
    return ret;
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
    for (auto i = 0; i < MAX_TILES_PER_TILESET; ++i) {
        const auto indices = surface_quads_with_curtains<uint16_t>(N_EDGE_VERTICES);
        auto index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
        index_buffer->create();
        index_buffer->bind();
        index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
        index_buffer->allocate(indices.data(), bufferLengthInBytes(indices));
        index_buffer->release();
        m_index_buffers.emplace_back(std::move(index_buffer), indices.size());
    }
    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &m_max_anisotropy);
}

const std::vector<TileSet>& TileManager::tiles() const
{
    return m_gpu_tiles;
}

bool compareTileSetPair(std::pair<float, const TileSet*> t1, std::pair<float, const TileSet*> t2)
{
    return (t1.first < t2.first);
}

const nucleus::tile_scheduler::DrawListGenerator::TileSet TileManager::generate_tilelist(const nucleus::camera::Definition& camera) const {
    return m_draw_list_generator.generate_for(camera);
}

void TileManager::draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera,
                       const nucleus::tile_scheduler::DrawListGenerator::TileSet draw_tiles,
                       bool sort_tiles, glm::dvec3 sort_position) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    shader_program->set_uniform("n_edge_vertices", N_EDGE_VERTICES);
    shader_program->set_uniform("texture_sampler", 2);
    shader_program->set_uniform("height_sampler", 1);

    // Sort depending on distance to sort_position
    std::vector<std::pair<float, const TileSet*>> tile_list;
    for (const auto& tileset : tiles()) {
        float dist = 0.0;
        if (!draw_tiles.contains(tileset.tiles.front().first))
            continue;
        if (sort_tiles) {
            glm::vec2 pos_wrt = glm::vec2(
                tileset.tiles[0].second.min.x - sort_position.x,
                tileset.tiles[0].second.min.y - sort_position.y
                );
            dist = glm::length(pos_wrt);
        }
        tile_list.push_back(std::pair<float, const TileSet*>(dist, &tileset));
    }
    if (sort_tiles) std::sort(tile_list.begin(), tile_list.end(), compareTileSetPair);

    for (const auto& tileset : tile_list) {
        tileset.second->vao->bind();
        shader_program->set_uniform_array("bounds", boundsArray(*tileset.second, camera.position()));
        shader_program->set_uniform("tileset_id", (int)((tileset.second->tiles[0].first.coords[0] + tileset.second->tiles[0].first.coords[1])));
        shader_program->set_uniform("tileset_zoomlevel", tileset.second->tiles[0].first.zoom_level);
        tileset.second->ortho_texture->bind(2);
        tileset.second->heightmap_texture->bind(1);
        f->glDrawElements(GL_TRIANGLE_STRIP, tileset.second->gl_element_count, tileset.second->gl_index_type, nullptr);
    }
    f->glBindVertexArray(0);
}

void TileManager::remove_tile(const tile::Id& tile_id)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;
    // clear slot
    // or remove from list and free resources
    const auto found_tile = std::find_if(m_gpu_tiles.begin(), m_gpu_tiles.end(), [&tile_id](const TileSet& tileset) {
        return tileset.tiles.front().first == tile_id;
    });
    if (found_tile != m_gpu_tiles.end())
        m_gpu_tiles.erase(found_tile);
    m_draw_list_generator.remove_tile(tile_id);

    emit tiles_changed();
}

void TileManager::initilise_attribute_locations(ShaderProgram* /*program*/) { }

void TileManager::set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr& new_aabb_decorator)
{
    m_draw_list_generator.set_aabb_decorator(new_aabb_decorator);
}

void TileManager::add_tile(
    const tile::Id& id, tile::SrsAndHeightBounds bounds, const nucleus::utils::ColourTexture& ortho_texture, const nucleus::Raster<uint16_t>& height_map)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    // need to call GLWindow::makeCurrent, when calling through signals?
    // find an empty slot => todo, for now just create a new tile every time.
    // setup / copy data to gpu
    TileSet tileset;
    tileset.tiles.emplace_back(id, tile::SrsBounds(bounds));
    tileset.vao = std::make_unique<QOpenGLVertexArrayObject>();
    tileset.vao->create();
    tileset.vao->bind();

    { // vao state
        m_index_buffers[0].first->bind();
        tileset.gl_element_count = int(m_index_buffers[0].second);
        tileset.gl_index_type = GL_UNSIGNED_SHORT;
    }
    tileset.vao->release();
    tileset.ortho_texture = std::make_unique<Texture>(Texture::Target::_2d,
        ortho_texture.algorithm() == nucleus::utils::ColourTexture::Algorithm::Uncompressed_RGBA ? Texture::Format::RGBA8 : Texture::Format::CompressedRGBA8);
    tileset.ortho_texture->setParams(Texture::Filter::Linear, Texture::Filter::Linear);
    tileset.ortho_texture->upload(ortho_texture);

    tileset.heightmap_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R16UI);
    tileset.heightmap_texture->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);
    tileset.heightmap_texture->upload(height_map);

    // add to m_gpu_tiles
    m_gpu_tiles.push_back(std::move(tileset));
    m_draw_list_generator.add_tile(id);

    emit tiles_changed();
}

void TileManager::set_permissible_screen_space_error(float new_permissible_screen_space_error)
{
    m_draw_list_generator.set_permissible_screen_space_error(new_permissible_screen_space_error);
}

void TileManager::update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    for (const auto& quad : new_quads) {
        for (const auto& tile : quad.tiles) {
            // test for validity
            assert(tile.id.zoom_level < 100);
            assert(tile.height);
            assert(tile.ortho);
            add_tile(tile.id, tile.bounds, *tile.ortho, *tile.height);
        }
    }
    for (const auto& quad : deleted_quads) {
        for (const auto& id : quad.children()) {
            remove_tile(id);
        }
    }
}
