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
#include <vector>

#include "stb_slim/stb_truetype.h"

#include <unordered_map>

namespace nucleus {
class MapLabelManager {
public:
    explicit MapLabelManager();

    void init();

    const std::vector<MapLabel>& labels() const;
    const std::vector<unsigned int>& indices() const;
    const QImage& font_atlas() const;
    const QImage& icon() const;

private:
    // list of all characters that will be available (will be rendered to the font_atlas)
    static constexpr auto all_char_list = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789()[]{},;.:-_!\"§$%&/\\=+-*/#'~°^<>|@€´`öÖüÜäÄß";

    static constexpr glm::ivec2 m_font_outline = glm::ivec2(2, 2);
    static constexpr glm::ivec2 m_font_padding = glm::ivec2(2, 2);
    static constexpr QSize m_font_atlas_size = QSize(512, 512);
    static constexpr float uv_width_norm = 1.0f / m_font_atlas_size.width();

    // 3 channels -> 1 for font; 1 for outline; the last channel is empty
    static constexpr int m_channel_count = 3;

    std::vector<MapLabel> m_labels;
    std::vector<unsigned int> m_indices;

    std::unordered_map<int, const MapLabel::CharData> m_char_data;

    stbtt_fontinfo m_fontinfo;
    std::vector<uint8_t> m_font_bitmap;

    void create_font();
    void inline make_outline(std::vector<uint8_t>& temp_bitmap, const int lasty);

    QImage m_font_atlas;
    QImage m_icon;
};
} // namespace nucleus
