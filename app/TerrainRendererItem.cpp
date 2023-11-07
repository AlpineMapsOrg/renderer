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

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include "RenderThreadNotifier.h"
#include "TerrainRenderer.h"
#include "gl_engine/Window.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/Controller.h"
#include <nucleus/tile_scheduler/Scheduler.h>

#include "nucleus/srs.h" // for calculation of lat long from cam pos
#include "nucleus/utils/sun_calculations.h"
#include "nucleus/camera/PositionStorage.h"
#include "nucleus/utils/UrlModifier.h"

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
    m_timer_manager = new TimerFrontendManager();
    m_update_timer->setSingleShot(true);
    m_update_timer->setInterval(1000 / m_frame_limit);
    qDebug("TerrainRendererItem::TerrainRendererItem(QQuickItem* parent)");
    qDebug() << "gui thread: " << QThread::currentThread();
    setMirrorVertically(true);
    setAcceptTouchEvents(true);
    setAcceptedMouseButtons(Qt::MouseButton::AllButtons);

    // INITIALIZE shared config with URL parameter:
    auto urlmodifier = nucleus::utils::UrlModifier::get();
    bool param_available = false;
    auto config_base64_string = urlmodifier->get_query_item(URL_PARAMETER_KEY_CONFIG, &param_available);
    if (param_available) {
        qInfo() << "Initialize config with:" << config_base64_string;
        auto tmp = gl_engine::ubo_from_string<gl_engine::uboSharedConfig>(config_base64_string);
        set_shared_config(tmp);
    }

    selected_datetime_changed(QDateTime());

    connect(m_update_timer, &QTimer::timeout, this, [this]() {
        emit update_camera_requested();
        RenderThreadNotifier::instance()->notify();
    });
}

TerrainRendererItem::~TerrainRendererItem()
{
    delete m_timer_manager;
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

    // Connect definition change to aquire camera position for sun angle calculation
    connect(r->controller()->camera_controller(), &nucleus::camera::Controller::definition_changed, this, &TerrainRendererItem::camera_definition_changed);

    connect(this, &TerrainRendererItem::camera_definition_set_by_user, r->controller()->camera_controller(), &nucleus::camera::Controller::set_definition);
    connect(r->controller()->camera_controller(), &nucleus::camera::Controller::global_cursor_position_changed, this, &TerrainRendererItem::read_global_position);
    //connect(this, &TerrainRendererItem::ind)

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

    // connect glWindow to forward key events.
    connect(this, &TerrainRendererItem::key_pressed, r->glWindow(), &gl_engine::Window::key_press);
    connect(this, &TerrainRendererItem::shared_config_changed, r->glWindow(), &gl_engine::Window::shared_config_changed);
    connect(this, &TerrainRendererItem::render_looped_changed, r->glWindow(), &gl_engine::Window::render_looped_changed);
    // connect glWindow for shader hotreload by frontend button
    connect(this, &TerrainRendererItem::reload_shader, r->glWindow(), &gl_engine::Window::reload_shader);

    connect(r->glWindow(), &gl_engine::Window::report_measurements, this->m_timer_manager, &TimerFrontendManager::receive_measurements);

    connect(r->controller()->tile_scheduler(), &nucleus::tile_scheduler::Scheduler::gpu_quads_updated, RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);
    connect(tile_scheduler, &nucleus::tile_scheduler::Scheduler::gpu_quads_updated, RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);

    // Maybe shared config is already different (by loading from url)
    // so lets notify the Renderer here to replace the current configuration!
    emit shared_config_changed(m_shared_config);

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
    if (e->key() == Qt::Key::Key_H) {
        set_hud_visible(!m_hud_visible);
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

void TerrainRendererItem::read_global_position(glm::dvec3 latlonalt) {
    emit gui_update_global_cursor_pos(latlonalt.x, latlonalt.y, latlonalt.z);
}

void TerrainRendererItem::camera_definition_changed(const nucleus::camera::Definition& new_definition)
{
    m_last_camera_latlonalt = nucleus::srs::world_to_lat_long_alt(new_definition.position());
    recalculate_sun_angles();
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

bool TerrainRendererItem::render_looped() const {
    return m_render_looped;
}

void TerrainRendererItem::set_render_looped(bool new_render_looped) {
    if (new_render_looped == m_render_looped) return;
    m_render_looped = new_render_looped;
    emit render_looped_changed(m_render_looped);
    schedule_update();
}

gl_engine::uboSharedConfig TerrainRendererItem::shared_config() const {
    return m_shared_config;
}

void TerrainRendererItem::set_shared_config(gl_engine::uboSharedConfig new_shared_config) {
    if (m_shared_config != new_shared_config) {
        m_shared_config = new_shared_config;
        // Lets update the URL: (TODO: schedule URL update, we don't need to update all the time)
        auto data_string = gl_engine::ubo_as_string(m_shared_config);
        nucleus::utils::UrlModifier::get()->set_query_item(URL_PARAMETER_KEY_CONFIG, data_string);
        emit shared_config_changed(m_shared_config);
    }
}

void TerrainRendererItem::set_hud_visible(bool new_hud_visible) {
    if (new_hud_visible == m_hud_visible) return;
    m_hud_visible = new_hud_visible;
    emit hud_visible_changed(m_hud_visible);
}

void TerrainRendererItem::set_selected_camera_position_index(unsigned value) {

    schedule_update();
    emit camera_definition_set_by_user(nucleus::camera::PositionStorage::instance()->get_by_index(value));
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

QDateTime TerrainRendererItem::selected_datetime() const
{
    return m_selected_datetime;
}

void TerrainRendererItem::set_selected_datetime(QDateTime new_datetime)
{
    if (m_selected_datetime != new_datetime) {
        m_selected_datetime = new_datetime;
        emit selected_datetime_changed(m_selected_datetime);
        recalculate_sun_angles();
    }
}

QVector2D TerrainRendererItem::sun_angles() const {
    return m_sun_angles;
}

void TerrainRendererItem::set_sun_angles(QVector2D new_sunAngles) {
    if (new_sunAngles == m_sun_angles) return;
    m_sun_angles = new_sunAngles;
    emit sun_angles_changed(m_sun_angles);

    // Update the direction inside the gls shared_config
    auto newDir = nucleus::utils::sun_calculations::sun_rays_direction_from_sun_angles(glm::vec2(m_sun_angles.x(), m_sun_angles.y()));
    QVector4D newDirUboEntry(newDir.x, newDir.y, newDir.z, m_shared_config.m_sun_light_dir.w());
    m_shared_config.m_sun_light_dir = newDirUboEntry;
    emit shared_config_changed(m_shared_config);
}

void TerrainRendererItem::set_link_gl_sundirection(bool newValue) {
    if (newValue == m_link_gl_sundirection) return;
    m_link_gl_sundirection = newValue;
    emit link_gl_sundirection_changed(m_link_gl_sundirection);
}

void TerrainRendererItem::recalculate_sun_angles() {
    // Calculate sun angles
    if (m_link_gl_sundirection) {
        auto angles = nucleus::utils::sun_calculations::calculate_sun_angles(m_selected_datetime, m_last_camera_latlonalt);
        set_sun_angles(QVector2D(angles.x, angles.y));
    }
}
