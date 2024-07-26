/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include "MapLabelManager.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLPixelTransferOptions>
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif

#include "ShaderProgram.h"

#include "radix/tile.h"

#include <nucleus/srs.h>

using namespace Qt::Literals::StringLiterals;

namespace gl_engine {

MapLabelManager::MapLabelManager(QObject* parent)
    : QObject { parent }
{
    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END - 1; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;
        // initialize every type
        m_gpu_tiles[type] = std::unordered_map<tile::Id, std::shared_ptr<GPUVectorTile>, tile::Id::Hasher>();
    }
}

void MapLabelManager::init()
{
    // load the font texture
    const auto& label_meta = m_mapLabelFactory.create_label_meta();
    // const auto& font_atlas = m_mapLabelManager.font_atlas();
    m_font_texture = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::RG8);
    m_font_texture->setParams(Texture::Filter::MipMapLinear, Texture::Filter::Linear);
    m_font_texture->upload(label_meta.font_atlas);

    // load the icon texture
    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END - 1; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;
        const auto icon = (label_meta.icons.contains(type)) ? label_meta.icons.at(type) : label_meta.icons.at(nucleus::vectortile::FeatureType::ENUM_END);
        m_icon_texture[type] = std::make_unique<Texture>(Texture::Target::_2d, Texture::Format::RGBA8);
        m_icon_texture[type]->setParams(Texture::Filter::MipMapLinear, Texture::Filter::Linear);
        m_icon_texture[type]->upload(icon);
    }

    m_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    m_index_buffer->create();
    m_index_buffer->bind();
    m_index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_index_buffer->allocate(m_mapLabelFactory.indices.data(), m_mapLabelFactory.indices.size() * sizeof(unsigned int));
    m_indices_count = m_mapLabelFactory.indices.size();
}

void MapLabelManager::add_tile(const tile::Id& id, const nucleus::vectortile::FeatureType& type, const nucleus::vectortile::VectorTile& vector_tile)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    std::shared_ptr<GPUVectorTile> vectortile = std::make_shared<GPUVectorTile>();
    vectortile->id = id;

    vectortile->vao = std::make_unique<QOpenGLVertexArrayObject>();
    vectortile->vao->create();
    vectortile->vao->bind();

    { // vao state

        m_index_buffer->bind();

        vectortile->vertex_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        vectortile->vertex_buffer->create();
        vectortile->vertex_buffer->bind();
        vectortile->vertex_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);

        const auto features = vector_tile.at(type);
        const auto allLabels = m_mapLabelFactory.create_labels(features);

        vectortile->vertex_buffer->allocate(allLabels.data(), allLabels.size() * sizeof(nucleus::maplabel::VertexData));
        vectortile->instance_count = allLabels.size();

        QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

        // vertex positions
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::maplabel::VertexData), nullptr);
        f->glVertexAttribDivisor(0, 1); // buffer is active for 1 instance (for the whole quad)
        // uvs
        f->glEnableVertexAttribArray(1);
        f->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::maplabel::VertexData), (GLvoid*)(sizeof(glm::vec4)));
        f->glVertexAttribDivisor(1, 1); // buffer is active for 1 instance (for the whole quad)
        // world position
        f->glEnableVertexAttribArray(2);
        f->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(nucleus::maplabel::VertexData), (GLvoid*)((sizeof(glm::vec4) * 2)));
        f->glVertexAttribDivisor(2, 1); // buffer is active for 1 instance (for the whole quad)
        // label importance
        f->glEnableVertexAttribArray(3);
        f->glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(nucleus::maplabel::VertexData), (GLvoid*)((sizeof(glm::vec4) * 2 + (sizeof(glm::vec3)))));
        f->glVertexAttribDivisor(3, 1); // buffer is active for 1 instance (for the whole quad)
    }

    vectortile->vao->release();

    // add vector tile to gpu tiles
    m_gpu_tiles.at(type)[id] = vectortile;
}

void MapLabelManager::remove_tile(const tile::Id& tile_id)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END - 1; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

        // we can only remove something that exists
        if (!m_gpu_tiles.contains(type) || !m_gpu_tiles.at(type).contains(tile_id))
            continue;

        if (m_gpu_tiles.at(type)[tile_id]->vao)
            m_gpu_tiles.at(type)[tile_id]->vao->destroy(); // ecplicitly destroy vao

        m_gpu_tiles.at(type).erase(tile_id);
    }
}

void MapLabelManager::update_gpu_quads(
    const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    for (const auto& quad : new_quads) {
        for (const auto& tile : quad.tiles) {
            // test for validity
            if (!tile.vector_tile || tile.vector_tile->empty())
                continue;

            assert(tile.id.zoom_level < 100);

            for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END - 1; i++) {
                nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

                if (m_gpu_tiles.at(type).contains(tile.id))
                    continue; // no need to add it twice

                add_tile(tile.id, type, *tile.vector_tile);
            }
        }
    }
    for (const auto& quad : deleted_quads) {
        for (const auto& id : quad.children()) {
            remove_tile(id);
        }
    }
}

void MapLabelManager::draw(Framebuffer* gbuffer, ShaderProgram* shader_program, const nucleus::camera::Definition& camera,
    const nucleus::tile_scheduler::DrawListGenerator::TileSet draw_tiles) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glEnable(GL_BLEND);

    glm::mat4 inv_view_rot = glm::inverse(camera.local_view_matrix());
    shader_program->set_uniform("inv_view_rot", inv_view_rot);
    shader_program->set_uniform("label_dist_scaling", true);

    shader_program->set_uniform("texin_depth", 0);
    gbuffer->bind_colour_texture(1, 0);

    shader_program->set_uniform("font_sampler", 1);
    m_font_texture->bind(1);

    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END - 1; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

        shader_program->set_uniform("icon_sampler", 2);
        m_icon_texture.at(type)->bind(2);

        for (const auto& vectortile : m_gpu_tiles.at(type)) {
            // only draw if vector tile is fully loaded and is contained in draw_tiles
            if (vectortile.second->instance_count > 0 && draw_tiles.contains(vectortile.first)) {
                vectortile.second->vao->bind();

                // if the labels wouldn't collide, we could use an extra buffer, one draw call and
                // f->glBlendEquationSeparate(GL_MIN, GL_MAX);
                shader_program->set_uniform("drawing_outline", true);
                f->glDrawElementsInstanced(GL_TRIANGLES, m_indices_count, GL_UNSIGNED_INT, 0, vectortile.second->instance_count);
                shader_program->set_uniform("drawing_outline", false);
                f->glDrawElementsInstanced(GL_TRIANGLES, m_indices_count, GL_UNSIGNED_INT, 0, vectortile.second->instance_count);

                vectortile.second->vao->release();
            }
        }
    }
}

} // namespace gl_engine
