/*****************************************************************************
 * Alpine Terrain Renderer
 *
 * Copyright (C) 2011-2015 Vladimir Agafonkin
 *      from: https://github.com/mourner/suncalc
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

#include "sun_calculations.h"
#include <cmath>
#include <QDebug>

namespace nucleus::utils::sun_calculations {

const double rad = M_PI / 180.0;
const double e = rad * 23.4397; // obliquity of the Earth

const double dayMs = 1000.0 * 60.0 * 60.0 * 24.0;
const double J1970 = 2440588.0;
const double J2000 = 2451545.0;

inline double toJulian(const QDateTime& date) {
    return date.toMSecsSinceEpoch() / dayMs - 0.5 + J1970;
}

inline QDateTime fromJulian(double j) {
    double msecssinceepoch = (j + 0.5 - J1970) * dayMs;
    return QDateTime::fromMSecsSinceEpoch((qint64)msecssinceepoch);
}

inline double toDays(const QDateTime& date) {
    return toJulian(date) - J2000;
}

inline double rightAscension(double l, double b)
{
    return atan2(sin(l) * cos(e) - tan(b) * sin(e), cos(l));
}

inline double declination(double l, double b)
{
    return asin(sin(b) * cos(e) + cos(b) * sin(e) * sin(l));
}

inline double azimuth(double H, double phi, double dec)
{
    return atan2(sin(H), cos(H) * sin(phi) - tan(dec) * cos(phi));
}

inline double altitude(double H, double phi, double dec)
{
    return asin(sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(H));
}

inline double siderealTime(double d, double lw)
{
    return rad * (280.16 + 360.9856235 * d) - lw;
}

inline double astroRefraction(double h)
{
    if (h < 0) // the following formula works for positive altitudes only.
        h = 0; // if h = -0.08901179 a div/0 would occur.

    // formula 16.4 of "Astronomical Algorithms" 2nd edition by Jean Meeus (Willmann-Bell, Richmond) 1998.
    // 1.02 / tan(h + 10.26 / (h + 5.10)) h in degrees, result in arc minutes -> converted to rad:
    return 0.0002967 / tan(h + 0.00312536 / (h + 0.08901179));
}

inline double solarMeanAnomaly(double d)
{
    return rad * (357.5291 + 0.98560028 * d);
}

inline double eclipticLongitude(double M)
{

    double C = rad * (1.9148 * sin(M) + 0.02 * sin(2 * M) + 0.0003 * sin(3 * M)); // equation of center
    double P = rad * 102.9372; // perihelion of the Earth

    return M + C + P + M_PI;
}

glm::dvec2 sunCoords(double d)
{
    double M = solarMeanAnomaly(d);
    double L = eclipticLongitude(M);

    return {
        declination(L, 0),
        rightAscension(L, 0)
    };
}

// calculates sun position for a given date and latitude/longitude
glm::dvec2 getPosition(const QDateTime& date, double lat, double lng)
{
    double lw  = rad * -lng;
    double phi = rad * lat;
    double d   = toDays(date);

    glm::dvec2 c  = sunCoords(d);
    double H  = siderealTime(d, lw) - c.y;

    return {
        azimuth(H, phi, c.x),
        altitude(H, phi, c.x)
    };
}

// NOTE: The original suncalc.js library also includes sunrise/sunset and moon
// calculations. If those are necessary at any point during the development
// of this project feel free to also translate the rest of suncalc.js.
// https://github.com/mourner/suncalc

glm::vec2 calculate_sun_angles(const QDateTime& dt, glm::dvec3 latlongalt)
{
    auto sun_pos_in_rad = getPosition(dt, latlongalt.x, latlongalt.y);
    // Note: Original suncalc.js returns azimuth in [-pi,pi], but we need [0,2*pi]
    double az = glm::degrees(sun_pos_in_rad.x + M_PI);
    double alt = glm::degrees(sun_pos_in_rad.y);
    // Zenith angle is complementary to alt angle. They add up to 90°
    double ze = 90.0 - alt;
    return glm::vec2(az, ze);
}

glm::vec3 sun_rays_direction_from_sun_angles(const glm::vec2& sun_angles)
{
    auto phi = glm::radians(360.0 - sun_angles.x - 270.0);
    auto theta = glm::radians(sun_angles.y);
    auto dir = glm::vec3(-glm::sin(theta) * glm::cos(phi), -glm::sin(theta) * glm::sin(phi), -glm::cos(theta));
    return glm::normalize(dir);
}

}
