/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
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

#include <catch2/catch.hpp>

#include <glm/gtx/string_cast.hpp>

#include "sherpa/tile.h"

namespace Catch {

// template<>
template <glm::length_t s, typename T>
struct StringMaker<glm::vec<s, T>> {
    static std::string convert(const glm::vec<s, T>& value)
    {
        return glm::to_string(value);
    }
};

template <>
struct StringMaker<tile::Id> {
    static std::string convert(const tile::Id& value)
    {
        std::string scheme;
        switch (value.scheme) {
        case tile::Scheme::Tms:
            scheme = "Tms";
            break;
        case tile::Scheme::SlippyMap:
            scheme = "SlippyMap";
            break;
        }
        return std::string("tile::Id{") + std::to_string(value.zoom_level) + ", " + glm::to_string(value.coords) + ", " + scheme + "}";
    }
};
}

inline std::ostream& operator<<(std::ostream& os, const tile::Id& value)
{
    std::string scheme;
    switch (value.scheme) {
    case tile::Scheme::Tms:
        scheme = "Tms";
        break;
    case tile::Scheme::SlippyMap:
        scheme = "SlippyMap";
        break;
    }
    std::string text = "tile::Id{" + std::to_string(value.zoom_level) + ", " + glm::to_string(value.coords) + ", " + scheme + "}";
    os << text;
    return os;
}
