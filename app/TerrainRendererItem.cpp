/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company (Giuseppe D'Angelo)
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Jakob Maier
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "TerrainRendererItem.h"

#include "RenderThreadNotifier.h"
#include "RenderingContext.h"
#include "TerrainRenderer.h"
#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>
#include <gl_engine/Window.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <memory>
#include <nucleus/camera/Controller.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/map_label/Filter.h>
#include <nucleus/map_label/Scheduler.h>
#include <nucleus/picker/PickerManager.h>
#include <nucleus/srs.h>
#include <nucleus/tile/GeometryScheduler.h>
#include <nucleus/tile/TextureScheduler.h>
#include <nucleus/utils/UrlModifier.h>
#include <nucleus/utils/sun_calculations.h>

#ifdef ALP_ENABLE_DEV_TOOLS
#include "TimerFrontendManager.h"
#endif

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
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("TerrainRendererItem()");
#endif
    qDebug("TerrainRendererItem()");
    qDebug() << "GUI thread: " << QThread::currentThread();

    m_url_modifier = std::make_shared<nucleus::utils::UrlModifier>();

    m_settings = new AppSettings(this, m_url_modifier);
    m_tile_statistics = new TileStatistics(this);
    connect(m_settings, &AppSettings::datetime_changed, this, &TerrainRendererItem::datetime_changed);
    connect(m_settings, &AppSettings::gl_sundir_date_link_changed, this, &TerrainRendererItem::gl_sundir_date_link_changed);
    connect(m_settings, &AppSettings::render_quality_changed, this, &TerrainRendererItem::schedule_update);
    connect(RenderThreadNotifier::instance(), &RenderThreadNotifier::redraw_requested, this, &TerrainRendererItem::schedule_update);

    m_update_timer->setSingleShot(!m_continuous_update);
    m_update_timer->setInterval(m_redraw_delay);
    setMirrorVertically(true);
    setAcceptTouchEvents(true);
    setAcceptedMouseButtons(Qt::MouseButton::AllButtons);

    connect(m_update_timer, &QTimer::timeout, this, [this]() {
        emit update_camera_requested();
        RenderThreadNotifier::instance()->notify();
    });

    connect(this, &TerrainRendererItem::init_after_creation, this, &TerrainRendererItem::init_after_creation_slot);
}


TerrainRendererItem::~TerrainRendererItem()
{
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("~TerrainRendererItem()");
#endif
}

QQuickFramebufferObject::Renderer* TerrainRendererItem::createRenderer() const
{
    qDebug("QQuickFramebufferObject::Renderer* TerrainRendererItem::createRenderer() const");
    qDebug() << "Rendering thread: " << QThread::currentThread();
    // called on rendering thread.
    auto ctx = RenderingContext::instance();

    auto* r = new TerrainRenderer();
    connect(r->glWindow(), &nucleus::AbstractRenderWindow::update_requested, this, &TerrainRendererItem::schedule_update);
    connect(r->glWindow(), &gl_engine::Window::tile_stats_ready, this->m_tile_statistics, &TileStatistics::set_gpu_stats);
    connect(ctx->geometry_scheduler(), &nucleus::tile::Scheduler::stats_ready, this->m_tile_statistics, &TileStatistics::update_scheduler_stats);
    connect(ctx->map_label_scheduler(), &nucleus::tile::Scheduler::stats_ready, this->m_tile_statistics, &TileStatistics::update_scheduler_stats);
    connect(ctx->ortho_scheduler(), &nucleus::tile::Scheduler::stats_ready, this->m_tile_statistics, &TileStatistics::update_scheduler_stats);
    connect(m_update_timer, &QTimer::timeout, this, &QQuickFramebufferObject::update);

    connect(this, &TerrainRendererItem::touch_made, r->controller(), &nucleus::camera::Controller::touch);
    connect(this, &TerrainRendererItem::mouse_pressed, r->controller(), &nucleus::camera::Controller::mouse_press);
    connect(this, &TerrainRendererItem::mouse_moved, r->controller(), &nucleus::camera::Controller::mouse_move);
    connect(this, &TerrainRendererItem::wheel_turned, r->controller(), &nucleus::camera::Controller::wheel_turn);
    connect(this, &TerrainRendererItem::key_pressed, r->controller(), &nucleus::camera::Controller::key_press);
    connect(this, &TerrainRendererItem::key_released, r->controller(), &nucleus::camera::Controller::key_release);
    connect(this, &TerrainRendererItem::update_camera_requested, r->controller(), &nucleus::camera::Controller::advance_camera);
    connect(this, &TerrainRendererItem::position_set_by_user, r->controller(), &nucleus::camera::Controller::fly_to_latitude_longitude);
    connect(this, &TerrainRendererItem::rotation_north_requested, r->controller(), &nucleus::camera::Controller::rotate_north);

    connect(this, &TerrainRendererItem::touch_made, ctx->picker_manager().get(), &nucleus::picker::PickerManager::touch_event);
    connect(this, &TerrainRendererItem::mouse_pressed, ctx->picker_manager().get(), &nucleus::picker::PickerManager::mouse_press_event);
    connect(this, &TerrainRendererItem::mouse_released, ctx->picker_manager().get(), &nucleus::picker::PickerManager::mouse_release_event);
    connect(this, &TerrainRendererItem::mouse_moved, ctx->picker_manager().get(), &nucleus::picker::PickerManager::mouse_move_event);

    // Connect definition change to aquire camera position for sun angle calculation
    connect(r->controller(), &nucleus::camera::Controller::definition_changed, this, &TerrainRendererItem::camera_definition_changed);
    connect(r->controller(), &nucleus::camera::Controller::global_cursor_position_changed, this, &TerrainRendererItem::set_world_space_cursor_position);

    connect(this, &TerrainRendererItem::camera_definition_set_by_user, r->controller(), &nucleus::camera::Controller::set_model_matrix);
    connect(this, &TerrainRendererItem::tile_cache_size_changed, ctx->geometry_scheduler(), &nucleus::tile::Scheduler::set_ram_quad_limit);
    connect(this, &TerrainRendererItem::tile_cache_size_changed, ctx->ortho_scheduler(), &nucleus::tile::Scheduler::set_ram_quad_limit);
    connect(this, &TerrainRendererItem::tile_cache_size_changed, ctx->map_label_scheduler(), &nucleus::tile::Scheduler::set_ram_quad_limit);

    // connect glWindow to forward key events.
    connect(this, &TerrainRendererItem::shared_config_changed, r->glWindow(), &gl_engine::Window::shared_config_changed);

    // connect glWindow for shader hotreload by frontend button
    connect(this, &TerrainRendererItem::reload_shader, r->glWindow(), &gl_engine::Window::reload_shader);

    connect(this, &TerrainRendererItem::label_filter_changed, ctx->label_filter().get(), &nucleus::map_label::Filter::update_filter);
    connect(ctx->picker_manager().get(), &nucleus::picker::PickerManager::pick_evaluated, this, &TerrainRendererItem::set_picked_feature);

#ifdef ALP_ENABLE_DEV_TOOLS
    connect(r->glWindow(), &gl_engine::Window::timer_measurements_ready, TimerFrontendManager::instance(), &TimerFrontendManager::receive_measurements);
#endif

    // We now have to initialize everything based on the url, but we need to do this on the thread this instance
    // belongs to. (gui thread?) Therefore we use the following signal to signal the init process
    emit init_after_creation();

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

void TerrainRendererItem::mouseReleaseEvent(QMouseEvent* e)
{
    this->setFocus(true);
    emit mouse_released(nucleus::event_parameter::make(e));
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

void TerrainRendererItem::camera_definition_changed(const nucleus::camera::Definition& new_definition)
{
    auto camPositionWS = new_definition.position();
    m_last_camera_latlonalt = nucleus::srs::world_to_lat_long_alt(camPositionWS);

    // Calculate an arbitrary lookat point infront of the camera
    glm::dvec3 lookAtPosition = new_definition.calculate_lookat_position(1000.0);
    m_last_camera_lookat_latlonalt = nucleus::srs::world_to_lat_long_alt(lookAtPosition);

    auto campos_as_string = nucleus::utils::UrlModifier::latlonalt_to_urlsafe_string(m_last_camera_latlonalt);
    auto lookat_as_string = nucleus::utils::UrlModifier::latlonalt_to_urlsafe_string(m_last_camera_lookat_latlonalt);
    m_url_modifier->set_query_item(URL_PARAMETER_KEY_CAM_POS, campos_as_string);
    m_url_modifier->set_query_item(URL_PARAMETER_KEY_CAM_LOOKAT, lookat_as_string);

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

void TerrainRendererItem::set_gl_preset(const QString& preset_b64_string) {
    qInfo() << "Override config with:" << preset_b64_string;
    auto tmp = gl_engine::ubo_from_string<gl_engine::uboSharedConfig>(preset_b64_string);
    update_gl_sun_dir_from_sun_angles(tmp);
    set_shared_config(tmp);
}

void TerrainRendererItem::schedule_update()
{
    //    qDebug("void TerrainRendererItem::schedule_update()");
    if (m_update_timer->isActive())
        return;
    m_update_timer->start();
}

int TerrainRendererItem::redraw_delay() const { return m_redraw_delay; }

void TerrainRendererItem::set_redraw_delay(int new_delay)
{
    new_delay = std::clamp(new_delay, 0, 50);
    if (m_redraw_delay == new_delay)
        return;
    m_redraw_delay = new_delay;
    m_update_timer->setInterval(m_redraw_delay);
    emit redraw_delay_changed();
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

gl_engine::uboSharedConfig TerrainRendererItem::shared_config() const { return m_shared_config; }

void TerrainRendererItem::set_shared_config(gl_engine::uboSharedConfig new_shared_config) {
    if (m_shared_config != new_shared_config) {
        m_shared_config = new_shared_config;
        auto data_string = gl_engine::ubo_as_string(m_shared_config);
        m_url_modifier->set_query_item(URL_PARAMETER_KEY_CONFIG, data_string);
        emit shared_config_changed(m_shared_config);
    }
}

nucleus::map_label::FilterDefinitions TerrainRendererItem::label_filter() const { return m_label_filter; }
void TerrainRendererItem::set_label_filter(nucleus::map_label::FilterDefinitions new_label_filter)
{
    if (m_label_filter != new_label_filter) {
        m_label_filter = new_label_filter;
        emit label_filter_changed(m_label_filter);
    }
}

void TerrainRendererItem::set_selected_camera_position_index(unsigned value) {
    qDebug() << "TerrainRendererItem::set_selected_camera_position_index(unsigned value): " << value;
    if (value > 100)
        return;
    schedule_update();
    emit camera_definition_set_by_user(nucleus::camera::PositionStorage::instance()->get_by_index(value));
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

QVector2D TerrainRendererItem::sun_angles() const {
    return m_sun_angles;
}

void TerrainRendererItem::set_sun_angles(QVector2D new_sunAngles) {
    if (new_sunAngles == m_sun_angles) return;
    m_sun_angles = new_sunAngles;
    emit sun_angles_changed(m_sun_angles);

    // Update the direction inside the gls shared_config
    update_gl_sun_dir_from_sun_angles(m_shared_config);
    emit shared_config_changed(m_shared_config);
}

void TerrainRendererItem::recalculate_sun_angles() {
    auto angles = nucleus::utils::sun_calculations::calculate_sun_angles(m_settings->datetime(), m_last_camera_latlonalt);
    set_sun_angles(QVector2D(angles.x, angles.y));
}

void TerrainRendererItem::update_gl_sun_dir_from_sun_angles(gl_engine::uboSharedConfig& ubo) {
    auto newDir = nucleus::utils::sun_calculations::sun_rays_direction_from_sun_angles(glm::vec2(m_sun_angles.x(), m_sun_angles.y()));
    QVector4D newDirUboEntry(newDir.x, newDir.y, newDir.z, ubo.m_sun_light_dir.w());
    ubo.m_sun_light_dir = newDirUboEntry;
}

const QVector3D& TerrainRendererItem::world_space_cursor_position() const { return m_world_space_cursor_position; }

void TerrainRendererItem::set_world_space_cursor_position(const glm::dvec3& new_pos)
{
    QVector3D new_vec3d = { static_cast<float>(new_pos.x), static_cast<float>(new_pos.y), static_cast<float>(new_pos.z) };
    if (m_world_space_cursor_position == new_vec3d)
        return;
    m_world_space_cursor_position = new_vec3d;
    emit world_space_cursor_position_changed(m_world_space_cursor_position);
}

const nucleus::picker::Feature& TerrainRendererItem::picked_feature() const { return m_picked_feature; }

void TerrainRendererItem::set_picked_feature(const nucleus::picker::Feature& new_picked_feature)
{
    if (m_picked_feature == new_picked_feature)
        return;
    m_picked_feature = new_picked_feature;
    emit picked_feature_changed(m_picked_feature);
}

bool TerrainRendererItem::continuous_update() const
{
    return m_continuous_update;
}

void TerrainRendererItem::set_continuous_update(bool new_continuous_update)
{
    if (m_continuous_update == new_continuous_update)
        return;
    m_continuous_update = new_continuous_update;
    m_update_timer->setSingleShot(!m_continuous_update);
    m_update_timer->start();
    emit continuous_update_changed(m_continuous_update);
}

void TerrainRendererItem::init_after_creation_slot() {
    // INITIALIZE shared config with URL parameter:
    auto urlmodifier = m_url_modifier.get();
    bool param_available = false;
    auto config_base64_string = urlmodifier->get_query_item(URL_PARAMETER_KEY_CONFIG, &param_available);
    if (param_available) set_gl_preset(config_base64_string);

    auto campos_string = urlmodifier->get_query_item(URL_PARAMETER_KEY_CAM_POS);
    auto camlookat_string = urlmodifier->get_query_item(URL_PARAMETER_KEY_CAM_LOOKAT);
    if (!campos_string.isEmpty() && !camlookat_string.isEmpty()) {
        // INITIALIZE camera definition from URL parameters
        qInfo() << "Initialize camera with pos :" << campos_string << "lookat :" << camlookat_string;
        auto campos_latlonalt = nucleus::utils::UrlModifier::urlsafe_string_to_dvec3(campos_string);
        auto lookat_latlonalt = nucleus::utils::UrlModifier::urlsafe_string_to_dvec3(camlookat_string);
        auto campos_ws = nucleus::srs::lat_long_alt_to_world(campos_latlonalt);
        auto lookat_ws = nucleus::srs::lat_long_alt_to_world(lookat_latlonalt);
        auto newDefinition = nucleus::camera::Definition(campos_ws, lookat_ws);
        emit camera_definition_set_by_user(newDefinition);
    }

    // Maybe shared config is already different (by loading from url)
    // so lets notify the Renderer here to replace the current configuration!
    emit shared_config_changed(m_shared_config);
}

void TerrainRendererItem::datetime_changed(const QDateTime&)
{
    recalculate_sun_angles();
}

void TerrainRendererItem::gl_sundir_date_link_changed(bool)
{
    recalculate_sun_angles();
}
