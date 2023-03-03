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

#pragma once

#include <QOpenGLPaintDevice>
#include <QOpenGLWindow>
#include <QPainter>
#include <QVector3D>
#include <chrono>
#include <glm/glm.hpp>
#include <memory>

#include "helpers.h"
#include "nucleus/AbstractRenderWindow.h"
#include "nucleus/camera/AbstractDepthTester.h"
#include "nucleus/camera/Definition.h"

class QOpenGLTexture;
class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
class TileManager;

namespace gl_engine {
class DebugPainter;
class ShaderManager;
class Framebuffer;
class Atmosphere;

class Window : public nucleus::AbstractRenderWindow, public nucleus::camera::AbstractDepthTester {
    Q_OBJECT
public:
    Window();
    ~Window() override;

    void initialise_gpu() override;
    void resize_framebuffer(int w, int h) override;
    void paint(QOpenGLFramebufferObject* framebuffer = nullptr) override;
    void paintOverGL(QPainter* painter);

    [[nodiscard]] float depth(const glm::dvec2& normalised_device_coordinates) override;
    [[nodiscard]] glm::dvec3 position(const glm::dvec2& normalised_device_coordinates) override;
    void deinit_gpu() override;
    void set_aabb_decorator(const nucleus::tile_scheduler::AabbDecoratorPtr&) override;
    void add_tile(const std::shared_ptr<nucleus::Tile>&) override;
    void remove_tile(const tile::Id&) override;
    [[nodiscard]] nucleus::camera::AbstractDepthTester* depth_tester() override;
    void keyPressEvent(QKeyEvent*);

public slots:
    void update_camera(const nucleus::camera::Definition& new_definition) override;
    void update_debug_scheduler_stats(const QString& stats) override;

private:
    using ClockResolution = std::chrono::microseconds;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock, ClockResolution>;

    std::unique_ptr<TileManager> m_tile_manager; // needs opengl context
    std::unique_ptr<DebugPainter> m_debug_painter; // needs opengl context
    std::unique_ptr<Atmosphere> m_atmosphere; // needs opengl context
    std::unique_ptr<ShaderManager> m_shader_manager;
    std::unique_ptr<Framebuffer> m_framebuffer;
    std::unique_ptr<Framebuffer> m_depth_buffer;
    gl_engine::helpers::ScreenQuadGeometry m_screen_quad_geometry;

    nucleus::camera::Definition m_camera;

    int m_frame = 0;
    bool m_initialised = false;
    TimePoint m_frame_start;
    TimePoint m_frame_end;
    QString m_debug_text;
    QString m_debug_scheduler_stats;
};
}
