/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

namespace nucleus::utils::bit_coding {
inline glm::vec2 to_f16f16(const glm::u8vec4& v)
{
    const auto dec = [](uint8_t v1, uint8_t v2) {
        return float((v1 << 8) | v2) / 65535.f;
    };
    return { dec(v[0], v[1]), dec(v[2], v[3]) };
};

}
