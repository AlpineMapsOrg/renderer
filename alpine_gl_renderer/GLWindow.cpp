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
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <glm/glm.hpp>

#include "Atmosphere.h"
#include "Framebuffer.h"
#include "GLDebugPainter.h"
#include "GLHelpers.h"
#include "GLShaderManager.h"
#include "GLTileManager.h"
#include "GLWindow.h"
#include "ShaderProgram.h"
#include "nucleus/TileScheduler.h"

GLWindow::GLWindow()
    : m_camera({ 1822577.0, 6141664.0 - 500, 171.28 + 500 }, { 1822577.0, 6141664.0, 171.28 }) // should point right at the stephansdom
{
    m_tile_manager = std::make_unique<GLTileManager>();
    QTimer::singleShot(0, [this]() { this->update(); });

}

GLWindow::~GLWindow()
{
    makeCurrent();
}

void GLWindow::initializeGL()
{
    QOpenGLDebugLogger* logger = new QOpenGLDebugLogger(this);
    logger->initialize();
    connect(logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
        qDebug() << message;
    });
    logger->disableMessages(QList<GLuint>({ 131185 }));
    logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);

    m_debug_painter = std::make_unique<GLDebugPainter>();
    m_shader_manager = std::make_unique<GLShaderManager>();
    m_atmosphere = std::make_unique<Atmosphere>();

    m_tile_manager->init();
    m_tile_manager->initiliseAttributeLocations(m_shader_manager->tileShader());
    m_screen_quad_geometry = gl_helpers::create_screen_quad_geometry();
    m_framebuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::Int24, std::vector({ Framebuffer::ColourFormat::RGBA8 }));
}

void GLWindow::resizeGL(int w, int h)
{
    if (w == 0 || h == 0)
        return;
    const qreal retinaScale = devicePixelRatio();
    const int width = int(retinaScale * w);
    const int height = int(retinaScale * h);

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    m_framebuffer->resize({ width, height });
    m_atmosphere->resize({ width, height });

    f->glViewport(0, 0, width, height);
    emit viewport_changed({ w, h });
}

void GLWindow::paintGL()
{
    m_frame_start = std::chrono::time_point_cast<ClockResolution>(Clock::now());

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    m_framebuffer->bind();
    f->glClearColor(1.0, 0.0, 0.5, 1);

    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LESS);
////    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    m_shader_manager->tileShader()->bind();

    m_tile_manager->draw(m_shader_manager->tileShader(), m_camera);

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

    m_shader_manager->screen_quad_program()->bind();
    m_framebuffer->bind_colour_texture(0);
    m_screen_quad_geometry.draw();

    m_shader_manager->release();

//        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    f->glFinish(); // synchronization
    m_frame_end = std::chrono::time_point_cast<ClockResolution>(Clock::now());
}

void GLWindow::paintOverGL()
{
    const auto frame_duration = (m_frame_end - m_frame_start);
    const auto frame_duration_float = double(frame_duration.count()) / 1000.;
    const auto frame_duration_text = QString("Last frame: %1ms, draw indicator: ")
                                         .arg(QString::asprintf("%04.1f", frame_duration_float));

    const auto random_u32 = QRandomGenerator::global()->generate();

    QPainter painter(this);
    painter.setFont(QFont("Helvetica", 12));
    painter.setPen(Qt::white);
    QRect text_bb = painter.boundingRect(10, 20, 1, 15, Qt::TextSingleLine, frame_duration_text);
    painter.drawText(10, 20, frame_duration_text);
    painter.drawText(10, 40, m_debug_scheduler_stats);
    painter.drawText(10, 60, m_debug_text);
    painter.setBrush(QBrush(QColor(random_u32)));
    painter.drawRect(int(text_bb.right()) + 5, 8, 12, 12);
}

void GLWindow::mouseMoveEvent(QMouseEvent* e)
{
    // send depth information only on mouse press to be more efficient (?)
    emit mouse_moved(e);
}

void GLWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key::Key_F5) {
        m_shader_manager->reload_shaders();
        update();
        qDebug("all shaders reloaded");
    }

    emit key_pressed(e->keyCombination());
}

void GLWindow::touchEvent(QTouchEvent* ev)
{
    if (ev->isEndEvent()) {
        m_debug_text = "";
        return;
    }
    m_debug_text = "touches: ";
    for (const auto& point : ev->points()) {
        m_debug_text.append(QString("%1:%2/%3; ").arg(point.id()).arg(point.position().x()).arg(point.position().y()));
    }
    update();
    emit touch_made(ev);
}

void GLWindow::update_camera(const camera::Definition& new_definition)
{
    m_camera = new_definition;
    update();
}

void GLWindow::update_debug_scheduler_stats(const QString& stats)
{
    m_debug_scheduler_stats = stats;
    update();
}

void GLWindow::mousePressEvent(QMouseEvent* ev)
{
    float distance = 500;   // todo read and compute from depth buffer
    emit mouse_pressed(ev, distance);
}

GLTileManager* GLWindow::gpuTileManager() const
{
    return m_tile_manager.get();
}
