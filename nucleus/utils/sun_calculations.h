
#pragma once

#include <QDateTime>
#include <glm/glm.hpp>

namespace nucleus::utils::sun_calculations {

//Convenience function to calculate azimuth and zenith given latlongalt and datetime
glm::vec2 calculate_sun_angles(const QDateTime& dt, glm::dvec3 latlongalt);

glm::vec3 sun_rays_direction_from_sun_angles(const glm::vec2& sun_angles);

}
