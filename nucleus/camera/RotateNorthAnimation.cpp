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

#include "RotateNorthAnimation.h"
#include "AbstractDepthTester.h"

#include <QDebug>

namespace nucleus::camera {

RotateNorthAnimation::RotateNorthAnimation(Definition camera, AbstractDepthTester* depth_tester)
{
    m_operation_centre = depth_tester->position(glm::dvec2(0.0, 0.0));
    m_operation_centre_screen = glm::vec2(float(camera.viewport_size().x) / 2.0f,
        float(camera.viewport_size().y) / 2.0f);

    auto cameraFrontAxis = camera.z_axis();
    m_degrees_from_north = glm::degrees(glm::acos(glm::dot(glm::normalize(glm::vec3(cameraFrontAxis.x, cameraFrontAxis.y, 0)), glm::vec3(0, -1, 0))));

    m_total_duration = int(m_degrees_from_north * 5.f + 500.f);
    m_current_duration = 0;

    m_stopwatch.restart();
}

std::optional<Definition> RotateNorthAnimation::update(Definition camera, AbstractDepthTester*)
{
    if (m_current_duration >= m_total_duration) {
        return {};
    }

    auto dt = int(m_stopwatch.lap().count());
    if (dt > 120) { // catches big time steps
        dt = 120;
    }
    if (m_current_duration + dt > m_total_duration) { // last step
        dt = m_total_duration - m_current_duration;
    }

    float dt_eased = ease_in_out((float(m_current_duration + dt)) / float(m_total_duration)) - ease_in_out(float(m_current_duration) / float(m_total_duration));

    if (camera.z_axis().x > 0) {
        camera.orbit(m_operation_centre, glm::vec2(-m_degrees_from_north * dt_eased, 0));
    } else {
        camera.orbit(m_operation_centre, glm::vec2(m_degrees_from_north * dt_eased, 0));
    }
    m_current_duration += dt;
    return camera;
}

std::optional<glm::vec2> RotateNorthAnimation::operation_centre()
{
    return m_operation_centre_screen;
}

float RotateNorthAnimation::ease_in_out(float t)
{
    const float p = 0.3f;
    if (t < 0.5f) {
        return 0.5f * pow(2 * t, 1.0f / p);
    } else {
        return 1 - 0.5f * pow(2 * (1 - t), 1.0f / p);
    }
}
}
