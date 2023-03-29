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

#include "FirstPersonInteraction.h"
#include "AbstractDepthTester.h"

#include <QDebug>

namespace nucleus::camera {

std::optional<Definition> FirstPersonInteraction::mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester)
{

    if (e.buttons == Qt::LeftButton || e.buttons == Qt::MiddleButton) {
        const auto delta = e.point.position() - e.point.lastPosition();
        camera.orbit_clamped(camera.position(), glm::vec2(delta.x(), delta.y()) * -0.1f);
    }

    if (e.buttons == Qt::NoButton)
        return {};
    else
        return camera;
}

std::optional<Definition> FirstPersonInteraction::touch_event(const event_parameter::Touch& e, Definition camera, AbstractDepthTester* depth_tester)
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

std::optional<Definition> FirstPersonInteraction::wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester)
{
    if (e.angle_delta.y() > 0) {
        m_speed_modifyer = std::min(m_speed_modifyer * 1.3f, 4000.0f);
    } else {
        m_speed_modifyer = std::max(m_speed_modifyer / 1.3f, 1.0f);
    }
    return camera;
}

std::optional<Definition> FirstPersonInteraction::key_press_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* ray_caster)
{
    auto direction = glm::dvec3();
    if (e.key() == Qt::Key_W || m_key_w) {
        m_key_w = true;
        direction -= camera.z_axis();
    }
    if (e.key() == Qt::Key_S || m_key_s) {
        m_key_s = true;
        direction += camera.z_axis();
    }
    if (e.key() == Qt::Key_A || m_key_a) {
        m_key_a = true;
        direction -= camera.x_axis();
    }
    if (e.key() == Qt::Key_D || m_key_d) {
        m_key_d = true;
        direction += camera.x_axis();
    }
    if (e.key() == Qt::Key_E || m_key_e) {
        m_key_e = true;
        direction += glm::dvec3(0, 0, 1);
    }
    if (e.key() == Qt::Key_Q || m_key_q) {
        m_key_q = true;
        direction -= glm::dvec3(0, 0, 1);
    }
    glm::normalize(direction);
    camera.move(direction * (double)m_speed_modifyer);
    return camera;
}

std::optional<Definition> FirstPersonInteraction::key_release_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* ray_caster)
{
    if (e.key() == Qt::Key_W) {
        m_key_w = false;
    }
    if (e.key() == Qt::Key_S) {
        m_key_s = false;
    }
    if (e.key() == Qt::Key_A) {
        m_key_a = false;
    }
    if (e.key() == Qt::Key_D) {
        m_key_d = false;
    }
    if (e.key() == Qt::Key_E) {
        m_key_e = false;
    }
    if (e.key() == Qt::Key_Q) {
        m_key_q = false;
    }
    return camera;
}

}
