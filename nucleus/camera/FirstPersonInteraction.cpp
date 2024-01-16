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

std::optional<Definition> FirstPersonInteraction::mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester*)
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

std::optional<Definition> FirstPersonInteraction::wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester*)
{
    if (e.angle_delta.y() > 0) {
        m_speed_modifyer = std::min(m_speed_modifyer * 1.3f, 4000.0f);
    } else {
        m_speed_modifyer = std::max(m_speed_modifyer / 1.3f, 1.0f);
    }
    return camera;
}

std::optional<Definition> FirstPersonInteraction::key_press_event(const QKeyCombination& e, Definition camera, AbstractDepthTester*)
{
    if (m_keys_pressed == 0) {
        m_stopwatch.restart();
    }
    m_keys_pressed++;

    if (e.key() == Qt::Key_W) {
        m_key_w = true;
    }
    if (e.key() == Qt::Key_S) {
        m_key_s = true;
    }
    if (e.key() == Qt::Key_A) {
        m_key_a = true;
    }
    if (e.key() == Qt::Key_D) {
        m_key_d = true;
    }
    if (e.key() == Qt::Key_E) {
        m_key_e = true;
    }
    if (e.key() == Qt::Key_Q) {
        m_key_q = true;
    }
    if (e.key() == Qt::Key_Shift) {
        m_key_shift = true;
    }
    return camera;
}

std::optional<Definition> FirstPersonInteraction::key_release_event(const QKeyCombination& e, Definition camera, AbstractDepthTester*)
{
    m_keys_pressed--;
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
    if (e.key() == Qt::Key_Shift) {
        m_key_shift = false;
    }
    return camera;
}

std::optional<Definition> FirstPersonInteraction::update(Definition camera, AbstractDepthTester*)
{
    if (m_keys_pressed == 0) {
        return {};
    }
    auto direction = glm::dvec3();
    if (m_key_w) {
        direction -= camera.z_axis();
    }
    if (m_key_s) {
        direction += camera.z_axis();
    }
    if (m_key_a) {
        direction -= camera.x_axis();
    }
    if (m_key_d) {
        direction += camera.x_axis();
    }
    if (m_key_e) {
        direction += camera.y_axis();
    }
    if (m_key_q) {
        direction -= camera.y_axis();
    }
    glm::normalize(direction);
    double dt = m_stopwatch.lap().count();
    if (dt > 120) { // catches big time steps
        dt = 120;
    }
    if (m_key_shift) {
        camera.move(direction * (dt / 30 * m_speed_modifyer * 3));
    } else {
        camera.move(direction * (dt / 30 * m_speed_modifyer));
    }
    return camera;
}
}
