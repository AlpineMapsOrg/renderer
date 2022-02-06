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

#include <QMoveEvent>
#include <array>

#include "GLWindow.h"
#include <QImage>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLExtraFunctions>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QOpenGLDebugLogger>
#include <QDebug>
#include <QRandomGenerator>


#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render_backend/Tile.h"
#include "render_backend/utils/terrain_mesh_index_generator.h"

namespace {
QMatrix4x4 toQtType(const glm::mat4& mat) {
  return QMatrix4x4(glm::value_ptr(mat)).transposed();
}
template <typename T>
int bufferLengthInBytes(const std::vector<T>& vec) {
  return int(vec.size() * sizeof(T));
}
}

GLWindow::GLWindow() : m_camera(glm::dvec3{-2.f, -2.f, 2.f}, {0.f, 0.f, 0.f})
{
  QTimer::singleShot(0, [this]() {this->update();});
}

GLWindow::~GLWindow()
{
  makeCurrent();
  delete m_program;
}

static const char *vertexShaderSource = R"(
  in highp float height;
  out lowp vec2 uv;
  uniform highp mat4 matrix;
  void main() {
     int n_edge_vertices = 64;
     int row = gl_VertexID / n_edge_vertices;
     int col = gl_VertexID - (row * n_edge_vertices);
     vec4 pos = vec4(col, row, height, 1.0);
     uv = vec2(float(col) / float(n_edge_vertices), float(row) / float(n_edge_vertices));
     gl_Position = matrix * pos;
  })";

static const char *fragmentShaderSource = R"(
  in lowp vec2 uv;
  out lowp vec4 out_Color;
  void main() {
     out_Color = vec4(uv, 0, 1);
  })";

QByteArray versionedShaderCode(const char *src)
{
  QByteArray versionedSrc;

  if (QOpenGLContext::currentContext()->isOpenGLES())
    versionedSrc.append(QByteArrayLiteral("#version 300 es\n"));
  else
    versionedSrc.append(QByteArrayLiteral("#version 330\n"));

  versionedSrc.append(src);
  return versionedSrc;
}

void GLWindow::initializeGL()
{
  QOpenGLDebugLogger *logger = new QOpenGLDebugLogger(this);
  logger->initialize();
  connect(logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
    qDebug() << message;
  });
  logger->disableMessages(QList<GLuint>({131185}));
  logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

  m_gl_paint_device = std::make_unique<QOpenGLPaintDevice>();
  m_tile_manager = std::make_unique<GLTileManager>();

  m_program = new QOpenGLShaderProgram(this);
  m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, versionedShaderCode(vertexShaderSource));
  m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, versionedShaderCode(fragmentShaderSource));
  {
    const auto success = m_program->link();
    Q_ASSERT(success);
  }

  const auto height_attr = m_program->attributeLocation("height");
  Q_ASSERT(height_attr != -1);
  m_tile_manager->setAttributeLocations({height_attr});
  m_matrixUniform = m_program->uniformLocation("matrix");
  Q_ASSERT(m_matrixUniform != GLuint(-1));

  Tile t;
  t.height_map = Raster<uint16_t>(64);
  t.bounds.max = {4., 4.};
  t.id.zoom_level = 0;
  t.id.coords = {0, 0};

  m_tile_manager->addTile(std::make_shared<Tile>(std::move(t)));



//  glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
}

void GLWindow::resizeGL(int w, int h)
{
  if (w == 0 || h == 0)
    return;
  const qreal retinaScale = devicePixelRatio();
  const int width = int(retinaScale * w);
  const int height = int(retinaScale * h);

  m_camera.setPerspectiveParams(45, width, height);
  m_gl_paint_device->setSize({w, h});
  m_gl_paint_device->setDevicePixelRatio(retinaScale);
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
  f->glViewport(0, 0, width, height);
  update();
}

void GLWindow::paintGL()
{
  m_frame_start = std::chrono::time_point_cast<ClockResolution>(Clock::now());

  QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();
  f->glClearColor(0.5, 0.5, 0.5, 1);

  f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  m_program->bind();

  m_program->setUniformValue(m_matrixUniform, toQtType(m_camera.viewProjectionMatrix()));

  const auto& tilesets = m_tile_manager->tiles();
  for (const auto& tileset : tilesets) {
    tileset.vao->bind();
    f->glDrawElements(GL_TRIANGLE_STRIP, tileset.gl_element_count,  tileset.gl_index_type, nullptr);
    tileset.vao->release();
  }
  m_program->release();

  m_frame_end = std::chrono::time_point_cast<ClockResolution>(Clock::now());
}

void GLWindow::paintOverGL()
{
  const auto frame_duration = (m_frame_end - m_frame_start);
  const auto frame_duration_float = double(frame_duration.count()) / 1000.;
  const auto frame_duration_text = QString("Last frame: %1ms, draw indicator: ")
                                       .arg(QString::asprintf("%04.1f", frame_duration_float));

  const auto random_u32 = QRandomGenerator::global()->generate();

  QPainter painter(m_gl_paint_device.get());
  painter.setFont(QFont("Helvetica", 12));
  painter.setPen(Qt::white);
  QRect text_bb = painter.boundingRect(10, 20, 1, 15, Qt::TextSingleLine, frame_duration_text);
  painter.drawText(10, 20, frame_duration_text);
  painter.setBrush(QBrush(QColor(random_u32)));
  painter.drawRect(int(text_bb.right()) + 5, 8, 12, 12);
}

void GLWindow::mouseMoveEvent(QMouseEvent* e)
{
  glm::ivec2 mouse_position{e->pos().x(), e->pos().y()};
  if (e->buttons() == Qt::LeftButton) {
    const auto delta = mouse_position - m_previous_mouse_pos;
    m_camera.pan(glm::vec2(delta) * 0.01f);
    update();
  }
  if (e->buttons() == Qt::MiddleButton) {
    const auto delta = mouse_position - m_previous_mouse_pos;
    m_camera.orbit(glm::vec2(delta) * 0.1f);
    update();
  }
  if (e->buttons() == Qt::RightButton) {
    const auto delta = mouse_position - m_previous_mouse_pos;
    m_camera.zoom(delta.y * 0.005);
    update();
  }
  m_previous_mouse_pos = mouse_position;
}
