/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include <cmath>

// Easing functions from https://easings.net/

namespace nucleus::utils::easing {

inline float easeInQuad(float x) { return x * x; }

inline float easeOutQuad(float x) { return 1.0f - (1.0f - x) * (1.0f - x); }

inline float easeOutElastic(float x)
{
    constexpr float c4 = (2.0f * 3.14159265359f) / 3.0f;
    if (x == 0.0f)
        return 0.0f;
    if (x == 1.0f)
        return 1.0f;
    return std::pow(2.0f, -10.0f * x) * std::sin((x * 10.0f - 0.75f) * c4) + 1.0f;
}

inline float easeInElastic(float x)
{
    constexpr float c4 = (2.0f * 3.14159265359f) / 3.0f;
    if (x == 0.0f)
        return 0.0f;
    if (x == 1.0f)
        return 1.0f;
    return -std::pow(2.0f, 10.0f * x - 10.0f) * std::sin((x * 10.0f - 10.75f) * c4);
}

inline float easeOutBounce(float x)
{
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;
    if (x < 1.0f / d1) {
        return n1 * x * x;
    } else if (x < 2.0f / d1) {
        x -= 1.5f / d1;
        return n1 * x * x + 0.75f;
    } else if (x < 2.5f / d1) {
        x -= 2.25f / d1;
        return n1 * x * x + 0.9375f;
    } else {
        x -= 2.625f / d1;
        return n1 * x * x + 0.984375f;
    }
}

inline float easeInBounce(float x) { return 1.0f - easeOutBounce(1.0f - x); }

} // namespace nucleus::utils::easing
