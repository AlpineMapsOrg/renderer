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

#pragma once

#include "AppSettings.h"
#include "TileStatistics.h"
#include <QDateTime>
#include <QList>
#include <QQmlEngine>
#include <QQuickFramebufferObject>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <gl_engine/UniformBufferObjects.h>
#include <nucleus/camera/Definition.h>
#include <nucleus/event_parameter.h>
#include <nucleus/map_label/FilterDefinitions.h>
#include <nucleus/picker/types.h>

class QTimer;

class TerrainRendererItem : public QQuickFramebufferObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(TerrainRenderer)
    Q_PROPERTY(int redraw_delay READ redraw_delay WRITE set_redraw_delay NOTIFY redraw_delay_changed)
    Q_PROPERTY(nucleus::camera::Definition camera READ camera NOTIFY camera_changed)
    Q_PROPERTY(int camera_width READ camera_width NOTIFY camera_width_changed)
    Q_PROPERTY(int camera_height READ camera_height NOTIFY camera_height_changed)
    Q_PROPERTY(float field_of_view READ field_of_view WRITE set_field_of_view NOTIFY field_of_view_changed)
    Q_PROPERTY(float camera_rotation_from_north READ camera_rotation_from_north NOTIFY camera_rotation_from_north_changed)
    Q_PROPERTY(QPointF camera_operation_centre READ camera_operation_centre NOTIFY camera_operation_centre_changed)
    Q_PROPERTY(bool camera_operation_centre_visibility READ camera_operation_centre_visibility NOTIFY camera_operation_centre_visibility_changed)
    Q_PROPERTY(float camera_operation_centre_distance READ camera_operation_centre_distance NOTIFY camera_operation_centre_distance_changed)
    Q_PROPERTY(gl_engine::uboSharedConfig shared_config READ shared_config WRITE set_shared_config NOTIFY shared_config_changed)
    Q_PROPERTY(nucleus::map_label::FilterDefinitions label_filter READ label_filter WRITE set_label_filter NOTIFY label_filter_changed)
    Q_PROPERTY(AppSettings* settings MEMBER m_settings CONSTANT)
    Q_PROPERTY(TileStatistics* tile_statistics MEMBER m_tile_statistics CONSTANT)
    Q_PROPERTY(unsigned int tile_cache_size READ tile_cache_size WRITE set_tile_cache_size NOTIFY tile_cache_size_changed)
    Q_PROPERTY(unsigned int selected_camera_position_index MEMBER m_selected_camera_position_index WRITE set_selected_camera_position_index)
    Q_PROPERTY(QVector2D sun_angles READ sun_angles WRITE set_sun_angles NOTIFY sun_angles_changed)
    Q_PROPERTY(bool continuous_update READ continuous_update WRITE set_continuous_update NOTIFY continuous_update_changed)
    Q_PROPERTY(nucleus::picker::Feature picked_feature READ picked_feature NOTIFY picked_feature_changed FINAL)
    Q_PROPERTY(QVector3D world_space_cursor_position READ world_space_cursor_position NOTIFY world_space_cursor_position_changed FINAL)

public:
    explicit TerrainRendererItem(QQuickItem* parent = 0);
    ~TerrainRendererItem() override;
    Renderer *createRenderer() const Q_DECL_OVERRIDE;

signals:

    void redraw_delay_changed();

    void mouse_pressed(const nucleus::event_parameter::Mouse&) const;
    void mouse_released(const nucleus::event_parameter::Mouse&) const;
    void mouse_moved(const nucleus::event_parameter::Mouse&) const;
    void wheel_turned(const nucleus::event_parameter::Wheel&) const;
    void touch_made(const nucleus::event_parameter::Touch&) const;
    void key_pressed(const QKeyCombination&) const;
    void key_released(const QKeyCombination&) const;
    void update_camera_requested() const;
    //    void viewport_changed(const glm::uvec2& new_viewport) const;
    void position_set_by_user(double new_latitude, double new_longitude);
    void camera_definition_set_by_user(const nucleus::camera::Definition&) const;

    void shared_config_changed(gl_engine::uboSharedConfig new_shared_config) const;
    void label_filter_changed(const nucleus::map_label::FilterDefinitions label_filter) const;
    void hud_visible_changed(bool new_hud_visible);

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

    void tile_cache_size_changed(unsigned new_cache_size);

    void sun_angles_changed(QVector2D newSunAngles);

    void reload_shader();

    void init_after_creation() const;

    void continuous_update_changed(bool continuous_update);

    void feature_changed();

    void picked_feature_changed(const nucleus::picker::Feature& picked_feature);

    void world_space_cursor_position_changed(const QVector3D& world_space_cursor_position);

protected:
    void touchEvent(QTouchEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;

public slots:
    void set_position(double latitude, double longitude);
    void rotate_north();
    void set_gl_preset(const QString& preset_b64_string);
    void camera_definition_changed(const nucleus::camera::Definition& new_definition); // gets called whenever camera changes

private slots:
    void schedule_update();
    void init_after_creation_slot();
    void datetime_changed(const QDateTime& new_datetime);
    void gl_sundir_date_link_changed(bool new_value);

public:
    [[nodiscard]] int redraw_delay() const;
    void set_redraw_delay(int new_frame_limit);

    [[nodiscard]] nucleus::camera::Definition camera() const;
    void set_read_only_camera(const nucleus::camera::Definition& new_camera); // implementation detail

    int camera_width() const;
    void set_read_only_camera_width(int new_camera_width);

    int camera_height() const;
    void set_read_only_camera_height(int new_camera_height);

    float field_of_view() const;
    void set_field_of_view(float new_field_of_view);

    float camera_rotation_from_north() const;
    void set_camera_rotation_from_north(float new_camera_rotation_from_north);

    QPointF camera_operation_centre() const;
    void set_camera_operation_centre(QPointF new_camera_operation_centre);

    bool camera_operation_centre_visibility() const;
    void set_camera_operation_centre_visibility(bool new_camera_operation_centre_visibility);

    float camera_operation_centre_distance() const;
    void set_camera_operation_centre_distance(float new_camera_operation_centre_distance);

    gl_engine::uboSharedConfig shared_config() const;
    void set_shared_config(gl_engine::uboSharedConfig new_shared_config);

    nucleus::map_label::FilterDefinitions label_filter() const;
    void set_label_filter(nucleus::map_label::FilterDefinitions new_label_filter);

    void set_selected_camera_position_index(unsigned value);

    [[nodiscard]] unsigned int tile_cache_size() const;
    void set_tile_cache_size(unsigned int new_tile_cache_size);

    QVector2D sun_angles() const;
    void set_sun_angles(QVector2D new_sunAngles);

    const AppSettings* settings() { return m_settings; }

    bool continuous_update() const;
    void set_continuous_update(bool new_continuous_update);

    const nucleus::picker::Feature& picked_feature() const;

    const QVector3D& world_space_cursor_position() const;

private:
    void set_world_space_cursor_position(const glm::dvec3& new_world_space_cursor_position);
    void set_picked_feature(const nucleus::picker::Feature& new_picked_feature);
    void recalculate_sun_angles();
    void update_gl_sun_dir_from_sun_angles(gl_engine::uboSharedConfig& ubo);

    bool m_continuous_update = false;
    float m_camera_rotation_from_north = 0;
    QPointF m_camera_operation_centre;
    bool m_camera_operation_centre_visibility = false;
    float m_camera_operation_centre_distance = 1;
    float m_field_of_view = 60;
    int m_redraw_delay = 1;
    unsigned m_tile_cache_size = 12000;
    unsigned int m_selected_camera_position_index = 0;
    QDateTime m_selected_datetime = QDateTime::currentDateTime();

    nucleus::picker::Feature m_picked_feature;
    QVector3D m_world_space_cursor_position;

    gl_engine::uboSharedConfig m_shared_config;
    nucleus::map_label::FilterDefinitions m_label_filter;

    QTimer* m_update_timer = nullptr;
    nucleus::camera::Definition m_camera;
    int m_camera_width = 0;
    int m_camera_height = 0;

    AppSettings* m_settings;
    TileStatistics* m_tile_statistics;
    QVector2D m_sun_angles; // azimuth and zenith
    glm::dvec3 m_last_camera_latlonalt;
    glm::dvec3 m_last_camera_lookat_latlonalt;

    // Note: This was originaly a singleton. But that introduces some issues
    // with the multi-thread nature of this app. So far the url modifier is
    // only necessary in this class and on this thread, so we'll use it here.
    std::shared_ptr<nucleus::utils::UrlModifier> m_url_modifier;
};
