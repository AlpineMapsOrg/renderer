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

#include <iostream>

using namespace Qt::Literals::StringLiterals;

namespace gl_engine {

MapLabelManager::MapLabelManager(QObject* parent)
    : QObject { parent }
{
    for (int i = 0; i < nucleus::FeatureType::ENUM_END - 1; i++) {
        nucleus::FeatureType type = (nucleus::FeatureType)i;
        // initialize every type
        m_gpu_tiles[type] = std::unordered_map<tile::Id, std::shared_ptr<GPUVectorTile>, tile::Id::Hasher>();
    }
}

void MapLabelManager::init()
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    // load the font texture
    const auto& font_atlas = m_mapLabelManager.font_atlas();
    m_font_texture = std::make_unique<Texture>(Texture::Target::_2d);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, font_atlas.width(), font_atlas.height(), 0, GL_RG, GL_UNSIGNED_BYTE, font_atlas.bytes());
    f->glGenerateMipmap(GL_TEXTURE_2D);

    // load the icon texture
    for (int i = 0; i < nucleus::FeatureType::ENUM_END - 1; i++) {
        nucleus::FeatureType type = (nucleus::FeatureType)i;
        QImage icon = m_mapLabelManager.icon(type);
        m_icon_texture[type] = std::make_unique<QOpenGLTexture>(icon);
        m_icon_texture[type]->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_icon_texture[type]->setMagnificationFilter(QOpenGLTexture::Linear);
    }

    ////////////////////////////////////
    /// TEST
    /// TODO remove

    // testTile = tile::Id { .zoom_level = 13, .coords = { 4384, 2878 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

    // std::shared_ptr<nucleus::FeatureTXTPeak> t = std::make_shared<nucleus::FeatureTXTPeak>();
    // t->name = u"Großglockner"_s;
    // t->position = nucleus::srs::lat_long_alt_to_world(glm::dvec3(47.07455, 12.69388, 3798));
    // t->elevation = 3798;
    // t->type = nucleus::FeatureType::Peak;
    // t->id = 1;

    // testLabels.insert(t);

    // GPUVectorTile vectortile;
    // vectortile.id = testTile;
    // vectortile.indices_count = 0; // this is used for checking if it is fully loaded and we dont want to risk possible undefined behaviour
    // // we are adding the "empty" vectortile so that it can be removed while waiting for vector tile network/calculations
    // m_gpu_tiles.at(nucleus::FeatureType::Peak)[testTile] = std::move(vectortile);

    // create_vao(testTile, testLabels);
}

void MapLabelManager::create_vao(const tile::Id id, const std::unordered_set<std::shared_ptr<nucleus::FeatureTXT>>& features)
{
    // std::cout << "create vao" << id << std::endl;
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;
    for (int i = 0; i < nucleus::FeatureType::ENUM_END - 1; i++) {
        nucleus::FeatureType type = (nucleus::FeatureType)i;

        if (!m_gpu_tiles.at(type).contains(id))
            continue; // tile was already removed again -> no need to create vao

        std::shared_ptr<GPUVectorTile> vectortile = m_gpu_tiles.at(type)[id];

        vectortile->vao = std::make_unique<QOpenGLVertexArrayObject>();
        vectortile->vao->create();
        vectortile->vao->bind();

        { // vao state
            // TODO index buffer doesnt change -> only make it once and bind it for every vao
            vectortile->index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
            vectortile->index_buffer->create();
            vectortile->index_buffer->bind();
            vectortile->index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
            vectortile->index_buffer->allocate(m_mapLabelManager.indices().data(), m_mapLabelManager.indices().size() * sizeof(unsigned int));
            vectortile->indices_count = m_mapLabelManager.indices().size();

            vectortile->vertex_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
            vectortile->vertex_buffer->create();
            vectortile->vertex_buffer->bind();
            vectortile->vertex_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);

            const auto allLabels = m_mapLabelManager.create_labels(features);

            vectortile->vertex_buffer->allocate(allLabels.data(), allLabels.size() * sizeof(nucleus::VertexData));
            vectortile->instance_count = allLabels.size();

            QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

            // vertex positions
            f->glEnableVertexAttribArray(0);
            f->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::VertexData), nullptr);
            f->glVertexAttribDivisor(0, 1); // buffer is active for 1 instance (for the whole quad)
            // uvs
            f->glEnableVertexAttribArray(1);
            f->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::VertexData), (GLvoid*)(sizeof(glm::vec4)));
            f->glVertexAttribDivisor(1, 1); // buffer is active for 1 instance (for the whole quad)
            // world position
            f->glEnableVertexAttribArray(2);
            f->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(nucleus::VertexData), (GLvoid*)((sizeof(glm::vec4) * 2)));
            f->glVertexAttribDivisor(2, 1); // buffer is active for 1 instance (for the whole quad)
            // label importance
            f->glEnableVertexAttribArray(3);
            f->glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(nucleus::VertexData), (GLvoid*)((sizeof(glm::vec4) * 2 + (sizeof(glm::vec3)))));
            f->glVertexAttribDivisor(3, 1); // buffer is active for 1 instance (for the whole quad)
        }

        vectortile->vao->release();
    }
}

void MapLabelManager::remove_tile(const tile::Id& tile_id)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    for (int i = 0; i < nucleus::FeatureType::ENUM_END - 1; i++) {
        nucleus::FeatureType type = (nucleus::FeatureType)i;

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
            assert(tile.id.zoom_level < 100);

            for (int i = 0; i < nucleus::FeatureType::ENUM_END - 1; i++) {
                nucleus::FeatureType type = (nucleus::FeatureType)i;

                if (m_gpu_tiles.at(type).contains(tile.id))
                    continue; // no need to add it twice

                std::shared_ptr<GPUVectorTile> vectortile = std::make_shared<GPUVectorTile>();
                vectortile->id = tile.id;
                vectortile->indices_count = 0; // this is used for checking if it is fully loaded and we dont want to risk possible undefined behaviour
                // we are adding the "empty" vectortile so that it can be removed while waiting for vector tile network/calculations
                m_gpu_tiles.at(type)[tile.id] = std::move(vectortile);
            }

            // we have to make sure that the vector tile is fully loaded (and that the height has been calculated)
            // -> we therefore use signals and slots
            emit added_tile(tile.id);
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

    for (int i = 0; i < nucleus::FeatureType::ENUM_END - 1; i++) {
        nucleus::FeatureType type = (nucleus::FeatureType)i;

        shader_program->set_uniform("icon_sampler", 2);
        m_icon_texture.at(type)->bind(2);

        for (const auto& vectortile : m_gpu_tiles.at(type)) {
            // only draw if vector tile is fully loaded and is contained in draw_tiles
            if (vectortile.second->indices_count > 0 && draw_tiles.contains(vectortile.first)) {

                vectortile.second->vao->bind();

                // if the labels wouldn't collide, we could use an extra buffer, one draw call and
                // f->glBlendEquationSeparate(GL_MIN, GL_MAX);
                shader_program->set_uniform("drawing_outline", true);
                f->glDrawElementsInstanced(GL_TRIANGLES, vectortile.second->indices_count, GL_UNSIGNED_INT, 0, vectortile.second->instance_count);
                shader_program->set_uniform("drawing_outline", false);
                f->glDrawElementsInstanced(GL_TRIANGLES, vectortile.second->indices_count, GL_UNSIGNED_INT, 0, vectortile.second->instance_count);

                vectortile.second->vao->release();
            }
        }
    }
}

} // namespace gl_engine
