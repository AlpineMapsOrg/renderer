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
#include <nucleus/map_label/FontRenderer.h>

#include <chrono>

using namespace Qt::Literals::StringLiterals;

namespace gl_engine {

MapLabelManager::MapLabelManager(QObject* parent)
    : QObject { parent }
{
}

void MapLabelManager::init()
{
    // load the font texture
    const auto& atlas_data = m_mapLabelFactory.init_font_atlas();

    m_font_texture = std::make_unique<Texture>(Texture::Target::_2dArray, Texture::Format::RG8);
    m_font_texture->allocate_array(nucleus::maplabel::FontRenderer::font_atlas_size.width(), nucleus::maplabel::FontRenderer::font_atlas_size.height(), nucleus::maplabel::FontRenderer::max_textures);
    for(unsigned int i = 0; i < atlas_data.font_atlas.size(); i++)
    {
        m_font_texture->upload(atlas_data.font_atlas[i],i);
    }

    const auto& labelIcons = m_mapLabelFactory.get_label_icons();

    // load the icon texture
    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;
        QImage icon = (labelIcons.contains(type)) ? labelIcons.at(type) : labelIcons.at(nucleus::vectortile::FeatureType::ENUM_END);
        m_icon_texture[type] = std::make_unique<QOpenGLTexture>(icon);
        m_icon_texture[type]->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_icon_texture[type]->setMagnificationFilter(QOpenGLTexture::Linear);
    }

    m_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    m_index_buffer->create();
    m_index_buffer->bind();
    m_index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_index_buffer->allocate(m_mapLabelFactory.indices.data(), m_mapLabelFactory.indices.size() * sizeof(unsigned int));
    m_indices_count = m_mapLabelFactory.indices.size();
}

void MapLabelManager::renew_font_atlas()
{
    const auto& atlas_data = m_mapLabelFactory.renew_font_atlas();

    if(atlas_data.changed)
    {
        for(unsigned int i = 0; i < atlas_data.font_atlas.size(); i++)
        {
            m_font_texture->upload(atlas_data.font_atlas[i],i);
        }
    }
}

void MapLabelManager::add_tile(const tile::Id& id, const nucleus::vectortile::VectorTile& vector_tile)
{
    m_filter.add_tile(id, vector_tile);
}

void MapLabelManager::upload_to_gpu(const tile::Id& id,  const std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>>& features)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

    // TODO this is a one to one copy from remove_tile without remove filter... -> find a better solution...
    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

               // we can only remove something that exists
        if (!m_gpu_tiles.contains(id) || !m_gpu_tiles.at(id).contains(type))
            continue;

        if (m_gpu_tiles.at(id)[type]->vao)
            m_gpu_tiles.at(id)[type]->vao->destroy(); // ecplicitly destroy vao

        m_gpu_tiles.at(id).erase(type);
    }// TODO end

    // initialize map
    m_gpu_tiles[id] = std::unordered_map<nucleus::vectortile::FeatureType, std::shared_ptr<GPUVectorTile>>();

    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

        std::shared_ptr<GPUVectorTile> vectortile = std::make_shared<GPUVectorTile>();
        vectortile->id = id;

        if(features.contains(type))
        {
            vectortile->vao = std::make_unique<QOpenGLVertexArrayObject>();
            vectortile->vao->create();
            vectortile->vao->bind();

            { // vao state
                m_index_buffer->bind();

                vectortile->vertex_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
                vectortile->vertex_buffer->create();
                vectortile->vertex_buffer->bind();
                vectortile->vertex_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);

                const auto allLabels = m_mapLabelFactory.create_labels(features.at(type));

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
                // texture index
                f->glEnableVertexAttribArray(4);
                f->glVertexAttribIPointer(4, 1, GL_INT, sizeof(nucleus::maplabel::VertexData), (GLvoid*)((sizeof(glm::vec4) * 2 + (sizeof(glm::vec3)) + sizeof(float))));
                f->glVertexAttribDivisor(4, 1); // buffer is active for 1 instance (for the whole quad)
            }

            vectortile->vao->release();

        }

        // add vector tile to gpu tiles
        m_gpu_tiles.at(id)[type] = vectortile;
    }

}

void MapLabelManager::filter_and_upload()
{
//    auto start = std::chrono::system_clock::now();

    auto filtered_features = m_filter.filter();

    for (const auto& vectortile : filtered_features)
    {
        upload_to_gpu(vectortile.first, vectortile.second);
    }

//    std::chrono::duration<double> elapsed_seconds = std::chrono::system_clock::now()-start;
//    std::cout << "filter: " << elapsed_seconds.count() << std::endl;

}

void MapLabelManager::remove_tile(const tile::Id& tile_id)
{
    if (!QOpenGLContext::currentContext()) // can happen during shutdown.
        return;

     m_filter.remove_tile(tile_id);

    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

        // we can only remove something that exists
        if (!m_gpu_tiles.contains(tile_id) || !m_gpu_tiles.at(tile_id).contains(type))
            continue;

        if (m_gpu_tiles.at(tile_id)[type]->vao)
            m_gpu_tiles.at(tile_id)[type]->vao->destroy(); // ecplicitly destroy vao

        m_gpu_tiles.at(tile_id).erase(type);
    }

}

void MapLabelManager::update_gpu_quads(
    const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    renew_font_atlas();

    for (const auto& quad : new_quads) {
        for (const auto& tile : quad.tiles) {
            // test for validity
            if (!tile.vector_tile || tile.vector_tile->empty())
                continue;

            assert(tile.id.zoom_level < 100);

            if (m_gpu_tiles.contains(tile.id))
                continue; // no need to add it twice

            add_tile(tile.id, *tile.vector_tile);
        }
    }
    for (const auto& quad : deleted_quads) {
        for (const auto& id : quad.children()) {
            remove_tile(id);
        }
    }

    filter_and_upload();
}

void MapLabelManager::update_filter(const FilterDefinitions& filter_definitions)
{
    m_filter.update_filter(filter_definitions);
    filter_and_upload();
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

    for (const auto& vectortile : m_gpu_tiles) {
        if(!draw_tiles.contains(vectortile.first))
            continue; // tile is not in draw_tiles -> look at next tile

        for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END; i++) {
            nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;
            if(!vectortile.second.contains(type))
                continue; // type is empty -> look at next type

            shader_program->set_uniform("icon_sampler", 2);
            m_icon_texture.at(type)->bind(2);

            const auto& gpu_tile = vectortile.second.at(type);

            // only draw if vector tile is fully loaded
            if (gpu_tile->instance_count > 0) {
                gpu_tile->vao->bind();

                // if the labels wouldn't collide, we could use an extra buffer, one draw call and
                // f->glBlendEquationSeparate(GL_MIN, GL_MAX);
                shader_program->set_uniform("drawing_outline", true);
                f->glDrawElementsInstanced(GL_TRIANGLES, m_indices_count, GL_UNSIGNED_INT, 0, gpu_tile->instance_count);
                shader_program->set_uniform("drawing_outline", false);
                f->glDrawElementsInstanced(GL_TRIANGLES, m_indices_count, GL_UNSIGNED_INT, 0, gpu_tile->instance_count);

                gpu_tile->vao->release();
            }
        }
    }
}

} // namespace gl_engine
