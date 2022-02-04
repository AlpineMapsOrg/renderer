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
  delete m_vbo;
  delete m_vao;
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
    qDebug() << "\n";
  });
  logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

  static const GLfloat vertices[] = {
      0.0f,  0.707f,
      -0.5f, -0.5f,
      0.5f, -0.5f
  };

  m_gl_paint_device = std::make_unique<QOpenGLPaintDevice>();

  m_program = new QOpenGLShaderProgram(this);
  m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
  m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
  m_program->link();
  m_posAttr = m_program->attributeLocation("posAttr");
  Q_ASSERT(m_posAttr != -1);
  m_colAttr = m_program->attributeLocation("colAttr");
  Q_ASSERT(m_colAttr != -1);
  m_matrixUniform = m_program->uniformLocation("matrix");
  Q_ASSERT(m_matrixUniform != -1);
}

void GLWindow::resizeGL(int w, int h)
{
  m_proj.setToIdentity();
  m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 100.0f);
  m_gl_paint_device->setSize({w, h});
  m_gl_paint_device->setDevicePixelRatio(1);
}

void GLWindow::paintGL()
{
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

  static const GLfloat vertices[] = {
      0.0f,  0.707f,
      -0.5f, -0.5f,
      0.5f, -0.5f
  };

  static const GLfloat colors[] = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f
  };

  f->glVertexAttribPointer(m_posAttr, 2, GL_FLOAT, GL_FALSE, 0, vertices);
  f->glVertexAttribPointer(m_colAttr, 3, GL_FLOAT, GL_FALSE, 0, colors);

  f->glEnableVertexAttribArray(m_posAttr);
  f->glEnableVertexAttribArray(m_colAttr);

  f->glDrawArrays(GL_TRIANGLES, 0, 3);

  f->glDisableVertexAttribArray(m_colAttr);
  f->glDisableVertexAttribArray(m_posAttr);

  m_program->release();
  update();

}

void GLWindow::paintOverGL()
{
  QPainter painter(m_gl_paint_device.get());
  painter.setPen(Qt::white);
  painter.drawRect(100, 100, 1200, 1200);
  painter.drawText(100, 50, "Hello world!");
}
