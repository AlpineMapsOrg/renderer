/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#include <memory>

#include <QObject>

#include <glm/glm.hpp>

#include "../event_parameter.h"
#include "Definition.h"
#include "InteractionStyle.h"

namespace nucleus::camera {
class AbstractDepthTester;

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(const Definition& camera, AbstractDepthTester* depth_tester);

    [[nodiscard]] const Definition& definition() const;
    std::optional<glm::vec2> get_operation_centre();
    std::optional<float> get_operation_centre_distance();
    std::optional<float> get_speed();

public slots:
    void set_definition(const Definition& new_definition);
    void set_near_plane(float distance);
    void set_viewport(const glm::uvec2& new_viewport);
    void set_latitude_longitude(double latitude, double longitude);
    void set_field_of_view(float fov_degrees);
    void move(const glm::dvec3& v);
    void orbit(const glm::dvec3& centre, const glm::dvec2& degrees);
    void update() const;

    void mouse_press(const event_parameter::Mouse&);
    void mouse_move(const event_parameter::Mouse&);
    void wheel_turn(const event_parameter::Wheel&);
    void key_press(const QKeyCombination&);
    void key_release(const QKeyCombination&);
    void touch(const event_parameter::Touch&);
    void update_camera_request();

signals:
    void definition_changed(const Definition& new_definition) const;

private:
    void set_interaction_style(std::unique_ptr<InteractionStyle> new_style);
    void set_animation_style(std::unique_ptr<InteractionStyle> new_style);

    Definition m_definition;
    AbstractDepthTester* m_depth_tester;
    std::unique_ptr<InteractionStyle> m_interaction_style;
    std::unique_ptr<InteractionStyle> m_animation_style;
    std::chrono::steady_clock::time_point m_last_frame_time;
};

}
