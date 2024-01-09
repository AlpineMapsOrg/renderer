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

void CadInteraction::reset_interaction(Definition camera, AbstractDepthTester* depth_tester)
{
    m_operation_centre = depth_tester->position(glm::dvec2(0.0, 0.0));
    m_operation_centre_screen = glm::vec2(float(camera.viewport_size().x) / 2.0f, float(camera.viewport_size().y) / 2.0f);
}

std::optional<Definition> CadInteraction::mouse_press_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester)
{
    if (m_operation_centre.x == 0 && m_operation_centre.y == 0 && m_operation_centre.z == 0) {
        reset_interaction(camera, depth_tester);
    }
    if (e.buttons == Qt::LeftButton && m_stopwatch.lap().count() < 300) { // double click
        m_interpolation_start = m_operation_centre;
        m_interpolation_target = depth_tester->position(camera.to_ndc({ e.point.position().x(), e.point.position().y() }));
        m_interpolation_duration = 0;

        return camera;
    }
    return {};
}

std::optional<Definition> CadInteraction::mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester*)
{
    if (e.buttons == Qt::LeftButton && !m_key_ctrl && !m_key_alt) {
        const auto delta = e.point.position() - e.point.lastPosition();
        double dist = glm::distance(camera.position(), m_operation_centre);
        double moveSpeedModifier = 750.0;

        m_operation_centre = m_operation_centre - camera.x_axis() * delta.x() * (dist / moveSpeedModifier);
        m_operation_centre = m_operation_centre + camera.y_axis() * delta.y() * (dist / moveSpeedModifier);
        camera.move(-camera.x_axis() * delta.x() * (dist / moveSpeedModifier));
        camera.move(camera.y_axis() * delta.y() * (dist / moveSpeedModifier));
    }
    if (e.buttons == Qt::MiddleButton || (e.buttons == Qt::LeftButton && m_key_ctrl && !m_key_alt)) {
        const auto delta = e.point.position() - e.point.lastPosition();
        camera.orbit_clamped(m_operation_centre, glm::vec2(delta.x(), delta.y()) * -0.1f);
    }
    if (e.buttons == Qt::RightButton || (e.buttons == Qt::LeftButton && !m_key_ctrl && m_key_alt)) {
        const auto delta = e.point.position() - e.point.lastPosition();
        double dist = glm::distance(camera.position(), m_operation_centre);
        double zoomDist = -(delta.y() + delta.x()) * dist / 400.0;
        if (zoomDist < dist) {
            if (dist > 5.0 || zoomDist > 0.0) { // always allow zoom out
                camera.zoom(zoomDist);
            } else {
                m_operation_centre = m_operation_centre - camera.z_axis() * 300.0;
            }
        }
    }

    if (e.buttons == Qt::NoButton)
        return {};
    else
        return camera;
}

std::optional<Definition> CadInteraction::wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester)
{
    if (m_operation_centre.x == 0 && m_operation_centre.y == 0 && m_operation_centre.z == 0) {
        reset_interaction(camera, depth_tester);
    }

    double dist = glm::distance(camera.position(), m_operation_centre);
    if (e.angle_delta.y() > 0) {
        if (dist > 5.0) {
            camera.zoom(-dist / 10.0);
        } else {
            m_operation_centre = m_operation_centre - camera.z_axis() * 300.0;
        }
    } else {
        camera.zoom(dist / 10.0);
    }
    return camera;
}

std::optional<Definition> CadInteraction::key_press_event(const QKeyCombination& e, Definition camera, AbstractDepthTester*)
{
    if (e.key() == Qt::Key_Control) {
        m_key_ctrl = true;
    }
    if (e.key() == Qt::Key_Alt) {
        m_key_alt = true;
    }
    return camera;
}

std::optional<Definition> CadInteraction::key_release_event(const QKeyCombination& e, Definition camera, AbstractDepthTester*)
{
    if (e.key() == Qt::Key_Control) {
        m_key_ctrl = false;
    }
    if (e.key() == Qt::Key_Alt) {
        m_key_alt = false;
    }
    return camera;
}

std::optional<Definition> CadInteraction::update(Definition camera, AbstractDepthTester*)
{
    int total_duration = 90;

    if (m_interpolation_duration >= total_duration) {
        return {};
    }

    auto dt = m_stopwatch.lap().count();

    if (m_interpolation_duration + dt > total_duration) { // last step
        dt = total_duration - m_interpolation_duration;
    }

    auto total_move = m_interpolation_target - m_interpolation_start;
    double step = (double)dt / total_duration;

    camera.move(total_move * step);
    m_operation_centre = m_operation_centre + total_move * step;

    m_interpolation_duration += dt;
    return camera;
}

std::optional<glm::vec2> CadInteraction::operation_centre(){
    return m_operation_centre_screen;
}

std::optional<float> CadInteraction::operation_centre_distance(Definition camera){
    return glm::distance(m_operation_centre, camera.position());
}
}
