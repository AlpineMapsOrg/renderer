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

#include "TextureLayer.h"

#include "ShaderProgram.h"
#include "ShaderRegistry.h"
#include "Texture.h"
#include "TileGeometry.h"
#include <QOpenGLExtraFunctions>

namespace gl_engine {

TextureLayer::TextureLayer(unsigned int resolution, QObject* parent)
    : QObject { parent }
    , m_resolution(resolution)
{
}

void gl_engine::TextureLayer::init(ShaderRegistry* shader_registry)
{
    m_shader = std::make_shared<ShaderProgram>("tile.vert", "tile.frag");
    shader_registry->add_shader(m_shader);

    m_texture_array = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::CompressedRGBA8);
    m_texture_array->setParams(Texture::Filter::MipMapLinear, Texture::Filter::Linear, true);
    m_texture_array->allocate_array(m_resolution, m_resolution, unsigned(m_gpu_array_helper.size()));

    m_instanced_zoom = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R8UI);
    m_instanced_zoom->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    m_instanced_array_index = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R16UI);
    m_instanced_array_index->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);
}

void TextureLayer::draw(const TileGeometry& tile_geometry,
    const nucleus::camera::Definition& camera,
    const nucleus::tile::DrawListGenerator::TileSet& draw_tiles,
    bool sort_tiles,
    glm::dvec3 sort_position) const
{
    m_shader->bind();
    m_texture_array->bind(2);
    m_shader->set_uniform("texture_sampler", 2);

    const auto draw_list = tile_geometry.sort(camera, draw_tiles);
    nucleus::Raster<uint8_t> zoom_level_raster = { glm::uvec2 { 1024, 1 } };
    nucleus::Raster<uint16_t> array_index_raster = { glm::uvec2 { 1024, 1 } };
    for (unsigned i = 0; i < std::min(unsigned(draw_list.size()), 1024u); ++i) {
        const auto layer = m_gpu_array_helper.layer(draw_list[i]);
        zoom_level_raster.pixel({ i, 0 }) = layer.id.zoom_level;
        array_index_raster.pixel({ i, 0 }) = layer.index;
    }

    m_instanced_array_index->bind(7);
    m_shader->set_uniform("instanced_array_index_sampler", 7);
    m_instanced_array_index->upload(array_index_raster);

    m_instanced_zoom->bind(8);
    m_shader->set_uniform("instanced_zoom_sampler", 8);
    m_instanced_zoom->upload(zoom_level_raster);

    tile_geometry.draw(m_shader.get(), camera, draw_tiles, sort_tiles, sort_position);
}

unsigned TextureLayer::tile_count() const { return m_gpu_array_helper.n_occupied(); }

void TextureLayer::update_gpu_tiles(const std::vector<nucleus::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuTextureTile>& new_tiles)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    for (const auto& tile_id : deleted_tiles) {
        m_gpu_array_helper.remove_tile(tile_id);
    }
    for (const auto& tile : new_tiles) {
        // test for validity
        assert(tile.id.zoom_level < 100);
        assert(tile.texture);

        // find empty spot and upload texture
        const auto layer_index = m_gpu_array_helper.add_tile(tile.id);
        m_texture_array->upload(*tile.texture, layer_index);
    }
}

void TextureLayer::set_quad_limit(unsigned int new_limit) { m_gpu_array_helper.set_quad_limit(new_limit); }

} // namespace gl_engine
