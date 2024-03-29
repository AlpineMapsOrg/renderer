/*****************************************************************************
 * Alpine Terrain Renderer
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

#pragma once

#include <QQuickFramebufferObject>
#include <QTimer>
#include <QList>
#include <QString>
#include <QVector3D>
#include <QVector2D>
#include <QDateTime>
#include <map>

#include "nucleus/camera/Definition.h"
#include "nucleus/event_parameter.h"
#include "nucleus/utils/GPX.h"
#include "gl_engine/UniformBufferObjects.h"
#include "timing/TimerFrontendManager.h"
#include "AppSettings.h"

class TerrainRendererItem : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(int frame_limit READ frame_limit WRITE set_frame_limit NOTIFY frame_limit_changed)
    Q_PROPERTY(nucleus::camera::Definition camera READ camera NOTIFY camera_changed)
    Q_PROPERTY(int camera_width READ camera_width NOTIFY camera_width_changed)
    Q_PROPERTY(int camera_height READ camera_height NOTIFY camera_height_changed)
    Q_PROPERTY(float field_of_view READ field_of_view WRITE set_field_of_view NOTIFY field_of_view_changed)
    Q_PROPERTY(float camera_rotation_from_north READ camera_rotation_from_north NOTIFY camera_rotation_from_north_changed)
    Q_PROPERTY(QPointF camera_operation_centre READ camera_operation_centre NOTIFY camera_operation_centre_changed)
    Q_PROPERTY(bool camera_operation_centre_visibility READ camera_operation_centre_visibility NOTIFY camera_operation_centre_visibility_changed)
    Q_PROPERTY(float camera_operation_centre_distance READ camera_operation_centre_distance NOTIFY camera_operation_centre_distance_changed)
    Q_PROPERTY(gl_engine::uboSharedConfig shared_config READ shared_config WRITE set_shared_config NOTIFY shared_config_changed)
    Q_PROPERTY(TimerFrontendManager* timer_manager MEMBER m_timer_manager CONSTANT)
    Q_PROPERTY(AppSettings* settings MEMBER m_settings CONSTANT)
    Q_PROPERTY(unsigned int in_flight_tiles READ in_flight_tiles NOTIFY in_flight_tiles_changed)
    Q_PROPERTY(unsigned int queued_tiles READ queued_tiles NOTIFY queued_tiles_changed)
    Q_PROPERTY(unsigned int cached_tiles READ cached_tiles NOTIFY cached_tiles_changed)
    Q_PROPERTY(unsigned int tile_cache_size READ tile_cache_size WRITE set_tile_cache_size NOTIFY tile_cache_size_changed)
    Q_PROPERTY(bool render_looped READ render_looped WRITE set_render_looped NOTIFY render_looped_changed)
    Q_PROPERTY(unsigned int selected_camera_position_index MEMBER m_selected_camera_position_index WRITE set_selected_camera_position_index)
    Q_PROPERTY(bool hud_visible READ hud_visible WRITE set_hud_visible NOTIFY hud_visible_changed)
    Q_PROPERTY(QVector2D sun_angles READ sun_angles WRITE set_sun_angles NOTIFY sun_angles_changed)
    Q_PROPERTY(float track_width READ track_width WRITE set_track_width NOTIFY track_width_changed)
    Q_PROPERTY(unsigned int track_shading READ track_shading WRITE set_track_shading NOTIFY track_shading_changed)

public:
    explicit TerrainRendererItem(QQuickItem* parent = 0);
    ~TerrainRendererItem() override;
    Renderer *createRenderer() const Q_DECL_OVERRIDE;

signals:

    void frame_limit_changed();

    void mouse_pressed(const nucleus::event_parameter::Mouse&) const;
    void mouse_moved(const nucleus::event_parameter::Mouse&) const;
    void wheel_turned(const nucleus::event_parameter::Wheel&) const;
    void touch_made(const nucleus::event_parameter::Touch&) const;
    void key_pressed(const QKeyCombination&) const;
    void key_released(const QKeyCombination&) const;
    void update_camera_requested() const;
    //    void viewport_changed(const glm::uvec2& new_viewport) const;
    void position_set_by_user(double new_latitude, double new_longitude);
    void camera_definition_set_by_user(const nucleus::camera::Definition&) const;
    void track_width_changed(float width);
    void track_shading_changed(unsigned int shading);

    void shared_config_changed(gl_engine::uboSharedConfig new_shared_config) const;
    void render_looped_changed(bool new_render_looped);
    void hud_visible_changed(bool new_hud_visible);

    void gpx_track_added_by_user(const nucleus::gpx::Gpx& track);

    void rotation_north_requested();
    void camera_changed();
    void camera_width_changed();
    void camera_height_changed();
    void field_of_view_changed();
    void camera_rotation_from_north_changed();
    void camera_operation_centre_changed();
    void camera_operation_centre_visibility_changed();
    void camera_operation_centre_distance_changed();
    void render_quality_changed(float new_render_quality);

    void in_flight_tiles_changed(unsigned new_n);

    void queued_tiles_changed(unsigned new_n);

    void cached_tiles_changed(unsigned new_n);

    void tile_cache_size_changed(unsigned new_cache_size);

    void gui_update_global_cursor_pos(double latitude, double longitude, double altitude);

    void sun_angles_changed(QVector2D newSunAngles);

    void reload_shader();

    void init_after_creation() const;

protected:
    void touchEvent(QTouchEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;

public slots:
    void set_position(double latitude, double longitude);
    void rotate_north();
    void set_gl_preset(const QString& preset_b64_string);
    void read_global_position(glm::dvec3 latlonalt);
    void camera_definition_changed(const nucleus::camera::Definition& new_definition); // gets called whenever camera changes
    void add_track(const QString& track);
    void get_upload_file();

private slots:
    void schedule_update();
    void init_after_creation_slot();
    void datetime_changed(const QDateTime& new_datetime);
    void gl_sundir_date_link_changed(bool new_value);


public:
    [[nodiscard]] int frame_limit() const;
    void set_frame_limit(int new_frame_limit);

    [[nodiscard]] nucleus::camera::Definition camera() const;
    void set_read_only_camera(const nucleus::camera::Definition& new_camera); // implementation detail

    int camera_width() const;
    void set_read_only_camera_width(int new_camera_width);

    int camera_height() const;
    void set_read_only_camera_height(int new_camera_height);

    float field_of_view() const;
    void set_field_of_view(float new_field_of_view);

    float track_width() const;
    void set_track_width(float width);

    unsigned int track_shading() const;
    void set_track_shading(unsigned int shading);

    float camera_rotation_from_north() const;
    void set_camera_rotation_from_north(float new_camera_rotation_from_north);

    QPointF camera_operation_centre() const;
    void set_camera_operation_centre(QPointF new_camera_operation_centre);

    bool camera_operation_centre_visibility() const;
    void set_camera_operation_centre_visibility(bool new_camera_operation_centre_visibility);

    float camera_operation_centre_distance() const;
    void set_camera_operation_centre_distance(float new_camera_operation_centre_distance);

    bool render_looped() const;
    void set_render_looped(bool new_render_looped);

    gl_engine::uboSharedConfig shared_config() const;
    void set_shared_config(gl_engine::uboSharedConfig new_shared_config);

    bool hud_visible() const { return m_hud_visible; }
    void set_hud_visible(bool new_hud_visible);

    void set_selected_camera_position_index(unsigned value);

    [[nodiscard]] unsigned int in_flight_tiles() const;
    void set_in_flight_tiles(unsigned int new_in_flight_tiles);

    [[nodiscard]] unsigned int queued_tiles() const;
    void set_queued_tiles(unsigned int new_queued_tiles);

    [[nodiscard]] unsigned int cached_tiles() const;
    void set_cached_tiles(unsigned int new_cached_tiles);

    [[nodiscard]] unsigned int tile_cache_size() const;
    void set_tile_cache_size(unsigned int new_tile_cache_size);

    QVector2D sun_angles() const;
    void set_sun_angles(QVector2D new_sunAngles);

    const AppSettings* settings() { return m_settings; }

private:
    void recalculate_sun_angles();
    void update_gl_sun_dir_from_sun_angles(gl_engine::uboSharedConfig& ubo);

    float m_camera_rotation_from_north = 0;
    QPointF m_camera_operation_centre;
    bool m_camera_operation_centre_visibility = false;
    float m_camera_operation_centre_distance = 1;
    float m_field_of_view = 60;
    float m_track_width = 7;
    unsigned int m_track_shading = 0;
    int m_frame_limit = 60;
    unsigned m_tile_cache_size = 12000;
    unsigned m_cached_tiles = 0;
    unsigned m_queued_tiles = 0;
    unsigned m_in_flight_tiles = 0;
    unsigned int m_selected_camera_position_index = 0;
    bool m_render_looped = false;
    bool m_hud_visible = true;
    QDateTime m_selected_datetime = QDateTime::currentDateTime();

    gl_engine::uboSharedConfig m_shared_config;

    QTimer* m_update_timer = nullptr;
    nucleus::camera::Definition m_camera;
    int m_camera_width = 0;
    int m_camera_height = 0;

    TimerFrontendManager* m_timer_manager;
    AppSettings* m_settings;
    QVector2D m_sun_angles; // azimuth and zenith
    glm::dvec3 m_last_camera_latlonalt;
    glm::dvec3 m_last_camera_lookat_latlonalt;

    // Note: This was originaly a singleton. But that introduces some issues
    // with the multi-thread nature of this app. So far the url modifier is
    // only necessary in this class and on this thread, so we'll use it here.
    std::shared_ptr<nucleus::utils::UrlModifier> m_url_modifier;
};
