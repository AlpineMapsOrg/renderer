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
    m_instance_bounds_buffer->allocate(GLsizei(1024 * sizeof(glm::vec4)));

    m_instance_tile_id_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_instance_tile_id_buffer->create();
    m_instance_tile_id_buffer->bind();
    m_instance_tile_id_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_instance_tile_id_buffer->allocate(GLsizei(1024 * sizeof(glm::u32vec2)));

    m_dtm_array_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_dtm_array_index_buffer->create();
    m_dtm_array_index_buffer->bind();
    m_dtm_array_index_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_dtm_array_index_buffer->allocate(GLsizei(1024 * sizeof(uint16_t)));

    m_dtm_zoom_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_dtm_zoom_buffer->create();
    m_dtm_zoom_buffer->bind();
    m_dtm_zoom_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_dtm_zoom_buffer->allocate(GLsizei(1024 * sizeof(uint8_t)));

    m_vao = std::make_unique<QOpenGLVertexArrayObject>();
    m_vao->create();
    m_vao->bind();
    m_index_buffer.first->bind();
    m_vao->release();

    m_dtm_textures = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::R16UI);
    m_dtm_textures->setParams(Texture::Filter::Nearest, Texture::Filter::Nearest);
    m_dtm_textures->allocate_array(m_texture_resolution, m_texture_resolution, unsigned(m_gpu_array_helper.size()));

    auto example_shader = std::make_shared<ShaderProgram>("tile.vert", "tile.frag");
    const auto instance_bounds_location = example_shader->attribute_location("instance_bounds");
    qDebug() << "attrib location for instance_bounds: " << instance_bounds_location;
    const auto instance_tile_id_location = example_shader->attribute_location("instance_tile_id_packed");
    qDebug() << "attrib location for instance_tile_id_packed: " << instance_tile_id_location;
    const auto dtm_array_index_location = example_shader->attribute_location("dtm_array_index");
    qDebug() << "attrib location for dtm_array_index: " << dtm_array_index_location;
    const auto dtm_zoom_location = example_shader->attribute_location("dtm_zoom");
    qDebug() << "attrib location for dtm_zoom: " << dtm_array_index_location;

    m_vao->bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    if (instance_bounds_location != -1) {
        m_instance_bounds_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(instance_bounds_location));
        f->glVertexAttribPointer(GLuint(instance_bounds_location), /*size*/ 4, /*type*/ GL_FLOAT, /*normalised*/ GL_FALSE, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(instance_bounds_location), 1);
    }
    if (instance_tile_id_location != -1) {
        m_instance_tile_id_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(instance_tile_id_location));
        f->glVertexAttribIPointer(GLuint(instance_tile_id_location), /*size*/ 2, /*type*/ GL_UNSIGNED_INT, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(instance_tile_id_location), 1);
    }
    if (dtm_array_index_location != -1) {
        m_dtm_array_index_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(dtm_array_index_location));
        f->glVertexAttribIPointer(GLuint(dtm_array_index_location), /*size*/ 1, /*type*/ GL_UNSIGNED_SHORT, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(dtm_array_index_location), 1);
    }
    if (dtm_zoom_location != -1) {
        m_dtm_zoom_buffer->bind();
        f->glEnableVertexAttribArray(GLuint(dtm_zoom_location));
        f->glVertexAttribIPointer(GLuint(dtm_zoom_location), /*size*/ 1, /*type*/ GL_UNSIGNED_BYTE, /*stride*/ 0, nullptr);
        f->glVertexAttribDivisor(GLuint(dtm_zoom_location), 1);
    }
}

void TileGeometry::draw(ShaderProgram* shader, const nucleus::camera::Definition& camera, const std::vector<nucleus::tile::TileBounds>& draw_list) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    shader->set_uniform("n_edge_vertices", m_texture_resolution);
    shader->set_uniform("height_tex_sampler", 1);

    m_dtm_textures->bind(1);
    m_vao->bind();

    std::vector<glm::vec4> bounds;
    bounds.reserve(draw_list.size());

    std::vector<glm::u32vec2> packed_id;
    packed_id.reserve(draw_list.size());

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

    m_instance_bounds_buffer->bind();
    m_instance_bounds_buffer->write(0, bounds.data(), GLsizei(bounds.size() * sizeof(decltype(bounds)::value_type)));

    m_instance_tile_id_buffer->bind();
    m_instance_tile_id_buffer->write(0, packed_id.data(), GLsizei(packed_id.size() * sizeof(decltype(packed_id)::value_type)));

    m_dtm_array_index_buffer->bind();
    m_dtm_array_index_buffer->write(0, array_index_raster.bytes(), GLsizei(array_index_raster.width() * sizeof(uint16_t)));

    m_dtm_zoom_buffer->bind();
    m_dtm_zoom_buffer->write(0, zoom_level_raster.bytes(), GLsizei(zoom_level_raster.width() * sizeof(uint8_t)));

    f->glDrawElementsInstanced(GL_TRIANGLE_STRIP, GLsizei(m_index_buffer.second), GL_UNSIGNED_SHORT, nullptr, GLsizei(draw_list.size()));
    f->glBindVertexArray(0);
}

void TileGeometry::set_aabb_decorator(const nucleus::tile::utils::AabbDecoratorPtr& new_aabb_decorator) { m_aabb_decorator = new_aabb_decorator; }

void TileGeometry::set_tile_limit(unsigned int new_limit)
{
    assert(!m_dtm_textures);
    m_gpu_array_helper.set_tile_limit(new_limit);
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
}

} // namespace gl_engine
