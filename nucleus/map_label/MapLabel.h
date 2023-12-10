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

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "stb_slim/stb_truetype.h"

namespace nucleus {

class MapLabel {

public:
    struct VertexData {
        glm::vec4 position; // start_x, start_y, offset_x, offset_y
        glm::vec4 uv; // start_u, start_v, offset_u, offset_v
        glm::vec3 world_position;
    };

    MapLabel(std::string text, double latitude, double longitude, float altitude, float importance)
        : m_text(text)
        , m_latitude(latitude)
        , m_longitude(longitude)
        , m_altitude(altitude)
        , m_importance(importance) {};

    void init(stbtt_bakedchar* character_data, int char_start, int char_end);

    constexpr static float font_size = 30.0f;
    constexpr static float icon_size = 30.0f;

    const std::vector<VertexData>& vertices() const;

private:
    std::vector<VertexData> m_vertices;

    std::string m_text;
    double m_latitude;
    double m_longitude;
    float m_altitude;
    float m_importance;

    glm::vec3 m_label_position;
};
} // namespace nucleus
