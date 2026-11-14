/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "AnimationStyle.h"
#include "Definition.h"
#include "InteractionStyle.h"
#include "RoutePreviewAnimation.h"
#include "recording.h"
#include <QObject>
#include <glm/glm.hpp>
#include <memory>
#include <nucleus/event_parameter.h>

namespace nucleus {
class DataQuerier;
}

namespace nucleus::camera {
class AbstractDepthTester;

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(const Definition& camera,
                        AbstractDepthTester* depth_tester,
                        DataQuerier* data_querier);

    /// warning: not thread safe
    [[nodiscard]] const Definition& definition() const;
    std::optional<glm::vec2> operation_centre();
    std::optional<float> operation_centre_distance();

	void report_global_cursor_position(const QPointF& screen_pos);

public slots:
    void set_pixel_error_threshold(float threshold);
    void set_model_matrix(const Definition& new_definition);
    void set_near_plane(float distance);
    void set_viewport(const glm::uvec2& new_viewport);
    void fly_to_latitude_longitude(double latitude, double longitude);
    // translates the camera so the point it currently looks at lands on the
    // requested position; the orientation (rotation, pitch, distance, height
    // above ground) stays as it is
    void move_to_latitude_longitude(double latitude, double longitude);
    void rotate_north();
    void set_field_of_view(float fov_degrees);
    void move(const glm::dvec3& v);
    void orbit(const glm::dvec3& centre, const glm::dvec2& degrees);
    void update() const;

    void mouse_press(const event_parameter::Mouse&);
    void mouse_move(const event_parameter::Mouse&);
    void mouse_release(const event_parameter::Mouse&);
    void wheel_turn(const event_parameter::Wheel&);
    void key_press(const QKeyCombination&);
    void key_release(const QKeyCombination&);
    void touch(const event_parameter::Touch&);
    void advance_camera();

    // does a synchronous depth readback at the given screen position and
    // reports the terrain position in geo coordinates (lat/long/altitude)
    void query_world_position(const QPointF& screen_pos);

    // drop the tracked modifier/keyboard state (see InteractionStyle::clear_keyboard_state)
    void clear_keyboard_state();

    // route fly-through preview: the camera descends along the given polyline,
    // slightly above the terrain. while active, drags rotate around the
    // current position and the wheel changes the height. remains until
    // explicitly stopped
    void start_route_preview(const std::vector<glm::dvec3>& world_points);
    void stop_route_preview();
    void set_route_preview_paused(bool paused);
    void seek_route_preview(double fraction);
    void set_route_preview_speed(double metres_per_second);

signals:
    void definition_changed(const Definition& new_definition) const;
    void global_cursor_position_changed(glm::dvec3 pos) const;
    void world_position_queried(glm::dvec3 lat_long_alt) const;
    // progress fraction (0..1), paused flag, active flag, speed (m/s)
    void route_preview_changed(double progress_fraction, bool paused, bool active, double speed) const;

private:
    void set_interaction_style(std::unique_ptr<InteractionStyle> new_style);
    void set_animation_style(std::unique_ptr<InteractionStyle> new_style);

    recording::Device m_recorder;
    Definition m_definition;
    AbstractDepthTester* m_depth_tester;
    DataQuerier* m_data_querier;
    std::unique_ptr<InteractionStyle> m_interaction_style;
    std::unique_ptr<AnimationStyle> m_animation_style;
    std::unique_ptr<RoutePreviewAnimation> m_route_preview;
    Definition m_definition_before_preview;
    std::chrono::steady_clock::time_point m_last_frame_time;
    double m_last_emitted_preview_fraction = -1.0;
    double m_last_emitted_preview_speed = -1.0;
    bool m_last_emitted_preview_paused = false;
    bool m_last_emitted_preview_active = false;
    void emit_preview_state_if_changed();
};

}
