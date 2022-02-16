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

#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <chrono>
#include <memory>
#include <glm/glm.hpp>
#include <QOpenGLWindow>
#include <QVector3D>
#include <QOpenGLPaintDevice>
#include <QPainter>

#include "render_backend/Camera.h"

QT_BEGIN_NAMESPACE

class QOpenGLTexture;
class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
QT_END_NAMESPACE

class GLTileManager;
class GLShaderManager;

class GLWindow : public QOpenGLWindow
{
  Q_OBJECT
public:
  GLWindow();
  ~GLWindow() override;

  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
  void paintOverGL() override;

  GLTileManager* gpuTileManager() const;

protected:
  void mouseMoveEvent(QMouseEvent*) override;

signals:
  void cameraUpdated(const Camera&);

private:
  using ClockResolution = std::chrono::microseconds;
  using Clock = std::chrono::steady_clock;
  using TimePoint = std::chrono::time_point<Clock, ClockResolution>;

  std::unique_ptr<GLTileManager> m_tile_manager; // needs opengl context
  std::unique_ptr<GLShaderManager> m_shader_manager;
  std::unique_ptr<QOpenGLPaintDevice> m_gl_paint_device;

  Camera m_camera;
  glm::ivec2 m_previous_mouse_pos = {-1, -1};

  int m_frame = 0;
  bool m_initialised = false;
  TimePoint m_frame_start;
  TimePoint m_frame_end;
};

#endif
