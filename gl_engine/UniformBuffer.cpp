/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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
#include "UniformBuffer.h"
#include <QOpenGLExtraFunctions>
#include "ShaderProgram.h"
#include "UniformBufferObjects.h"
#include <QDebug>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#if defined(__ANDROID__)
#include <GLES3/gl3.h>  // for GL_UNIFORM_BUFFER! DONT EXACTLY KNOW WHY I NEED THIS HERE! (on other platforms it works without)
#endif

template <typename T>
gl_engine::UniformBuffer<T>::UniformBuffer(GLuint location, const std::string& name)
    : m_name(name)
    , m_location(location)
{
    m_f = QOpenGLContext::currentContext()->extraFunctions();
}

template <typename T>
gl_engine::UniformBuffer<T>::UniformBuffer::~UniformBuffer()
{
    m_f->glDeleteBuffers(1, &m_id);
}

template <typename T> void gl_engine::UniformBuffer<T>::init() {
    m_f->glGenBuffers(1, &m_id);
    m_f->glBindBuffer(GL_UNIFORM_BUFFER, m_id);

    //qDebug() << "Size of" << m_name << std::to_string(sizeof(T)) << "byte";

    m_f->glBufferData(GL_UNIFORM_BUFFER, sizeof(T), NULL, GL_STATIC_DRAW);
    m_f->glBindBuffer(GL_UNIFORM_BUFFER, 0);

    m_f->glBindBufferRange(GL_UNIFORM_BUFFER, m_location, m_id, 0, sizeof(T));
    this->update_gpu_data();
}

template <typename T> void gl_engine::UniformBuffer<T>::bind_to_shader(ShaderProgram* shader) {
    shader->set_uniform_block(m_name, m_location);
}

template <typename T> void gl_engine::UniformBuffer<T>::bind_to_shader(std::vector<ShaderProgram*> shader) {
    for (auto* sp : shader) {
        sp->set_uniform_block(m_name, m_location);
    }
}

template <typename T> void gl_engine::UniformBuffer<T>::update_gpu_data() {
    m_f->glBindBuffer(GL_UNIFORM_BUFFER, m_id);
    m_f->glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(T), &data);
    m_f->glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

template <typename T> QString gl_engine::UniformBuffer<T>::data_as_string() {
    return ubo_as_string(data);
}

template <typename T> bool gl_engine::UniformBuffer<T>::data_from_string(const QString& base64String) {
    bool result = true;
    auto newData = ubo_from_string<T>(base64String,&result);
    if (result) data = newData;
    return result;
}

// IMPORTANT: All possible Template Classes need to be defined here:
template class gl_engine::UniformBuffer<gl_engine::uboSharedConfig>;
template class gl_engine::UniformBuffer<gl_engine::uboCameraConfig>;
template class gl_engine::UniformBuffer<gl_engine::uboShadowConfig>;
template class gl_engine::UniformBuffer<gl_engine::uboTestConfig>;
