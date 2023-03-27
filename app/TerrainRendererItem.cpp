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

#include "TerrainRendererItem.h"

#include <memory>

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>

#include "RenderThreadNotifier.h"
#include "gl_engine/Window.h"
#include "nucleus/Controller.h"
#include "nucleus/camera/Controller.h"

#include <nucleus/tile_scheduler/GpuCacheTileScheduler.h>

namespace {
// helper type for the visitor from https://en.cppreference.com/w/cpp/utility/variant/visit
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}

class MyFrameBufferObjectRenderer : public QQuickFramebufferObject::Renderer {
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
        TerrainRendererItem* i = static_cast<TerrainRendererItem*>(item);
        m_controller->camera_controller()->set_virtual_resolution_factor(i->virtual_resolution_factor());
        m_controller->camera_controller()->set_field_of_view(i->field_of_view());
        const auto oc = m_controller->camera_controller()->get_operation_centre();
        if (oc.has_value()) {
            i->set_camera_operation_centre(QPointF(oc.value().x, oc.value().y));
        } else {
            i->set_camera_operation_centre(QPointF(0, 0));
        }
        if (!(i->camera() == m_controller->camera_controller()->definition())) {
            const auto tmp_camera = m_controller->camera_controller()->definition();
            QTimer::singleShot(0, i, [i, tmp_camera]() {
                i->set_read_only_camera(tmp_camera);
                i->set_read_only_frame_buffer_width(tmp_camera.viewport_size().x);
                i->set_read_only_frame_buffer_height(tmp_camera.viewport_size().y);
            });
        }
    }

    void render() Q_DECL_OVERRIDE
    {
        m_window->beginExternalCommands();
        m_glWindow->paint(this->framebufferObject());
        m_window->endExternalCommands();
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) Q_DECL_OVERRIDE
    {
        qDebug() << "QOpenGLFramebufferObject *createFramebufferObject(const QSize& " << size << ")";
        m_window->beginExternalCommands();
        m_glWindow->resize_framebuffer(size.width(), size.height());
        m_controller->camera_controller()->set_viewport({ size.width(), size.height() });
        m_window->endExternalCommands();
        QOpenGLFramebufferObjectFormat format;
        format.setSamples(1);
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, format);
    }

    [[nodiscard]] gl_engine::Window* glWindow() const
    {
        return m_glWindow.get();
    }

    [[nodiscard]] nucleus::Controller* controller() const
    {
        return m_controller.get();
    }

private:
    QQuickWindow *m_window;
    std::unique_ptr<gl_engine::Window> m_glWindow;
    std::unique_ptr<nucleus::Controller> m_controller;
};

// MyFrameBufferObject implementation

TerrainRendererItem::TerrainRendererItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
    , m_update_timer(new QTimer(this))
{
    m_update_timer->setSingleShot(true);
    m_update_timer->setInterval(1000 / m_frame_limit);
    qDebug("MyFrameBufferObject::MyFrameBufferObject(QQuickItem* parent)");
    qDebug() << "gui thread: " << QThread::currentThread();
    setMirrorVertically(true);
    setAcceptTouchEvents(true);
    setAcceptedMouseButtons(Qt::MouseButton::AllButtons);
}

TerrainRendererItem::~TerrainRendererItem()
{
    qDebug("MyFrameBufferObject::~MyFrameBufferObject()");
}

QQuickFramebufferObject::Renderer* TerrainRendererItem::createRenderer() const
{
    qDebug("QQuickFramebufferObject::Renderer* MyFrameBufferObject::createRenderer() const");
    qDebug() << "rendering thread: " << QThread::currentThread();
    // called on rendering thread.
    auto* r = new MyFrameBufferObjectRenderer();
    connect(r->glWindow(), &nucleus::AbstractRenderWindow::update_requested, this, &TerrainRendererItem::schedule_update);
    connect(m_update_timer, &QTimer::timeout, this, &QQuickFramebufferObject::update);

    connect(this, &TerrainRendererItem::touch_made, r->controller()->camera_controller(), &nucleus::camera::Controller::touch);
    connect(this, &TerrainRendererItem::mouse_pressed, r->controller()->camera_controller(), &nucleus::camera::Controller::mouse_press);
    connect(this, &TerrainRendererItem::mouse_moved, r->controller()->camera_controller(), &nucleus::camera::Controller::mouse_move);
    connect(this, &TerrainRendererItem::wheel_turned, r->controller()->camera_controller(), &nucleus::camera::Controller::wheel_turn);
    connect(this, &TerrainRendererItem::key_pressed, r->controller()->camera_controller(), &nucleus::camera::Controller::key_press);
    connect(this, &TerrainRendererItem::key_released, r->controller()->camera_controller(), &nucleus::camera::Controller::key_release);
    connect(this, &TerrainRendererItem::position_set_by_user, r->controller()->camera_controller(), &nucleus::camera::Controller::set_latitude_longitude);

    connect(r->controller()->tile_scheduler(), &nucleus::tile_scheduler::GpuCacheTileScheduler::tile_ready, RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);
    connect(r->controller()->tile_scheduler(), &nucleus::tile_scheduler::GpuCacheTileScheduler::tile_expired, RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);

    connect(m_timer, &QTimer::timeout, this, &TerrainRendererItem::key_timer);
    return r;
}

void TerrainRendererItem::touchEvent(QTouchEvent* e)
{
    emit touch_made(nucleus::event_parameter::make(e));
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::mousePressEvent(QMouseEvent* e)
{
    emit mouse_pressed(nucleus::event_parameter::make(e));
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::mouseMoveEvent(QMouseEvent* e)
{
    emit mouse_moved(nucleus::event_parameter::make(e));
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::wheelEvent(QWheelEvent* e)
{
    emit wheel_turned(nucleus::event_parameter::make(e));
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::keyPressEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) {
        return;
    }
    m_keys_pressed++;
    if (!m_timer->isActive()) {
        m_timer->start(1000.0f/30.0f);
    }
    emit key_pressed(e->keyCombination());
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::keyReleaseEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) {
        return;
    }
    m_keys_pressed--;
    if (m_keys_pressed <= 0) {
        m_timer->stop();
    }
    emit key_released(e->keyCombination());
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::key_timer()
{
    emit key_pressed(QKeyCombination(Qt::Key_T)); // TODO replace this with "key update" call
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::set_position(double latitude, double longitude)
{
    emit position_set_by_user(latitude, longitude);
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::schedule_update()
{
    //    qDebug("void MyFrameBufferObject::schedule_update()");
    if (m_update_timer->isActive())
        return;
    m_update_timer->start();
}

int TerrainRendererItem::frame_limit() const
{
    return m_frame_limit;
}

void TerrainRendererItem::set_frame_limit(int new_frame_limit)
{
    new_frame_limit = std::clamp(new_frame_limit, 1, 120);
    if (m_frame_limit == new_frame_limit)
        return;
    m_frame_limit = new_frame_limit;
    m_update_timer->setInterval(1000 / m_frame_limit);
    emit frame_limit_changed();
}

float TerrainRendererItem::virtual_resolution_factor() const
{
    return m_virtual_resolution_factor;
}

void TerrainRendererItem::set_virtual_resolution_factor(float new_virtual_resolution_factor)
{
    if (qFuzzyCompare(m_virtual_resolution_factor, new_virtual_resolution_factor))
        return;
    m_virtual_resolution_factor = new_virtual_resolution_factor;
    emit virtual_resolution_factor_changed();
    schedule_update();
}

nucleus::camera::Definition TerrainRendererItem::camera() const
{
    return m_camera;
}

void TerrainRendererItem::set_read_only_camera(const nucleus::camera::Definition& new_camera)
{
    // the camera is controlled by the rendering thread (i.e., movement, projection parameters, viewport size etc).
    // this method is only for copying the camera data to the gui thread for consumptino
    if (m_camera == new_camera)
        return;
    m_camera = new_camera;
    emit camera_changed();
}

int TerrainRendererItem::frame_buffer_width() const
{
    return m_frame_buffer_width;
}

void TerrainRendererItem::set_read_only_frame_buffer_width(int new_frame_buffer_width)
{
    if (m_frame_buffer_width == new_frame_buffer_width)
        return;
    m_frame_buffer_width = new_frame_buffer_width;
    emit frame_buffer_width_changed();
}

int TerrainRendererItem::frame_buffer_height() const
{
    return m_frame_buffer_height;
}

void TerrainRendererItem::set_read_only_frame_buffer_height(int new_frame_buffer_height)
{
    if (m_frame_buffer_height == new_frame_buffer_height)
        return;
    m_frame_buffer_height = new_frame_buffer_height;
    emit frame_buffer_height_changed();
}

float TerrainRendererItem::field_of_view() const
{
    return m_field_of_view;
}

void TerrainRendererItem::set_field_of_view(float new_field_of_view)
{
    if (qFuzzyCompare(m_field_of_view, new_field_of_view))
        return;
    m_field_of_view = new_field_of_view;
    emit field_of_view_changed();
    schedule_update();
}

QPointF TerrainRendererItem::camera_operation_centre() const
{
    return m_camera_operation_centre;
}

void TerrainRendererItem::set_camera_operation_centre(QPointF new_camera_operation_centre)
{
    if (m_camera_operation_centre == new_camera_operation_centre)
        return;
    m_camera_operation_centre = new_camera_operation_centre;
    emit camera_operation_centre_changed();
}
