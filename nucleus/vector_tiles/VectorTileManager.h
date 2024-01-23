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
#include <mapbox/vector_tile.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QByteArray>

namespace nucleus {

class print_value {

public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
    std::string operator()(std::vector<mapbox::feature::value> val)
    {
        return "vector";
    }

    std::string operator()(std::unordered_map<std::string, mapbox::feature::value> val)
    {
        return "unordered_map";
    }

    std::string operator()(mapbox::feature::null_value_t val)
    {
        return "null";
    }

    std::string operator()(std::nullptr_t val)
    {
        return "nullptr";
    }
#pragma GCC diagnostic pop

    std::string operator()(uint64_t val)
    {
        return std::to_string(val);
    }
    std::string operator()(int64_t val)
    {
        return std::to_string(val);
    }
    std::string operator()(double val)
    {
        return std::to_string(val);
    }
    std::string operator()(std::string const& val)
    {
        return val;
    }

    std::string operator()(bool val)
    {
        return val ? "true" : "false";
    }
};

struct Peak {
    // name/altitude should not change after initialization
    const std::string name;
    const float altitude;
    // might be updated if a more precise zoom level gives more precise values
    glm::vec2 position;
    float importance;
    int highest_zoom; // the highest zoom level where this info has been loaded

    size_t get_id()
    {
        return Peak::get_id(name, altitude);
    }

    static size_t get_id(const std::string name, const float altitude)
    {
        size_t hash = 0;
        hash ^= std::hash<std::string>()(name) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<float>()(altitude) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

class VectorTileManager {
public:
    explicit VectorTileManager();

    void get_tile(tile::Id id);

    const std::unordered_map<size_t, std::shared_ptr<Peak>>& get_peaks() const;

    void update_tile_data(std::string data, tile::Id id);

private:
    void load_tile(tile::Id id);

    void update_peak(std::string name, float altitude, glm::vec2 position, int zoom_level);

    std::unordered_set<tile::Id, tile::Id::Hasher> loaded_tiles;
    std::unordered_map<size_t, std::shared_ptr<Peak>> loaded_peaks;
};

} // namespace nucleus
