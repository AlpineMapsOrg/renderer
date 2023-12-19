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

#include <unordered_map>

namespace nucleus {

class MapLabel {

public:
    struct CharData {
        unsigned short x, y, width, height; // coordinates of bbox in bitmap
        float xoff, yoff; // position offsets for e.g. lower/uppercase
    };
    struct VertexData {
        glm::vec4 position; // start_x, start_y, offset_x, offset_y
        glm::vec4 uv; // start_u, start_v, offset_u, offset_v
        glm::vec3 world_position;
        float importance;
    };

    MapLabel(std::string text, double latitude, double longitude, float altitude, float importance)
        : m_text(text)
        , m_latitude(latitude)
        , m_longitude(longitude)
        , m_altitude(altitude)
        , m_importance(importance) {};

    void init(const std::unordered_map<int, const MapLabel::CharData>& character_data, const stbtt_fontinfo* fontinfo);

    constexpr static float font_size = 30.0f;
    constexpr static glm::vec2 icon_size = glm::vec2(30.0f);

    const std::vector<VertexData>& vertex_data() const;

private:
    std::vector<float> inline create_text_meta(const std::unordered_map<int, const MapLabel::CharData>& character_data, const stbtt_fontinfo* fontinfo, std::vector<int>& safe_chars, float& text_width);

    std::vector<VertexData> m_vertex_data;

    std::string m_text;
    double m_latitude;
    double m_longitude;
    float m_altitude;
    float m_importance;
};
} // namespace nucleus
