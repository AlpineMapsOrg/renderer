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

#include "AvalancheWarningLayer.h"

#include "ShaderProgram.h"
#include "ShaderRegistry.h"
#include "TileGeometry.h"
#include <QOpenGLExtraFunctions>
#include <TextureLayer.h>

namespace gl_engine {

AvalancheWarningLayer::AvalancheWarningLayer(QObject* parent)
    : QObject { parent }
{
}

void gl_engine::AvalancheWarningLayer::init(ShaderRegistry* shader_registry, std::shared_ptr<TextureLayer> surfaceshaded_layer)
{
    m_shader = std::make_shared<ShaderProgram>("tile.vert", "eaws.frag");
    shader_registry->add_shader(m_shader);

    m_texture_array = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::R16UI);
    m_texture_array->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest, false);
    m_texture_array->allocate_array(m_resolution, m_resolution, unsigned(m_gpu_array_helper.size()));

    m_instanced_zoom = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R8UI);
    m_instanced_zoom->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    m_instanced_array_index = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R16UI);
    m_instanced_array_index->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);
    m_surfshaded_layer = surfaceshaded_layer;
}

void AvalancheWarningLayer::draw(
    const TileGeometry& tile_geometry, const nucleus::camera::Definition& camera, const std::vector<nucleus::tile::TileBounds>& draw_list) const
{
    m_shader->bind();
    m_texture_array->bind(2);
    m_shader->set_uniform("texture_sampler", 2);

    nucleus::Raster<uint8_t> zoom_level_raster = { glm::uvec2 { 1024, 1 } };
    nucleus::Raster<uint16_t> array_index_raster = { glm::uvec2 { 1024, 1 } };
    for (unsigned i = 0; i < std::min(unsigned(draw_list.size()), 1024u); ++i) {
        const auto layer = m_gpu_array_helper.layer(draw_list[i].id);
        zoom_level_raster.pixel({ i, 0 }) = layer.id.zoom_level;
        array_index_raster.pixel({ i, 0 }) = layer.index;
    }

    m_instanced_array_index->bind(7);
    m_shader->set_uniform("instanced_texture_array_index_sampler", 7);
    m_instanced_array_index->upload(array_index_raster);

    m_instanced_zoom->bind(8);
    m_shader->set_uniform("instanced_texture_zoom_sampler", 8);
    m_instanced_zoom->upload(zoom_level_raster);

    m_surfshaded_layer->m_texture_array->bind(9);
    m_shader->set_uniform("texture_sampler2", 9);
    for (unsigned i = 0; i < std::min(unsigned(draw_list.size()), 1024u); ++i) {
        const auto layer = m_surfshaded_layer->m_gpu_array_helper.layer(draw_list[i].id);
        zoom_level_raster.pixel({ i, 0 }) = layer.id.zoom_level;
        array_index_raster.pixel({ i, 0 }) = layer.index;
    }

    m_surfshaded_layer->m_instanced_array_index->bind(10);
    m_shader->set_uniform("instanced_texture_array_index_sampler2", 10);
    m_surfshaded_layer->m_instanced_array_index->upload(array_index_raster);

    m_surfshaded_layer->m_instanced_zoom->bind(11);
    m_shader->set_uniform("instanced_texture_zoom_sampler2", 11);
    m_surfshaded_layer->m_instanced_zoom->upload(zoom_level_raster);

    tile_geometry.draw(m_shader.get(), camera, draw_list);
}

void AvalancheWarningLayer::update_gpu_tiles(const std::vector<nucleus::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuEawsTile>& new_tiles)
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

void AvalancheWarningLayer::set_tile_limit(unsigned int new_limit)
{
    assert(new_limit < 2048); // array textures with size > 2048 are not supported on all devices
    assert(!m_texture_array);
    m_gpu_array_helper.set_tile_limit(new_limit);
}

} // namespace gl_engine
