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

#include "CadInteraction.h"
#include "AbstractDepthTester.h"

#include <QDebug>

namespace nucleus::camera {

std::optional<Definition> CadInteraction::mouse_press_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester)
{
    if (m_operation_centre.x == 0 && m_operation_centre.y == 0 && m_operation_centre.z == 0) {
        m_operation_centre = depth_tester->position(glm::dvec2(0.0, 0.0));
    }
    return {};
}

std::optional<Definition> CadInteraction::mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester)
{
    m_operation_centre_screen = glm::vec2(camera.viewport_size().x / 2.0f, camera.viewport_size().y / 2.0f);
    if (e.buttons == Qt::LeftButton) {
        const auto delta = e.point.position() - e.point.lastPosition();
        float dist = glm::distance(camera.position(), m_operation_centre);
        double moveSpeedModifier = 750.0;

        m_operation_centre = m_operation_centre - camera.x_axis() * delta.x() * (dist / moveSpeedModifier);
        m_operation_centre = m_operation_centre + camera.y_axis() * delta.y() * (dist / moveSpeedModifier);
        camera.move(-camera.x_axis() * delta.x() * (dist / moveSpeedModifier));
        camera.move(camera.y_axis() * delta.y() * (dist / moveSpeedModifier));
    }
    if (e.buttons == Qt::MiddleButton) {
        const auto delta = e.point.position() - e.point.lastPosition();
        camera.orbit_clamped(m_operation_centre, glm::vec2(delta.x(), delta.y()) * -0.1f);
    }
    if (e.buttons == Qt::RightButton) {
        const auto delta = e.point.position() - e.point.lastPosition();
        float dist = glm::distance(camera.position(), m_operation_centre);
        float zoomDist = (delta.y() - delta.x()) * dist / 400.0;
        if (zoomDist < dist) {
            if (dist > 5.0f || zoomDist > 0.0f) { // always allow zoom out
                camera.zoom(zoomDist);
            }
        }
    }

    if (e.buttons == Qt::NoButton)
        return {};
    else
        return camera;
}

std::optional<Definition> CadInteraction::touch_event(const event_parameter::Touch& e, Definition camera, AbstractDepthTester* depth_tester)
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

std::optional<Definition> CadInteraction::wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester)
{
    m_operation_centre_screen = glm::vec2(camera.viewport_size().x / 2.0f, camera.viewport_size().y / 2.0f);
    if (m_operation_centre.x == 0 && m_operation_centre.y == 0 && m_operation_centre.z == 0) {
        m_operation_centre = depth_tester->position(glm::dvec2(0.0, 0.0));
    }

    float dist = glm::distance(camera.position(), m_operation_centre);
    if (e.angle_delta.y() > 0) {
        if (dist > 5.0f) {
            camera.zoom(-dist / 10.0);
        }
    } else {
        camera.zoom(dist / 10.0);
    }
    return camera;
}

std::optional<glm::vec2> CadInteraction::get_operation_centre(){
    return m_operation_centre_screen;
}
}
