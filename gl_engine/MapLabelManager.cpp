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
#include "nucleus/map_label/MapLabel.h"

namespace gl_engine {

MapLabelManager::MapLabelManager()
{
}
void MapLabelManager::init()
{
    m_vao = std::make_unique<QOpenGLVertexArrayObject>();
    m_vao->create();
    m_vao->bind();

    m_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    m_index_buffer->create();
    m_index_buffer->bind();
    m_index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_index_buffer->allocate(m_mapLabelManager.indices().data(), m_mapLabelManager.indices().size() * sizeof(unsigned int));

    m_vertex_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_vertex_buffer->create();
    m_vertex_buffer->bind();
    m_vertex_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);

    std::vector<nucleus::MapLabel::VertexData> allLabels;
    for (const auto& label : m_mapLabelManager.labels()) {
        allLabels.insert(allLabels.end(), label.vertex_data().begin(), label.vertex_data().end());
    }

    m_vertex_buffer->allocate(allLabels.data(), allLabels.size() * sizeof(nucleus::MapLabel::VertexData));
    m_instance_count = allLabels.size();

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    // vertex positions
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::MapLabel::VertexData), nullptr);
    f->glVertexAttribDivisor(0, 1); // buffer is active for 1 instance (for the whole quad)
    // uvs
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::MapLabel::VertexData), (GLvoid*)(sizeof(glm::vec4)));
    f->glVertexAttribDivisor(1, 1); // buffer is active for 1 instance (for the whole quad)
    // world position
    f->glEnableVertexAttribArray(2);
    f->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(nucleus::MapLabel::VertexData), (GLvoid*)((sizeof(glm::vec4) * 2)));
    f->glVertexAttribDivisor(2, 1); // buffer is active for 1 instance (for the whole quad)
    // label importance
    f->glEnableVertexAttribArray(3);
    f->glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(nucleus::MapLabel::VertexData), (GLvoid*)((sizeof(glm::vec4) * 2 + (sizeof(glm::vec3)))));
    f->glVertexAttribDivisor(3, 1); // buffer is active for 1 instance (for the whole quad)

    m_vao->release();

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
    QImage icon = m_mapLabelManager.icon();
    m_icon_texture = std::make_unique<QOpenGLTexture>(icon);
    m_icon_texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    m_icon_texture->setMagnificationFilter(QOpenGLTexture::Linear);
}

void MapLabelManager::draw(Framebuffer* gbuffer, ShaderProgram* shader_program, const nucleus::camera::Definition& camera) const
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

    shader_program->set_uniform("icon_sampler", 2);
    m_icon_texture->bind(2);

    m_vao->bind();

    // if the labels wouldn't collide, we could use an extra buffer, one draw call and
    // f->glBlendEquationSeparate(GL_MIN, GL_MAX);
    shader_program->set_uniform("drawing_outline", true);
    f->glDrawElementsInstanced(GL_TRIANGLES, m_mapLabelManager.indices().size(), GL_UNSIGNED_INT, 0, m_instance_count);
    shader_program->set_uniform("drawing_outline", false);
    f->glDrawElementsInstanced(GL_TRIANGLES, m_mapLabelManager.indices().size(), GL_UNSIGNED_INT, 0, m_instance_count);

    m_vao->release();
}

} // namespace gl_engine
