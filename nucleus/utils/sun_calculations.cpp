

#include <math.h>
#include "sun_calculations.h"
#include <QDebug>

namespace nucleus::utils::sun_calculations {

const glm::vec2 prec_sun_angles[] = {
    {   340.52,   112.59  },    // 00:00
    {   64.08,    88.63   },    // 06:00
    {   145.48,   32.92   },    // 12:00
    {   269.01,   64.14   },    // 18:00
    {   340.47,   112.84  }     // 24:00
};

glm::vec2 calculate_sun_angles(const QDateTime& dt, glm::dvec3 latlongalt) {
    // NOTE: This is a fake sun angle calculation which just interpolates azimuth and zenit for
    // calculated data for the 01.August 2023 around Großglockner.
    // WHY? The original calculation code in use is published under the wrong license. Until
    // a replacement is found/done we'll fake the calculation with the mentioned approach.
    int cnt_values = sizeof(prec_sun_angles) / sizeof(glm::vec2);
    float hour = dt.time().hour() + (dt.time().minute() / 60.0f);
    float time_gap = 24.0f / (cnt_values - 1); // should result in 6.0f with cnt_values == 5
    int ind_low = (int)(hour / time_gap);
    float p = (hour - ind_low * time_gap) / time_gap;
    return glm::mix(prec_sun_angles[ind_low], prec_sun_angles[ind_low + 1], p);
}

glm::vec3 sun_rays_direction_from_sun_angles(const glm::vec2& sun_angles){
    auto phi = glm::radians(360.0 - sun_angles.x - 270.0);
    auto theta = glm::radians(sun_angles.y);
    auto dir = glm::vec3(-glm::sin(theta) * glm::cos(phi), -glm::sin(theta) * glm::sin(phi), -glm::cos(theta));
    return glm::normalize(dir);
}

}
