/*****************************************************************************
 * AlpineMaps.org
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
#include "gesture.h"
#include <QDebug>
#include <glm/ext/scalar_constants.hpp>

namespace nucleus::camera {

OrbitInteraction::OrbitInteraction()
{
    auto detectors = std::vector<std::unique_ptr<gesture::Detector>>();
    detectors.push_back(std::make_unique<gesture::PanDetector>());
    detectors.push_back(std::make_unique<gesture::TapTapMoveDetector>());
    detectors.push_back(std::make_unique<gesture::ShoveDetector>(gesture::ShoveDetector::Type::Vertical));
    detectors.push_back(std::make_unique<gesture::PinchAndRotateDetector>(gesture::PinchAndRotateDetector::Mode::PinchOnly));
    detectors.push_back(std::make_unique<gesture::PinchAndRotateDetector>(gesture::PinchAndRotateDetector::Mode::PinchAndRotate));

    m_gesture_controller = std::make_unique<gesture::Controller>(std::move(detectors));
}

OrbitInteraction::~OrbitInteraction() = default;

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
    auto gesture = m_gesture_controller->analise(e, camera.viewport_size());
    if (!gesture) {
        m_operation_centre = {};
        m_operation_centre_screen = {};
        return {};
    }

    if (gesture->just_activated) {
        assert(gesture->values.contains(gesture::ValueName::Pivot));
        const auto& pivot = gesture->values[gesture::ValueName::Pivot];
        start(pivot, camera, depth_tester); // Set m_operation_centre
        m_operation_centre_screen = pivot;
    }

    if (gesture->values.contains(gesture::ValueName::Pan))
        pan(gesture->values[gesture::ValueName::Pivot], {}, &camera, depth_tester);

    if (gesture->values.contains(gesture::ValueName::TapTapMoveRelative)) {
        zoom(gesture->values[gesture::ValueName::TapTapMoveRelative].y * 0.8f, &camera, depth_tester);
    }
    if (gesture->values.contains(gesture::ValueName::Pinch)) {
        const auto distance = float(glm::distance(m_operation_centre, camera.position()));
        const auto new_distance = gesture->values[gesture::ValueName::Pinch].x * distance;
        camera.move(glm::normalize(m_operation_centre - camera.position()) * double(new_distance - distance));
    }

    if (gesture->values.contains(gesture::ValueName::Shove)) {
        const auto shove = gesture->values[gesture::ValueName::Shove];
        camera.orbit_clamped(m_operation_centre, glm::vec2(-shove) * 0.2f);
    }
    if (gesture->values.contains(gesture::ValueName::Rotation)) {
        const auto rotation = gesture->values[gesture::ValueName::Rotation].x;
        camera.orbit_clamped(m_operation_centre, glm::vec2(rotation, 0));
    }

    return camera;
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

std::optional<glm::vec2> OrbitInteraction::operation_centre() { return -m_operation_centre_screen; }

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
