/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include "radix/tile.h"
#include <glm/glm.hpp>

#include <iostream>
#include <string>

namespace nucleus {

struct FeatureTXT {
    long id;
    std::string name;
    int elevation;
    glm::vec3 position;

    void print() const
    {
        std::cout << name << "\t\t" << glm::to_string(position) << "\t" << elevation << "\t" << id << std::endl;
    }

    // hasher code adapted from radix/tile.h
    // bool operator==(const FeatureTXT& other) const { return other.position_round == position_round && other.text == text; }
    // bool operator<(const FeatureTXT& other) const { return std::tie(text, position_round.x, position_round.y) < std::tie(other.text, other.position_round.x, other.position_round.y); }
    // operator std::tuple<std::string, int, int>() const
    // {
    //     return std::make_tuple(text, position_round.x, position_round.y); // round(position.x / 1000.0)
    // }

    // using Hasher = typename hasher::for_tuple<std::string, int, int>;
};

} // namespace nucleus
