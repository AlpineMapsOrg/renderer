/****************************************************************************
**
** Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company.
** Author: Giuseppe D'Angelo
** Contact: info@kdab.com
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include "myframebufferobject.h"

#include <memory>

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>

#include "gl_engine/Window.h"
#include "nucleus/Controller.h"

class MyFrameBufferObjectRenderer : public QQuickFramebufferObject::Renderer
{
    float m_azimuth = 0.4;
    float m_elevation = 0.4;
    float m_distance = 0.4;

public:
    MyFrameBufferObjectRenderer()
    {
        m_glWindow = std::make_unique<gl_engine::Window>();
        m_glWindow->initialise_gpu();
        m_controller = std::make_unique<nucleus::Controller>(m_glWindow.get());
        qDebug("MyFrameBufferObjectRenderer()");
    }
    ~MyFrameBufferObjectRenderer() override
    {
        qDebug("~MyFrameBufferObjectRenderer()");
    }

    void synchronize(QQuickFramebufferObject *item) Q_DECL_OVERRIDE
    {
        m_window = item->window();
        MyFrameBufferObject* i = static_cast<MyFrameBufferObject*>(item);
        m_azimuth = i->azimuth();
        m_elevation = i->elevation();
        m_distance = i->distance();
        for (const auto& p : i->m_touch_events) {
            m_glWindow->touch_made(p);
        }

        i->m_touch_events.clear();
        //        m_glWindow->update_requested();
    }

    void render() Q_DECL_OVERRIDE
    {
        m_window->beginExternalCommands();
        QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
        f->glClearColor(m_azimuth, m_elevation, m_distance, 1);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        m_glWindow->paint(this->framebufferObject());
        m_window->endExternalCommands();
        //        update();
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) Q_DECL_OVERRIDE
    {
        qDebug() << "QOpenGLFramebufferObject *createFramebufferObject(const QSize& " << size << ")";
        m_window->beginExternalCommands();
        m_glWindow->resize(size.width(), size.height(), 1.0);
        m_window->endExternalCommands();
        QOpenGLFramebufferObjectFormat format;
        format.setSamples(1);
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, format);
    }

    gl_engine::Window* glWindow() const;

private:
    QQuickWindow *m_window;
    std::unique_ptr<gl_engine::Window> m_glWindow;
    std::unique_ptr<nucleus::Controller> m_controller;
};



// MyFrameBufferObject implementation

MyFrameBufferObject::MyFrameBufferObject(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
    , m_azimuth(0.0)
    , m_elevation(0.0)
    , m_distance(0.5)
{
    qDebug("MyFrameBufferObject::MyFrameBufferObject(QQuickItem* parent)");
    setMirrorVertically(true);
    setAcceptTouchEvents(true);
}

MyFrameBufferObject::~MyFrameBufferObject()
{
    qDebug("MyFrameBufferObject::~MyFrameBufferObject()");
}

QQuickFramebufferObject::Renderer* MyFrameBufferObject::createRenderer() const
{
    auto* r = new MyFrameBufferObjectRenderer;
    connect(r->glWindow(), &nucleus::AbstractRenderWindow::update_requested, this, &QQuickFramebufferObject::update);
    qRegisterMetaType<nucleus::event_parameter::Touch>();
    //    connect(
    //        this, &MyFrameBufferObject::touch_made, r->glWindow(), []() { qDebug("touch d"); }, Qt::QueuedConnection);
    //    connect(this, &MyFrameBufferObject::touch_made, r->glWindow(), &nucleus::AbstractRenderWindow::touch_made);
    //    connect(this, &MyFrameBufferObject::touch_made, r->glWindow(), &nucleus::AbstractRenderWindow::update_requested);
    return r;
}

float MyFrameBufferObject::azimuth() const
{
    return m_azimuth;
}

float MyFrameBufferObject::distance() const
{
    return m_distance;
}

float MyFrameBufferObject::elevation() const
{
    return m_elevation;
}

void MyFrameBufferObject::touchEvent(QTouchEvent* e)
{
    m_touch_events.push_back(nucleus::event_parameter::make(e));
    update();
    //    emit touch_made(nucleus::event_parameter::make(e));
}

void MyFrameBufferObject::setAzimuth(float azimuth)
{
    if (m_azimuth == azimuth)
        return;

    m_azimuth = azimuth;
    emit azimuthChanged(azimuth);
    update();
}

void MyFrameBufferObject::setDistance(float distance)
{
    if (m_distance == distance)
        return;

    m_distance = distance;
    emit distanceChanged(distance);
    update();
}

void MyFrameBufferObject::setElevation(float elevation)
{
    if (m_elevation == elevation)
        return;

    m_elevation = elevation;
    emit elevationChanged(elevation);
    update();
}

gl_engine::Window* MyFrameBufferObjectRenderer::glWindow() const
{
    return m_glWindow.get();
}
