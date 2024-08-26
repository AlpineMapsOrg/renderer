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

#include <stb_slim/stb_truetype.h>

#include <QSize>
#include <set>
#include <unordered_map>
#include <vector>

#include <nucleus/Raster.h>
#include <nucleus/map_label/MapLabelData.h>

namespace nucleus::maplabel {


struct FontData {
    float uv_width_norm;
    stbtt_fontinfo fontinfo;
    std::unordered_map<char16_t, CharData> char_data;
};

class FontRenderer
{
public:

    void init();
    void render(std::set<char16_t> chars, float font_size);
    const FontData& font_data();
    std::vector<Raster<glm::u8vec2>> font_atlas();

    static constexpr QSize m_font_atlas_size = QSize(1024, 1024);
    static constexpr int m_max_textures = 8;

private:
    void render_text(std::set<char16_t> chars, float font_size);
    void make_outline(std::set<char16_t> chars);


    static constexpr float m_font_outline = 7.2f;
    static constexpr glm::ivec2 m_font_padding = glm::ivec2(2, 2);
    static constexpr float m_uv_width_norm = 1.0f / m_font_atlas_size.width();

    int m_outline_margin;
    int m_x;
    int m_y;
    int m_bottom_y;
    int m_texture_index;

    FontData m_font_data;

    std::vector<Raster<glm::u8vec2>> m_font_atlas;

    QByteArray m_font_file;

};

} // namespace nucleus::maplabel

