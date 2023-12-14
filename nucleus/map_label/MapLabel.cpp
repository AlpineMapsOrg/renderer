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

#include "MapLabel.h"

#include "nucleus/srs.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <string>

#include "CharUtils.h"

namespace nucleus {

void MapLabel::init(const std::unordered_map<int, const MapLabel::CharData>& character_data, const stbtt_fontinfo* fontinfo)
{
    float offset_x = 0;
    float offset_y = -font_size / 2.0f + 75.0;

    float uv_width_norm = 1.0f / 512.0f;
    float uv_height_norm = 1.0f / 512.0f;

    const float text_spacing = 1.0f;

    std::string altitude_text = std::to_string(m_altitude);
    altitude_text = altitude_text.substr(0, altitude_text.find("."));
    std::string rendered_text = m_text + " (" + altitude_text + "m)";

    m_label_position = nucleus::srs::lat_long_alt_to_world({ m_latitude, m_longitude, m_altitude });

    std::vector<int> safe_chars = CharUtils::string_to_unicode_int_list(rendered_text);
    float text_width = 0;
    std::vector<float> kerningOffsets = createTextMeta(character_data, fontinfo, safe_chars, text_width);

    // center the text around the center
    offset_x -= text_width / 2.0;

    // label icon
    m_vertices.push_back({ glm::vec4(-icon_size.x / 2.0, icon_size.y / 2.0, icon_size.x, -icon_size.y), // vertex position + offset
        glm::vec4(10.0, 10.0, 1, 1), // uv position + offset
        m_label_position });

    for (int i = 0; i < safe_chars.size(); i++) {

        const MapLabel::CharData b = character_data.at(safe_chars[i]);

        m_vertices.push_back({ glm::vec4(offset_x + kerningOffsets[i] + b.xoff, offset_y - b.yoff, b.width, -b.height), // vertex position + offset
            glm::vec4(b.x * uv_width_norm, b.y * uv_width_norm, b.width * uv_width_norm, b.height * uv_width_norm), // uv position + offset
            m_label_position });
    }
}

const std::vector<MapLabel::VertexData>& MapLabel::vertices() const
{
    return m_vertices;
}

// calculate char offsets and text width
std::vector<float> inline MapLabel::createTextMeta(const std::unordered_map<int, const MapLabel::CharData>& character_data, const stbtt_fontinfo* fontinfo, std::vector<int> safe_chars, float& text_width)
{
    std::vector<float> kerningOffsets;

    float scale = stbtt_ScaleForPixelHeight(fontinfo, font_size);
    float xOffset = 0;
    for (int i = 0; i < safe_chars.size(); i++) {
        //        std::cout << "checking: " << safe_chars[i] << std::endl;
        assert(character_data.contains(safe_chars[i]));

        int advance, lsb;
        stbtt_GetCodepointHMetrics(fontinfo, safe_chars[i], &advance, &lsb);

        kerningOffsets.push_back(xOffset);

        xOffset += advance * scale;
        if (i + 1 < safe_chars.size())
            xOffset += scale * stbtt_GetCodepointKernAdvance(fontinfo, safe_chars[i], safe_chars[i + 1]);
    }
    kerningOffsets.push_back(xOffset);

    { // get width of last char
        //        std::cout << "checking: " << safe_chars[safe_chars.size() - 1] << std::endl;
        assert(character_data.contains(safe_chars[safe_chars.size() - 1]));
        const MapLabel::CharData b = character_data.at(safe_chars[safe_chars.size() - 1]);

        text_width = xOffset + b.width;
    }

    return std::move(kerningOffsets);
}

} // namespace nucleus
