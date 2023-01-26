/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <array>
#include <iostream>

#include <QDebug>
#include <QImage>
#include <QMoveEvent>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <glm/glm.hpp>

#include "Atmosphere.h"
#include "DebugPainter.h"
#include "Framebuffer.h"
#include "ShaderManager.h"
#include "ShaderProgram.h"
#include "TileManager.h"
#include "Window.h"
#include "helpers.h"

using gl_engine::Window;

Window::Window()
    : m_camera({ 1822577.0, 6141664.0 - 500, 171.28 + 500 }, { 1822577.0, 6141664.0, 171.28 }) // should point right at the stephansdom
{
    qDebug("Window::Window()");
    m_tile_manager = std::make_unique<TileManager>();
    QTimer::singleShot(1, [this]() { emit update_requested(); });
}

Window::~Window()
{
    qDebug("~Window::Window()");
}

void Window::initialise_gpu()
{
    QOpenGLDebugLogger* logger = new QOpenGLDebugLogger(this);
    logger->initialize();
    connect(logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
        qDebug() << message;
    });
    logger->disableMessages(QList<GLuint>({ 131185 }));
    logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);

    m_debug_painter = std::make_unique<DebugPainter>();
    m_shader_manager = std::make_unique<ShaderManager>();
    m_atmosphere = std::make_unique<Atmosphere>();

    m_tile_manager->init();
    m_tile_manager->initilise_attribute_locations(m_shader_manager->tile_shader());
    m_screen_quad_geometry = gl_engine::helpers::create_screen_quad_geometry();
    m_framebuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::Int24, std::vector({ Framebuffer::ColourFormat::RGBA8 }));
}

void Window::resize(int w, int h, qreal device_pixel_ratio)
{
    if (w == 0 || h == 0)
        return;
    const int width = int(device_pixel_ratio * w);
    const int height = int(device_pixel_ratio * h);

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    m_framebuffer->resize({ width, height });
    m_atmosphere->resize({ width, height });

    f->glViewport(0, 0, width, height);
}

void Window::paint(QOpenGLFramebufferObject* framebuffer)
{
    m_frame_start = std::chrono::time_point_cast<ClockResolution>(Clock::now());
    m_camera.set_viewport_size(m_framebuffer->size());

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    m_framebuffer->bind();
    f->glClearColor(1.0, 0.0, 0.5, 1);

    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LESS);
////    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    m_shader_manager->tile_shader()->bind();

    m_tile_manager->draw(m_shader_manager->tile_shader(), m_camera);

    //    {
    //        m_shader_manager->bindDebugShader();
    //        m_debug_painter->activate(m_shader_manager->debugShader(), world_view_projection_matrix);
    //        const auto position = m_camera.position();
    //        const auto direction_tl = m_camera.ray_direction({ -1, 1 });
    //        const auto direction_tr = m_camera.ray_direction({ 1, 1 });
    //        std::vector<glm::vec3> debug_cam_lines = { position + direction_tl * 10000.0,
    //            position,
    //            position + direction_tr * 10000.0 };
    //        m_debug_painter->drawLineStrip(debug_cam_lines);
    //    }
    m_shader_manager->atmosphere_bg_program()->bind();
    m_atmosphere->draw(m_shader_manager->atmosphere_bg_program(), m_camera, m_shader_manager->screen_quad_program(), m_framebuffer.get());

    m_framebuffer->unbind();
    if (framebuffer)
        framebuffer->bind();

    m_shader_manager->screen_quad_program()->bind();
    m_framebuffer->bind_colour_texture(0);
    m_screen_quad_geometry.draw();

    m_shader_manager->release();

//        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    f->glFinish(); // synchronization
    m_frame_end = std::chrono::time_point_cast<ClockResolution>(Clock::now());
}

void Window::paintOverGL(QPainter* painter)
{
    const auto frame_duration = (m_frame_end - m_frame_start);
    const auto frame_duration_float = double(frame_duration.count()) / 1000.;
    const auto frame_duration_text = QString("Last frame: %1ms, draw indicator: ")
                                         .arg(QString::asprintf("%04.1f", frame_duration_float));

    const auto random_u32 = QRandomGenerator::global()->generate();

    painter->setFont(QFont("Helvetica", 12));
    painter->setPen(Qt::white);
    QRect text_bb = painter->boundingRect(10, 20, 1, 15, Qt::TextSingleLine, frame_duration_text);
    painter->drawText(10, 20, frame_duration_text);
    painter->drawText(10, 40, m_debug_scheduler_stats);
    painter->drawText(10, 60, m_debug_text);
    painter->setBrush(QBrush(QColor(random_u32)));
    painter->drawRect(int(text_bb.right()) + 5, 8, 12, 12);
}

void Window::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key::Key_F5) {
        m_shader_manager->reload_shaders();
        qDebug("all shaders reloaded");
        emit update_requested();
    }
    if (e->key() == Qt::Key::Key_F11
        || (e->key() == Qt::Key_P && e->modifiers() == Qt::ControlModifier)
        || (e->key() == Qt::Key_F5 && e->modifiers() == Qt::ControlModifier)) {
        e->ignore();
    }

    emit key_pressed(e->keyCombination());
}
void Window::update_camera(const nucleus::camera::Definition& new_definition)
{
    qDebug("void Window::update_camera(const nucleus::camera::Definition& new_definition)");
    m_camera = new_definition;
    emit update_requested();
}

void Window::update_debug_scheduler_stats(const QString& stats)
{
    m_debug_scheduler_stats = stats;
    emit update_requested();
}
glm::dvec3 Window::ray_cast(const glm::dvec2& normalised_device_coordinates)
{
    return m_camera.position() + m_camera.ray_direction(normalised_device_coordinates) * 500.;
}

void Window::deinit_gpu()
{
    m_tile_manager.reset();
    m_debug_painter.reset();
    m_atmosphere.reset();
    m_shader_manager.reset();
    m_framebuffer.reset();
    m_screen_quad_geometry = {};
}

void Window::set_aabb_decorator(const nucleus::tile_scheduler::AabbDecoratorPtr& new_aabb_decorator)
{
    assert(m_tile_manager);
    m_tile_manager->set_aabb_decorator(new_aabb_decorator);
}

void Window::add_tile(const std::shared_ptr<nucleus::Tile>& tile)
{
    assert(m_tile_manager);
    m_tile_manager->add_tile(tile);
}

void Window::remove_tile(const tile::Id& id)
{
    assert(m_tile_manager);
    m_tile_manager->remove_tile(id);
}
