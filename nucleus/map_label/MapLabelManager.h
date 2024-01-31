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

#include "MapLabel.h"

#include <QImage>
#include <stb_slim/stb_truetype.h>
#include <unordered_map>
#include <vector>

#include "../Raster.h"

namespace nucleus {
class MapLabelManager {
public:
    explicit MapLabelManager();

    const std::vector<MapLabel>& labels() const;
    const std::vector<unsigned int>& indices() const;
    const Raster<glm::u8vec2>& font_atlas() const;
    const QImage& icon() const;

private:
    void init();
    Raster<uint8_t> make_font_raster();
    Raster<glm::u8vec2> make_outline(const Raster<uint8_t>& font_bitmap);

private:
    // list of all characters that will be available (will be rendered to the font_atlas)
    const QString all_char_list = QString::fromUtf16(u" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789()[]{},;.:-_!\"§$%&/\\=+-*/#'~°^<>|@€´`öÖüÜäÄß");

    // static constexpr glm::ivec2 m_font_outline = glm::ivec2(3, 3);
    static constexpr float m_font_outline = 7.2f;
    static constexpr glm::ivec2 m_font_padding = glm::ivec2(2, 2);
    static constexpr QSize m_font_atlas_size = QSize(512, 512);
    static constexpr float uv_width_norm = 1.0f / m_font_atlas_size.width();

    std::vector<MapLabel> m_labels;
    std::vector<unsigned int> m_indices;

    std::unordered_map<char16_t, const MapLabel::CharData> m_char_data;

    stbtt_fontinfo m_fontinfo;
    QByteArray m_font_file;

    Raster<glm::u8vec2> m_font_atlas;
    QImage m_icon;
};
} // namespace nucleus
