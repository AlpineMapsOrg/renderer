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

#include "TileGeometry.h"

#include "ShaderProgram.h"
#include "ShaderRegistry.h"
#include "Texture.h"
#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <nucleus/camera/Definition.h>
#include <nucleus/utils/terrain_mesh_index_generator.h>

namespace gl_engine {

namespace {
    template <typename T> int bufferLengthInBytes(const std::vector<T>& vec) { return int(vec.size() * sizeof(T)); }
} // namespace

TileGeometry::TileGeometry(unsigned int texture_resolution)
    : m_texture_resolution { texture_resolution }
{
}

void TileGeometry::init()
{

    using nucleus::utils::terrain_mesh_index_generator::surface_quads_with_curtains;
    assert(QOpenGLContext::currentContext());
    const auto indices = surface_quads_with_curtains<uint16_t>(m_texture_resolution);
    auto index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    index_buffer->create();
    index_buffer->bind();
    index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    index_buffer->allocate(indices.data(), bufferLengthInBytes(indices));
    index_buffer->release();
    m_index_buffer.first = std::move(index_buffer);
    m_index_buffer.second = indices.size();

    m_instance_bounds_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_instance_bounds_buffer->create();
    m_instance_bounds_buffer->bind();
    m_instance_bounds_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_instance_bounds_buffer->allocate(GLsizei(m_gpu_array_helper.size() * sizeof(glm::vec4)));

    m_instance_tile_id_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_instance_tile_id_buffer->create();
    m_instance_tile_id_buffer->bind();
    m_instance_tile_id_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_instance_tile_id_buffer->allocate(GLsizei(m_gpu_array_helper.size() * sizeof(glm::u32vec2)));

    m_vao = std::make_unique<QOpenGLVertexArrayObject>();
    m_vao->create();
    m_vao->bind();
    m_index_buffer.first->bind();
    m_vao->release();

    m_dtm_textures = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::R16UI);
    m_dtm_textures->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);
    m_dtm_textures->allocate_array(m_texture_resolution, m_texture_resolution, unsigned(m_gpu_array_helper.size()));

    m_dictionary_tile_id_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::RG32UI);
    m_dictionary_tile_id_texture->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    m_dictionary_array_index_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R16UI);
    m_dictionary_array_index_texture->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    m_instance_2_zoom = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R8UI);
    m_instance_2_zoom->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    m_instance_2_array_index = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::R16UI);
    m_instance_2_array_index->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    m_instance_2_dtm_bounds = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::RGBA32F);
    m_instance_2_dtm_bounds->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);

    auto example_shader = std::make_shared<ShaderProgram>("tile.vert", "tile.frag");
    int bounds = example_shader->attribute_location("instance_bounds");
    qDebug() << "attrib location for instance_bounds: " << bounds;
    int packed_tile_id = example_shader->attribute_location("instance_tile_id_packed");
    qDebug() << "attrib location for instance_tile_id_packed: " << packed_tile_id;

    m_vao->bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    if (bounds != -1) {
        m_instance_bounds_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(bounds));
        f->glVertexAttribPointer(GLuint(bounds), /*size*/ 4, /*type*/ GL_FLOAT, /*normalised*/ GL_FALSE, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(bounds), 1);
    }
    if (packed_tile_id != -1) {
        m_instance_tile_id_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(packed_tile_id));
        f->glVertexAttribIPointer(GLuint(packed_tile_id), /*size*/ 2, /*type*/ GL_UNSIGNED_INT, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(packed_tile_id), 1);
    }

    update_gpu_id_map();
}

void TileGeometry::draw(ShaderProgram* shader, const nucleus::camera::Definition& camera, const std::vector<nucleus::tile::TileBounds>& draw_list) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    shader->set_uniform("n_edge_vertices", m_texture_resolution);
    shader->set_uniform("height_tex_sampler", 1);
    shader->set_uniform("height_tex_index_sampler", 3);
    shader->set_uniform("height_tex_tile_id_sampler", 4);

    m_dtm_textures->bind(1);
    m_dictionary_array_index_texture->bind(3);
    m_dictionary_tile_id_texture->bind(4);
    m_vao->bind();

    std::vector<glm::vec4> bounds;
    bounds.reserve(draw_list.size());

    std::vector<glm::u32vec2> packed_id;
    packed_id.reserve(draw_list.size());

    {
        nucleus::Raster<uint8_t> zoom_level_raster = { glm::uvec2 { 1024, 1 } };
        nucleus::Raster<uint16_t> array_index_raster = { glm::uvec2 { 1024, 1 } };
        nucleus::Raster<glm::vec4> bounds_raster = { glm::uvec2 { 1024, 1 } };
        for (unsigned i = 0; i < std::min(unsigned(draw_list.size()), 1024u); ++i) {
            const auto& tile = draw_list[i];
            bounds.push_back(glm::vec4 { tile.bounds.min.x - camera.position().x,
                tile.bounds.min.y - camera.position().y,
                tile.bounds.max.x - camera.position().x,
                tile.bounds.max.y - camera.position().y });
            packed_id.push_back(nucleus::srs::pack(tile.id));

            const auto layer = m_gpu_array_helper.layer(draw_list[i].id);
            if (layer.id.zoom_level > 50) // happens during startup
                continue;

            zoom_level_raster.pixel({ i, 0 }) = layer.id.zoom_level;
            array_index_raster.pixel({ i, 0 }) = layer.index;
            const auto geom_aabb = m_aabb_decorator->aabb(layer.id);
            bounds_raster.pixel({ i, 0 }) = glm::vec4 { geom_aabb.min.x - camera.position().x,
                geom_aabb.min.y - camera.position().y,
                geom_aabb.max.x - camera.position().x,
                geom_aabb.max.y - camera.position().y };
        }

        m_instance_2_array_index->bind(5);
        shader->set_uniform("instance_2_array_index_sampler", 5);
        m_instance_2_array_index->upload(array_index_raster);

        m_instance_2_zoom->bind(6);
        shader->set_uniform("instance_2_zoom_sampler", 6);
        m_instance_2_zoom->upload(zoom_level_raster);

        m_instance_2_dtm_bounds->bind(9);
        shader->set_uniform("instance_2_bounds_sampler", 9);
        m_instance_2_dtm_bounds->upload(bounds_raster);
    }

    m_instance_bounds_buffer->bind();
    m_instance_bounds_buffer->write(0, bounds.data(), GLsizei(bounds.size() * sizeof(decltype(bounds)::value_type)));

    m_instance_tile_id_buffer->bind();
    m_instance_tile_id_buffer->write(0, packed_id.data(), GLsizei(packed_id.size() * sizeof(decltype(packed_id)::value_type)));

    f->glDrawElementsInstanced(GL_TRIANGLE_STRIP, GLsizei(m_index_buffer.second), GL_UNSIGNED_SHORT, nullptr, GLsizei(draw_list.size()));
    f->glBindVertexArray(0);
}

void TileGeometry::set_aabb_decorator(const nucleus::tile::utils::AabbDecoratorPtr& new_aabb_decorator) { m_aabb_decorator = new_aabb_decorator; }

void TileGeometry::set_tile_limit(unsigned int new_limit)
{
    assert(!m_dtm_textures);
    m_gpu_array_helper.set_tile_limit(new_limit);
}

void TileGeometry::update_gpu_id_map()
{
    auto [packed_ids, layers] = m_gpu_array_helper.generate_dictionary();
    m_dictionary_array_index_texture->upload(layers);
    m_dictionary_tile_id_texture->upload(packed_ids);
}

unsigned TileGeometry::tile_count() const { return m_gpu_array_helper.n_occupied(); }

void TileGeometry::update_gpu_tiles(const std::vector<radix::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuGeometryTile>& new_tiles)
{

    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    for (const auto& id : deleted_tiles) {
        m_gpu_array_helper.remove_tile(id);
    }
    for (const auto& tile : new_tiles) {
        // test for validity
        assert(tile.id.zoom_level < 100);
        assert(tile.surface);

        // find empty spot and upload texture
        const auto layer_index = m_gpu_array_helper.add_tile(tile.id);
        m_dtm_textures->upload(*tile.surface, layer_index);
    }
    update_gpu_id_map();
}

} // namespace gl_engine
