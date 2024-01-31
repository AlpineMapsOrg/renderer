/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include <QDateTime>
#include <glm/glm.hpp>

namespace nucleus::utils::sun_calculations {

//Convenience function to calculate azimuth and zenith given latlongalt and datetime
glm::vec2 calculate_sun_angles(const QDateTime& dt, glm::dvec3 latlongalt);

// Calculates direction vector for the given set of sun_angles (azimuth and zenith)
glm::vec3 sun_rays_direction_from_sun_angles(const glm::vec2& sun_angles);

}
