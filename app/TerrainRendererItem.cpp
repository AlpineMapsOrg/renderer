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
#include "TerrainRenderer.h"
#include "gl_engine/Window.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/Controller.h"
#include <nucleus/tile_scheduler/Scheduler.h>

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

TerrainRendererItem::TerrainRendererItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
    , m_update_timer(new QTimer(this))
{
    m_update_timer->setSingleShot(true);
    m_update_timer->setInterval(1000 / m_frame_limit);
    qDebug("TerrainRendererItem::TerrainRendererItem(QQuickItem* parent)");
    qDebug() << "gui thread: " << QThread::currentThread();
    setMirrorVertically(true);
    setAcceptTouchEvents(true);
    setAcceptedMouseButtons(Qt::MouseButton::AllButtons);

    connect(m_update_timer, &QTimer::timeout, this, [this]() {
        emit update_camera_requested();
        RenderThreadNotifier::instance()->notify();
    });
}

TerrainRendererItem::~TerrainRendererItem()
{
    qDebug("TerrainRendererItem::~TerrainRendererItem()");
}

QQuickFramebufferObject::Renderer* TerrainRendererItem::createRenderer() const
{
    qDebug("QQuickFramebufferObject::Renderer* TerrainRendererItem::createRenderer() const");
    qDebug() << "rendering thread: " << QThread::currentThread();
    // called on rendering thread.
    auto* r = new TerrainRenderer();
    connect(r->glWindow(), &nucleus::AbstractRenderWindow::update_requested, this, &TerrainRendererItem::schedule_update);
    connect(m_update_timer, &QTimer::timeout, this, &QQuickFramebufferObject::update);

    connect(this, &TerrainRendererItem::touch_made, r->controller()->camera_controller(), &nucleus::camera::Controller::touch);
    connect(this, &TerrainRendererItem::mouse_pressed, r->controller()->camera_controller(), &nucleus::camera::Controller::mouse_press);
    connect(this, &TerrainRendererItem::mouse_moved, r->controller()->camera_controller(), &nucleus::camera::Controller::mouse_move);
    connect(this, &TerrainRendererItem::wheel_turned, r->controller()->camera_controller(), &nucleus::camera::Controller::wheel_turn);
    connect(this, &TerrainRendererItem::key_pressed, r->controller()->camera_controller(), &nucleus::camera::Controller::key_press);
    connect(this, &TerrainRendererItem::key_released, r->controller()->camera_controller(), &nucleus::camera::Controller::key_release);
    connect(this, &TerrainRendererItem::update_camera_requested, r->controller()->camera_controller(), &nucleus::camera::Controller::update_camera_request);
    connect(this, &TerrainRendererItem::position_set_by_user, r->controller()->camera_controller(), &nucleus::camera::Controller::fly_to_latitude_longitude);
    connect(this,
            &TerrainRendererItem::rotation_north_requested,
            r->controller()->camera_controller(),
            &nucleus::camera::Controller::rotate_north);

    auto* const tile_scheduler = r->controller()->tile_scheduler();
    connect(this, &TerrainRendererItem::render_quality_changed, tile_scheduler, [=](float new_render_quality) {
        const auto permissible_error = 1.0f / new_render_quality;
        tile_scheduler->set_permissible_screen_space_error(permissible_error);
    });
    connect(this, &TerrainRendererItem::tile_cache_size_changed, tile_scheduler, &nucleus::tile_scheduler::Scheduler::set_ram_quad_limit);
    connect(tile_scheduler, &nucleus::tile_scheduler::Scheduler::quads_requested, this, [this](const std::vector<tile::Id>& ids) {
        const_cast<TerrainRendererItem*>(this)->set_queued_tiles(ids.size());
    });
    connect(tile_scheduler, &nucleus::tile_scheduler::Scheduler::quad_received, this, [this]() {
        const_cast<TerrainRendererItem*>(this)->set_queued_tiles(std::max(this->queued_tiles(), 1u) - 1);
    });
    connect(tile_scheduler, &nucleus::tile_scheduler::Scheduler::statistics_updated, this, [this](const nucleus::tile_scheduler::Scheduler::Statistics& stats) {
        const_cast<TerrainRendererItem*>(this)->set_cached_tiles(stats.n_tiles_in_ram_cache);
    });

    connect(tile_scheduler, &nucleus::tile_scheduler::Scheduler::gpu_quads_updated, RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);
    return r;
}

void TerrainRendererItem::touchEvent(QTouchEvent* e)
{
    this->setFocus(true);
    emit touch_made(nucleus::event_parameter::make(e));
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::mousePressEvent(QMouseEvent* e)
{
    this->setFocus(true);
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
    this->setFocus(true);
    emit wheel_turned(nucleus::event_parameter::make(e));
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::keyPressEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) {
        return;
    }
    emit key_pressed(e->keyCombination());
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::keyReleaseEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) {
        return;
    }
    emit key_released(e->keyCombination());
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::set_position(double latitude, double longitude)
{
    emit position_set_by_user(latitude, longitude);
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::rotate_north()
{
    emit rotation_north_requested();
    RenderThreadNotifier::instance()->notify();
}

void TerrainRendererItem::schedule_update()
{
    //    qDebug("void TerrainRendererItem::schedule_update()");
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

int TerrainRendererItem::camera_width() const
{
    return m_camera_width;
}

void TerrainRendererItem::set_read_only_camera_width(int new_camera_width)
{
    if (m_camera_width == new_camera_width)
        return;
    m_camera_width = new_camera_width;
    emit camera_width_changed();
}

int TerrainRendererItem::camera_height() const
{
    return m_camera_height;
}

void TerrainRendererItem::set_read_only_camera_height(int new_camera_height)
{
    if (m_camera_height == new_camera_height)
        return;
    m_camera_height = new_camera_height;
    emit camera_height_changed();
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

float TerrainRendererItem::camera_rotation_from_north() const
{
    return m_camera_rotation_from_north;
}

void TerrainRendererItem::set_camera_rotation_from_north(float new_camera_rotation_from_north)
{
    if (qFuzzyCompare(m_camera_rotation_from_north, new_camera_rotation_from_north))
        return;
    m_camera_rotation_from_north = new_camera_rotation_from_north;
    emit camera_rotation_from_north_changed();
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

bool TerrainRendererItem::camera_operation_centre_visibility() const
{
    return m_camera_operation_centre_visibility;
}

void TerrainRendererItem::set_camera_operation_centre_visibility(bool new_camera_operation_centre_visibility)
{
    if (m_camera_operation_centre_visibility == new_camera_operation_centre_visibility)
        return;
    m_camera_operation_centre_visibility = new_camera_operation_centre_visibility;
    emit camera_operation_centre_visibility_changed();
}

float TerrainRendererItem::camera_operation_centre_distance() const
{
    return m_camera_operation_centre_distance;
}

void TerrainRendererItem::set_camera_operation_centre_distance(float new_camera_operation_centre_distance)
{
    if (m_camera_operation_centre_distance == new_camera_operation_centre_distance)
        return;
    m_camera_operation_centre_distance = new_camera_operation_centre_distance;
    emit camera_operation_centre_distance_changed();
}

float TerrainRendererItem::render_quality() const
{
    return m_render_quality;
}

void TerrainRendererItem::set_render_quality(float new_render_quality)
{
    if (qFuzzyCompare(m_render_quality, new_render_quality))
        return;
    m_render_quality = new_render_quality;
    emit render_quality_changed(new_render_quality);
    schedule_update();
}

unsigned int TerrainRendererItem::in_flight_tiles() const
{
    return m_in_flight_tiles;
}

void TerrainRendererItem::set_in_flight_tiles(unsigned int new_in_flight_tiles)
{
    if (m_in_flight_tiles == new_in_flight_tiles)
        return;
    m_in_flight_tiles = new_in_flight_tiles;
    emit in_flight_tiles_changed(m_in_flight_tiles);
}

unsigned int TerrainRendererItem::queued_tiles() const
{
    return m_queued_tiles;
}

void TerrainRendererItem::set_queued_tiles(unsigned int new_queued_tiles)
{
    if (m_queued_tiles == new_queued_tiles)
        return;
    m_queued_tiles = new_queued_tiles;
    emit queued_tiles_changed(m_queued_tiles);
}

unsigned int TerrainRendererItem::cached_tiles() const
{
    return m_cached_tiles;
}

void TerrainRendererItem::set_cached_tiles(unsigned int new_cached_tiles)
{
    if (m_cached_tiles == new_cached_tiles)
        return;
    m_cached_tiles = new_cached_tiles;
    emit cached_tiles_changed(m_cached_tiles);
}

unsigned int TerrainRendererItem::tile_cache_size() const
{
    return m_tile_cache_size;
}

void TerrainRendererItem::set_tile_cache_size(unsigned int new_tile_cache_size)
{
    if (m_tile_cache_size == new_tile_cache_size)
        return;
    m_tile_cache_size = new_tile_cache_size;
    emit tile_cache_size_changed(m_tile_cache_size);
}
