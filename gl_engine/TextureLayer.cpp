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

TextureLayer::TextureLayer(QObject* parent)
    : QObject { parent }
{
}

void gl_engine::TextureLayer::init(ShaderRegistry* shader_registry)
{
    m_shader = std::make_shared<ShaderProgram>("tile.vert", "tile.frag");
    shader_registry->add_shader(m_shader);

    m_ortho_textures = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::CompressedRGBA8);
    m_ortho_textures->setParams(Texture::Filter::MipMapLinear, Texture::Filter::Linear, true);
    // TODO: might become larger than GL_MAX_ARRAY_TEXTURE_LAYERS
    m_ortho_textures->allocate_array(ORTHO_RESOLUTION, ORTHO_RESOLUTION, unsigned(m_gpu_array_helper.size()));

    m_tile_id_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::RG32UI);
    m_tile_id_texture->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    m_array_index_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R16UI);
    m_array_index_texture->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    update_gpu_id_map();
}

void TextureLayer::draw(const TileGeometry& tile_geometry,
    const nucleus::camera::Definition& camera,
    const nucleus::tile::DrawListGenerator::TileSet& draw_tiles,
    bool sort_tiles,
    glm::dvec3 sort_position) const
{
    m_shader->bind();
    m_shader->set_uniform("ortho_sampler", 2);
    m_ortho_textures->bind(2);

    m_shader->set_uniform("ortho_map_index_sampler", 5);
    m_array_index_texture->bind(5);
    m_shader->set_uniform("ortho_map_tile_id_sampler", 6);
    m_tile_id_texture->bind(6);

    tile_geometry.draw(m_shader.get(), camera, draw_tiles, sort_tiles, sort_position);
}

unsigned TextureLayer::tile_count() const { return m_gpu_array_helper.n_occupied(); }

void TextureLayer::update_gpu_quads(const std::vector<nucleus::tile::GpuTextureQuad>& new_quads, const std::vector<nucleus::tile::Id>& deleted_quads)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    for (const auto& quad : deleted_quads) {
        for (const auto& id : quad.children()) {
            m_gpu_array_helper.remove_tile(id);
        }
    }
    for (const auto& quad : new_quads) {
        for (const auto& tile : quad.tiles) {
            // test for validity
            assert(tile.id.zoom_level < 100);
            assert(tile.texture);

            // find empty spot and upload texture
            const auto layer_index = m_gpu_array_helper.add_tile(tile.id);
            m_ortho_textures->upload(*tile.texture, layer_index);
        }
    }
    update_gpu_id_map();
}

void TextureLayer::set_quad_limit(unsigned int new_limit) { m_gpu_array_helper.set_quad_limit(new_limit); }

void TextureLayer::update_gpu_id_map()
{
    auto [packed_ids, layers] = m_gpu_array_helper.generate_dictionary();
    m_array_index_texture->upload(layers);
    m_tile_id_texture->upload(packed_ids);
}

} // namespace gl_engine
