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

#include "glwindow.h"
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


GLWindow::GLWindow()
{
  m_world.setToIdentity();
  m_world.translate(0, 0, -1);
  m_world.rotate(180, 1, 0, 0);
  QTimer::singleShot(0, [this]() {this->update();});
}

GLWindow::~GLWindow()
{
  makeCurrent();
  delete m_program;
}

static const char *vertexShaderSource = R"(
  attribute highp vec4 posAttr;
  attribute lowp vec4 colAttr;
  varying lowp vec4 col;
  uniform highp mat4 matrix;
  void main() {
     col = colAttr;
     gl_Position = matrix * posAttr;
  })";

static const char *fragmentShaderSource = R"(
  varying lowp vec4 col;
  void main() {
     gl_FragColor = col;
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
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

  m_gl_paint_device = std::make_unique<QOpenGLPaintDevice>();

  m_program = new QOpenGLShaderProgram(this);
  m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
  m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
  {
    const auto success = m_program->link();
    Q_ASSERT(success);
  }

  const auto posAttr = m_program->attributeLocation("posAttr");
  Q_ASSERT(posAttr != -1);
  const auto colAttr = m_program->attributeLocation("colAttr");
  Q_ASSERT(colAttr != -1);
  m_matrixUniform = m_program->uniformLocation("matrix");
  Q_ASSERT(m_matrixUniform != GLuint(-1));

  const std::array<GLfloat, 6> vertices = {
      0.0f,  0.707f,
      -0.5f, -0.5f,
      0.5f, -0.5f
  };
  static_assert(sizeof(vertices) == 6 * 4);

  const std::array<GLfloat, 9> colors = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f
  };
  static_assert(sizeof(colors) == 9 * 4);

  const std::array<GLuint, 3> indices = {
      0, 1, 2
  };
  static_assert(sizeof(indices) == 3 * 4);

  m_vao = std::make_unique<QOpenGLVertexArrayObject>();
  m_vao->create();
  m_vao->bind();
  {   // vao state
    m_position_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_position_buffer->create();
    m_position_buffer->bind();
    m_position_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_position_buffer->allocate(vertices.data(), sizeof(vertices));
    f->glEnableVertexAttribArray(GLuint(posAttr));
    f->glVertexAttribPointer(GLuint(posAttr), /*size*/ 2, /*type*/ GL_FLOAT, /*normalised*/ GL_FALSE, /*stride*/ 0, nullptr);

    m_colour_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_colour_buffer->create();
    m_colour_buffer->bind();
    m_colour_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_colour_buffer->allocate(colors.data(), sizeof(colors));
    f->glEnableVertexAttribArray(GLuint(colAttr));
    f->glVertexAttribPointer(GLuint(colAttr), /*size*/ 3, /*type*/ GL_FLOAT, /*normalised*/ GL_FALSE, /*stride*/ 0, nullptr);

    m_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    m_index_buffer->create();
    m_index_buffer->bind();
    m_index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_index_buffer->allocate(indices.data(), sizeof(indices));
  }
  m_vao->release();
}

void GLWindow::resizeGL(int w, int h)
{
  m_proj.setToIdentity();
  m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 100.0f);
  m_gl_paint_device->setSize({w, h});
  const qreal retinaScale = devicePixelRatio();
  qDebug("w = %i, h = %i, scale=%f", w, h, retinaScale);
  m_gl_paint_device->setDevicePixelRatio(retinaScale);
}

void GLWindow::paintGL()
{
  m_frame_start = std::chrono::time_point_cast<ClockResolution>(Clock::now());

  // Now use QOpenGLExtraFunctions instead of QOpenGLFunctions as we want to
  // do more than what GL(ES) 2.0 offers.
  QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

  f->glClearColor(0.1, 0, 0, 1);

  const qreal retinaScale = devicePixelRatio();
  f->glViewport(0, 0, width() * retinaScale, height() * retinaScale);

  //    f->glClear(GL_COLOR_BUFFER_BIT);
  f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  m_program->bind();

  QMatrix4x4 matrix;
  matrix.perspective(60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
  matrix.translate(0, 0, -2);
  matrix.rotate(100.0f * m_frame++ / 60.0f + 1, 0, 1, 0);

  m_program->setUniformValue(m_matrixUniform, matrix);

  m_vao->bind();
  f->glDrawElements(GL_TRIANGLES, /* count */ 3,  GL_UNSIGNED_INT, nullptr);
  m_vao->release();
  m_program->release();

  m_frame_end = std::chrono::time_point_cast<ClockResolution>(Clock::now());
  update();

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
  painter.setBrush(QBrush(Qt::transparent));
  painter.drawRect(100, 100, 1200, 1200);
  painter.drawText(100, 50, "Hello world with vaos!");
}
