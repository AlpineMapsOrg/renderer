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

#include "MapLabels.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLPixelTransferOptions>
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif

#include "ShaderProgram.h"
#include "ShaderRegistry.h"
#include "radix/tile.h"
#include <nucleus/map_label/FontRenderer.h>
#include <nucleus/srs.h>

using namespace Qt::Literals::StringLiterals;

namespace gl_engine {

MapLabels::MapLabels(const nucleus::tile::utils::AabbDecoratorPtr& aabb_decorator, QObject* parent)
    : QObject { parent }
{
    m_draw_list_generator.set_aabb_decorator(aabb_decorator);
}

void MapLabels::init(ShaderRegistry* shader_registry)
{
    m_label_shader = std::make_shared<ShaderProgram>("labels.vert", "labels.frag");
    m_picker_shader = std::make_shared<ShaderProgram>("labels.vert", "labels_picker.frag");
    shader_registry->add_shader(m_label_shader);
    shader_registry->add_shader(m_picker_shader);

    // load the font texture
    const auto atlas_data = m_mapLabelFactory.init_font_atlas();

    m_font_texture = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::RG8);
    m_font_texture->setParams(Texture::Filter::MipMapLinear, Texture::Filter::Linear);
    m_font_texture->allocate_array(
        nucleus::map_label::FontRenderer::m_font_atlas_size.width(), nucleus::map_label::FontRenderer::m_font_atlas_size.height(), nucleus::map_label::FontRenderer::m_max_textures);
    for(unsigned int i = 0; i < atlas_data.font_atlas.size(); i++)
    {
        m_font_texture->upload(atlas_data.font_atlas[i],i);
    }

    const auto& labelIcons = m_mapLabelFactory.label_icons();

    m_icon_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::RGBA8);
    m_icon_texture->setParams(Texture::Filter::MipMapLinear, Texture::Filter::Linear);
    m_icon_texture->upload(labelIcons);

    m_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    m_index_buffer->create();
    m_index_buffer->bind();
    m_index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_index_buffer->allocate(m_mapLabelFactory.m_indices.data(), m_mapLabelFactory.m_indices.size() * sizeof(unsigned int));
    m_indices_count = m_mapLabelFactory.m_indices.size();
}

MapLabels::TileSet MapLabels::generate_draw_list(const nucleus::camera::Definition& camera) const
{
    return m_draw_list_generator.cull(m_draw_list_generator.generate_for(camera, 256, 18), camera.frustum());
}

void MapLabels::upload_to_gpu(const TileId& id, const PointOfInterestCollection& features)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    std::shared_ptr<GPUVectorTile> vectortile = std::make_shared<GPUVectorTile>();
    vectortile->id = id;

    vectortile->vao = std::make_unique<QOpenGLVertexArrayObject>();
    vectortile->vao->create();
    vectortile->vao->bind();

    const auto [allLabels, reference_point, atlas_data] = m_mapLabelFactory.create_labels(features);
    if (atlas_data.changed) {
        for (unsigned int i = 0; i < atlas_data.font_atlas.size(); i++) {
            m_font_texture->upload(atlas_data.font_atlas[i], i);
        }
    }
    vectortile->reference_point = reference_point;

    { // vao state
        m_index_buffer->bind();

        vectortile->vertex_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        vectortile->vertex_buffer->create();
        vectortile->vertex_buffer->bind();
        vectortile->vertex_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
        vectortile->vertex_buffer->allocate(allLabels.data(), allLabels.size() * sizeof(nucleus::map_label::VertexData));
        vectortile->instance_count = allLabels.size();

        QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

        // vertex positions
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::map_label::VertexData), nullptr);
        f->glVertexAttribDivisor(0, 1); // buffer is active for 1 instance (for the whole quad)
        // uvs
        f->glEnableVertexAttribArray(1);
        f->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::map_label::VertexData), (GLvoid*)(sizeof(glm::vec4)));
        f->glVertexAttribDivisor(1, 1); // buffer is active for 1 instance (for the whole quad)
        // picker color
        f->glEnableVertexAttribArray(2);
        f->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::map_label::VertexData), (GLvoid*)(sizeof(glm::vec4) * 2));
        f->glVertexAttribDivisor(2, 1); // buffer is active for 1 instance (for the whole quad)
        // world position
        f->glEnableVertexAttribArray(3);
        f->glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(nucleus::map_label::VertexData), (GLvoid*)(sizeof(glm::vec4) * 3));
        f->glVertexAttribDivisor(3, 1); // buffer is active for 1 instance (for the whole quad)
        // label importance
        f->glEnableVertexAttribArray(4);
        f->glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(nucleus::map_label::VertexData), (GLvoid*)((sizeof(glm::vec4) * 3 + (sizeof(glm::vec3)))));
        f->glVertexAttribDivisor(4, 1); // buffer is active for 1 instance (for the whole quad)
        // texture index
        f->glEnableVertexAttribArray(5);
        f->glVertexAttribIPointer(5, 1, GL_INT, sizeof(nucleus::map_label::VertexData), (GLvoid*)((sizeof(glm::vec4) * 3 + (sizeof(glm::vec3)) + sizeof(float))));
        f->glVertexAttribDivisor(5, 1); // buffer is active for 1 instance (for the whole quad)
    }

    vectortile->vao->release();

    // add vector tile to gpu tiles
    m_gpu_tiles[id] = vectortile;
}

void MapLabels::update_labels(const std::vector<PoiTile>& updated_tiles, const std::vector<TileId>& removed_tiles)
{
    // remove tiles that aren't needed anymore
    for (const auto& id : removed_tiles) {
        remove_tile(id);
        m_draw_list_generator.remove_tile(id);
    }

    for (const auto& vectortile : updated_tiles) {
        // since we are renewing the tile we remove it first to delete allocations like vao
        remove_tile(vectortile.id);

        upload_to_gpu(vectortile.id, *vectortile.data);
        m_draw_list_generator.add_tile(vectortile.id);
    }
}

void MapLabels::remove_tile(const TileId& tile_id)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    // we can only remove something that exists
    if (!m_gpu_tiles.contains(tile_id))
        return;

    if (m_gpu_tiles.at(tile_id)->vao)
        m_gpu_tiles.at(tile_id)->vao->destroy(); // ecplicitly destroy vao

    m_gpu_tiles.erase(tile_id);
}

void MapLabels::draw(Framebuffer* gbuffer, const nucleus::camera::Definition& camera, const TileSet& draw_tiles) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glEnable(GL_BLEND);

    m_label_shader->bind();
    m_label_shader->set_uniform("label_dist_scaling", true);
    m_label_shader->set_uniform("texin_depth", 0);
    gbuffer->bind_colour_texture(1, 0);

    m_label_shader->set_uniform("font_sampler", 1);
    m_font_texture->bind(1);
    m_label_shader->set_uniform("icon_sampler", 2);
    m_icon_texture->bind(2);

    for (const auto& vectortile : m_gpu_tiles) {
        if (!draw_tiles.contains(vectortile.first))
            continue; // tile is not in draw_tiles -> look at next tile

        if (vectortile.second->instance_count > 0) {
            vectortile.second->vao->bind();
            m_label_shader->set_uniform("reference_position", glm::vec3(vectortile.second->reference_point - camera.position()));

            // if the labels wouldn't collide, we could use an extra buffer, one draw call and
            // f->glBlendEquationSeparate(GL_MIN, GL_MAX);
            m_label_shader->set_uniform("drawing_outline", true);
            f->glDrawElementsInstanced(GL_TRIANGLES, m_indices_count, GL_UNSIGNED_INT, 0, vectortile.second->instance_count);
            m_label_shader->set_uniform("drawing_outline", false);
            f->glDrawElementsInstanced(GL_TRIANGLES, m_indices_count, GL_UNSIGNED_INT, 0, vectortile.second->instance_count);

            vectortile.second->vao->release();
        }
    }
}

void MapLabels::draw_picker(Framebuffer* gbuffer, const nucleus::camera::Definition& camera, const TileSet& draw_tiles) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    m_picker_shader->bind();
    m_picker_shader->set_uniform("label_dist_scaling", true);
    m_picker_shader->set_uniform("texin_depth", 0);
    gbuffer->bind_colour_texture(1, 0);

    for (const auto& vectortile : m_gpu_tiles) {
        if (!draw_tiles.contains(vectortile.first))
            continue; // tile is not in draw_tiles -> look at next tile

        // only draw if vector tile is fully loaded
        if (vectortile.second->instance_count > 0) {
            vectortile.second->vao->bind();
            m_picker_shader->set_uniform("reference_position", glm::vec3(vectortile.second->reference_point - camera.position()));

            f->glDrawElementsInstanced(GL_TRIANGLES, m_indices_count, GL_UNSIGNED_INT, 0, vectortile.second->instance_count);

            vectortile.second->vao->release();
        }
    }
}

unsigned MapLabels::tile_count() const { return unsigned(m_gpu_tiles.size()); }

} // namespace gl_engine
