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

#include "RotateNorthInteraction.h"
#include "AbstractDepthTester.h"

#include <QDebug>

namespace nucleus::camera {

std::optional<Definition> RotateNorthInteraction::update(Definition camera, AbstractDepthTester* depth_tester)
{
    if (m_operation_centre.x == 0 && m_operation_centre.y == 0 && m_operation_centre.z == 0) {
        m_operation_centre = depth_tester->position(glm::dvec2(0.0, 0.0));

        auto cameraFrontAxis = camera.z_axis();
        m_degrees_from_north = glm::degrees(glm::acos(glm::dot(glm::normalize(glm::dvec3(cameraFrontAxis.x, cameraFrontAxis.y, 0)), glm::dvec3(0, -1, 0))));
    }
    qDebug() << "counter " << m_counter;
    if (m_counter > 0) {
        if (camera.z_axis().x > 0) {
            camera.orbit(m_operation_centre, glm::vec2(-m_degrees_from_north / 20, 0));
        } else {
            camera.orbit(m_operation_centre, glm::vec2(m_degrees_from_north / 20, 0));
        }
        m_counter--;
        return camera;
    }
    return {};
}
}
