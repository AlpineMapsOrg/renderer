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

#include "MapLabel.h"

#include "CharUtils.h"
#include "srs.h"

#include <QDebug>

#include "stb_slim/stb_truetype.h"

namespace nucleus {

void MapLabel::init(const std::unordered_map<int, const MapLabel::CharData>& character_data, const stbtt_fontinfo* fontinfo, const float uv_width_norm)
{
    constexpr float offset_y = -font_size / 2.0f + 65.0f;
    constexpr float icon_offset_y = 10.0f;

    std::string altitude_text = std::to_string(m_altitude);
    altitude_text = altitude_text.substr(0, altitude_text.find("."));
    std::string rendered_text = m_text + " (" + altitude_text + "m)";

    glm::vec3 label_position = nucleus::srs::lat_long_alt_to_world({ m_latitude, m_longitude, m_altitude });

    std::cout << m_text << ": " << label_position.x << ", " << label_position.y << std::endl;

    std::vector<int> safe_chars = CharUtils::string_to_unicode_int_list(rendered_text);
    float text_width = 0;
    std::vector<float> kerningOffsets = create_text_meta(character_data, fontinfo, safe_chars, text_width);

    // center the text around the center
    const auto offset_x = -text_width / 2.0f;

    // label icon
    m_vertex_data.push_back({ glm::vec4(-icon_size.x / 2.0f, icon_size.y / 2.0f + icon_offset_y, icon_size.x, -icon_size.y + 1), // vertex position + offset
        glm::vec4(10.0f, 10.0f, 1, 1), // uv position + offset
        label_position, m_importance });

    for (unsigned long long i = 0; i < safe_chars.size(); i++) {

        const MapLabel::CharData b = character_data.at(safe_chars[i]);

        m_vertex_data.push_back({ glm::vec4(offset_x + kerningOffsets[i] + b.xoff, offset_y - b.yoff, b.width, -b.height), // vertex position + offset
            glm::vec4(b.x * uv_width_norm, b.y * uv_width_norm, b.width * uv_width_norm, b.height * uv_width_norm), // uv position + offset
            label_position, m_importance });
    }
}

const std::vector<MapLabel::VertexData>& MapLabel::vertex_data() const
{
    return m_vertex_data;
}

// calculate char offsets and text width
std::vector<float> inline MapLabel::create_text_meta(const std::unordered_map<int, const MapLabel::CharData>& character_data, const stbtt_fontinfo* fontinfo, std::vector<int>& safe_chars, float& text_width)
{
    std::vector<float> kerningOffsets;

    float scale = stbtt_ScaleForPixelHeight(fontinfo, font_size);
    float xOffset = 0;
    for (unsigned long long i = 0; i < safe_chars.size(); i++) {
        if (!character_data.contains(safe_chars[i])) {
            qDebug() << "character with unicode index(Dec: " << safe_chars[i] << ") cannot be shown -> please add it to nucleus/map_label/MapLabelManager.h.all_char_list";
            safe_chars[i] = 32; // replace with space character
        }

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
        if (!character_data.contains(safe_chars[safe_chars.size() - 1])) {
            qDebug() << "character with unicode index(Dec: " << safe_chars[safe_chars.size() - 1] << ") cannot be shown -> please add it to nucleus/map_label/MapLabelManager.h.all_char_list";
            safe_chars[safe_chars.size() - 1] = 32; // replace with space character
        }
        const MapLabel::CharData b = character_data.at(safe_chars[safe_chars.size() - 1]);

        text_width = xOffset + b.width;
    }

    return kerningOffsets;
}

} // namespace nucleus
