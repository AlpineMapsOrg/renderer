/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
 * Copyright (C) 2024 Gerald Kimmersdorfer
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
#include "glm/ext/scalar_constants.hpp"

#include <QDebug>

namespace nucleus::camera {

std::optional<Definition> OrbitInteraction::mouse_press_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester)
{
    start({e.point.position.x, e.point.position.y}, camera, depth_tester);
    return {};
}

std::optional<Definition> OrbitInteraction::mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester)
{
    if (e.buttons == Qt::LeftButton && !m_key_ctrl && !m_key_alt) {
        pan(e.point.position, e.point.last_position, &camera, depth_tester);
    }
    if (e.buttons == Qt::MiddleButton || (e.buttons == Qt::LeftButton && m_key_ctrl && !m_key_alt)) {
        m_operation_centre_screen = e.point.press_position;
        const auto delta = e.point.position - e.point.last_position;
        camera.orbit_clamped(m_operation_centre, delta * -0.1f);
    }
    if (e.buttons == Qt::RightButton || (e.buttons == Qt::LeftButton && !m_key_ctrl && m_key_alt)) {
        m_operation_centre_screen = e.point.press_position;
        const auto delta = e.point.position - e.point.last_position;

        zoom((delta.y + delta.x) / 4.f, &camera, depth_tester);
    }

    if (e.buttons == Qt::NoButton)
        return {};
    else
        return camera;
}

std::optional<Definition> OrbitInteraction::touch_event(const event_parameter::Touch& e, Definition camera, AbstractDepthTester* depth_tester)
{
    // touch move
    if (e.points.size() == 1 && e.points[0].state == event_parameter::TouchPointPressed) {
        start(e.points[0].position, camera, depth_tester);
        return {};
    }
    if (e.points.size() == 2 && e.points[1].state == event_parameter::TouchPointReleased) {
        start(e.points[0].position, camera, depth_tester);
        return {};
    }
    if (e.points.size() == 2 && e.points[0].state == event_parameter::TouchPointReleased) {
        start(e.points[1].position, camera, depth_tester);
        return {};
    }
    if (e.points.size() == 1) {
        pan(e.points[0].position, e.points[0].last_position, &camera, depth_tester);
        return camera;
    }
    if (e.points.size() == 2) {
        const auto current_centre = (e.points[0].position + e.points[1].position) * 0.5f;
        if (e.points[0].state == event_parameter::TouchPointPressed || e.points[1].state == event_parameter::TouchPointPressed) {
            m_operation_centre_screen = current_centre;
            start(current_centre, camera, depth_tester);
            return {};
        }
        const auto previous_centre = (e.points[0].last_position + e.points[1].last_position) * 0.5f;
        const auto first_touch = e.points[0].last_position;
        const auto second_touch = e.points[1].position;
        const auto previous_first_touch = e.points[0].last_position;
        const auto previous_second_touch = e.points[1].last_position;
        const auto pitch = -(current_centre.y - previous_centre.y) * 0.5f;

        const auto previous_yaw_dir = glm::normalize(glm::vec2(previous_first_touch - previous_second_touch));
        const auto previous_yaw_angle = std::atan2(previous_yaw_dir.y, previous_yaw_dir.x);
        const auto current_yaw_dir = glm::normalize(glm::vec2(first_touch - second_touch));
        const auto current_yaw_angle = std::atan2(current_yaw_dir.y, current_yaw_dir.x);

        const auto yaw = (current_yaw_angle - previous_yaw_angle) * 360 / glm::pi<float>();
        camera.orbit_clamped(m_operation_centre, glm::vec2(yaw, pitch));

        const auto previous_dist = glm::length(glm::vec2(previous_first_touch - previous_second_touch));
        const auto current_dist = glm::length(glm::vec2(first_touch - second_touch));
        zoom(-(previous_dist - current_dist), &camera, depth_tester);
        return camera;
    }
    return {};
}

std::optional<Definition> OrbitInteraction::wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester)
{
    m_operation_centre_screen = e.point.position;
    start(e.point.position, camera, depth_tester);
    zoom(float(e.angle_delta.y()) / 15.f, &camera, depth_tester);
    return camera;
}

std::optional<Definition> OrbitInteraction::key_press_event(const QKeyCombination& e, Definition camera, AbstractDepthTester*)
{
    if (e.key() == Qt::Key_Control) {
        m_key_ctrl = true;
    }
    if (e.key() == Qt::Key_Alt) {
        m_key_alt = true;
    }
    if (e.key() == Qt::Key_Shift) {
        m_key_shift = true;
    }
    return camera;
}

std::optional<Definition> OrbitInteraction::key_release_event(const QKeyCombination& e, Definition camera, AbstractDepthTester*)
{
    if (e.key() == Qt::Key_Control) {
        m_key_ctrl = false;
    }
    if (e.key() == Qt::Key_Alt) {
        m_key_alt = false;
    }
    if (e.key() == Qt::Key_Shift) {
        m_key_shift = false;
    }
    return camera;
}

std::optional<glm::vec2> OrbitInteraction::operation_centre(){
    return m_operation_centre_screen;
}

void OrbitInteraction::start(const glm::vec2& position, const Definition& camera, AbstractDepthTester* depth_tester)
{
    m_operation_centre = depth_tester->position(camera.to_ndc(position));

    auto degFromUp = glm::degrees(glm::acos(glm::dot(camera.z_axis(), glm::dvec3(0, 0, 1))));
    if (m_operation_centre.z > camera.position().z || degFromUp > 80.0) {
        m_move_vertical = true;
    } else {
        m_move_vertical = false;
    }
}

void OrbitInteraction::pan(const glm::vec2& position, const glm::vec2&, Definition* camera, AbstractDepthTester*)
{
    m_operation_centre_screen = position;

    glm::dvec3 camRay = camera->ray_direction(camera->to_ndc(position));
    if (m_move_vertical || m_key_shift) {
        double denom = glm::dot(camera->z_axis(), camRay);
        double distance = 0;
        if (denom != 0) {
            distance = glm::dot((m_operation_centre - camera->position()), camera->z_axis()) / denom;
        }
        auto mouseInWorld = camera->position() + (camRay * distance);
        camera->move(glm::dvec3(m_operation_centre.x - mouseInWorld.x, m_operation_centre.y - mouseInWorld.y, m_operation_centre.z - mouseInWorld.z));
    } else {
        double denom = glm::dot(glm::dvec3(0, 0, 1.0), camRay);
        double distance = 0;
        if (denom != 0) {
            distance = glm::dot((m_operation_centre - camera->position()), glm::dvec3(0, 0, 1.0)) / denom;
        }
        auto mouseInWorld = camera->position() + (camRay * distance);
        camera->move(glm::dvec3(m_operation_centre.x - mouseInWorld.x, m_operation_centre.y - mouseInWorld.y, 0));
    }
}

void OrbitInteraction::zoom(float amount, Definition* camera, AbstractDepthTester*)
{
    auto distance = float(glm::distance(m_operation_centre, camera->position()));
    distance = std::max(distance / 100, 0.07f);
    camera->move(glm::normalize(m_operation_centre - camera->position()) * double(amount * distance));
}
}
