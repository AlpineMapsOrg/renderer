/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include "OrbitInteraction.h"
#include "AbstractDepthTester.h"

#include <QDebug>

namespace nucleus::camera {

std::optional<Definition> OrbitInteraction::mouse_press_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester)
{
    m_operation_centre = depth_tester->ray_cast(camera, camera.to_ndc({ e.point.pressPosition().x(), e.point.pressPosition().y() }));
    return {};
}

std::optional<Definition> OrbitInteraction::mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester)
{

    if (e.buttons == Qt::LeftButton) {
        const auto delta = e.point.position() - e.point.lastPosition();

        glm::dvec3 hit = m_operation_centre - camera.position();
        float distance = std::sqrt(std::pow(hit.x, 2) + std::pow(hit.y, 2) + std::pow(hit.z, 2));
        float dist = 1.0 * std::max((distance / 1200), 0.07f);

        camera.pan(glm::vec2(delta.x(), delta.y()) * dist);
    }
    if (e.buttons == Qt::MiddleButton) {
        const auto delta = e.point.position() - e.point.lastPosition();
        camera.orbit(m_operation_centre, glm::vec2(delta.x(), delta.y()) * -0.1f);
    }
    if (e.buttons == Qt::RightButton) {
        const auto delta = e.point.position() - e.point.lastPosition();

        glm::dvec3 hit = m_operation_centre - camera.position();
        float distance = std::sqrt(std::pow(hit.x, 2) + std::pow(hit.y, 2) + std::pow(hit.z, 2));
        float dist = -1.0 * std::max((distance / 1200), 0.07f);

        camera.move(glm::normalize(hit) * ((delta.y() - delta.x()) * (double)dist));
    }

    if (e.buttons == Qt::NoButton)
        return {};
    else
        return camera;
}

std::optional<Definition> OrbitInteraction::touch_event(const event_parameter::Touch& e, Definition camera, AbstractDepthTester* depth_tester)
{
    glm::ivec2 first_touch = { e.points[0].position().x(), e.points[0].position().y() };
    glm::ivec2 second_touch;
    if (e.points.size() >= 2)
        second_touch = { e.points[1].position().x(), e.points[1].position().y() };

    // ugly code, but it prevents the following:
    // touch 1 at pos a, touch 2 at pos b
    // release touch 1, touch 2 becomes new touch 1. movement from b to a is initiated.
    if (m_was_double_touch) {
        m_previous_first_touch = first_touch;
    }
    m_was_double_touch = false;

    if (e.is_end_event) {
        m_was_double_touch = e.points.size() >= 2;
        return {};
    }
    if (e.is_begin_event) {
        m_previous_first_touch = first_touch;
        m_previous_second_touch = second_touch;
        return {};
    }
    // touch move
    if (e.points.size() == 1) {
        const auto delta = first_touch - m_previous_first_touch;
        camera.pan(glm::vec2(delta) * 10.0f);
    }
    if (e.points.size() == 2) {
        const auto previous_centre = (m_previous_first_touch + m_previous_second_touch) / 2;
        const auto current_centre = (first_touch + second_touch) / 2;
        const auto pitch = -(current_centre - previous_centre).y;

        const auto previous_yaw_dir = glm::normalize(glm::vec2(m_previous_first_touch - m_previous_second_touch));
        const auto previous_yaw_angle = std::atan2(previous_yaw_dir.y, previous_yaw_dir.x);
        const auto current_yaw_dir = glm::normalize(glm::vec2(first_touch - second_touch));
        const auto current_yaw_angle = std::atan2(current_yaw_dir.y, current_yaw_dir.x);
//        qDebug() << "prev: " << previous_yaw_angle << ", curr: " << current_yaw_angle;

        const auto yaw = (current_yaw_angle - previous_yaw_angle) * 600;
        camera.orbit(glm::vec2(yaw, pitch) * 0.1f);

        const auto previous_dist = glm::length(glm::vec2(m_previous_first_touch - m_previous_second_touch));
        const auto current_dist = glm::length(glm::vec2(first_touch - second_touch));
        camera.zoom((previous_dist - current_dist) * 10);
    }
    m_previous_first_touch = first_touch;
    m_previous_second_touch = second_touch;
    return camera;
}

std::optional<Definition> OrbitInteraction::wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester)
{
    glm::dvec3 hit = depth_tester->ray_cast(camera, camera.to_ndc({ e.point.position().x(), e.point.position().y() }));
    hit = hit - camera.position();
    float distance = std::sqrt(std::pow(hit.x, 2) + std::pow(hit.y, 2) + std::pow(hit.z, 2));

    float dist = std::max((distance / 1500), 0.07f);
    camera.move(glm::normalize(hit) * (e.angle_delta.y() * (double)dist));
    return camera;
}
}
