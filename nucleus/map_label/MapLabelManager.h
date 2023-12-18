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

#include "MapLabel.h"
#include <QImage>
#include <vector>

#include "stb_slim/stb_truetype.h"

#include <unordered_map>

namespace nucleus {
class MapLabelManager {
public:
    explicit MapLabelManager();
    ~MapLabelManager();

    void init();
    void createFont();

    const std::vector<MapLabel>& labels() const;
    const std::vector<unsigned int>& indices() const;
    const QImage& font_atlas() const;
    const QImage& icon() const;

private:
    // list of all characters that will be available (will be rendered to the font_atlas)
    const std::string all_char_list = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789()[]{},;.:-_!\"§$%&/\\=+-*/#'~°^<>|@€´`öÖüÜäÄß";

    std::vector<MapLabel> m_labels;

    stbtt_fontinfo m_fontinfo;
    uint8_t* m_font_bitmap;

    static constexpr glm::vec2 m_font_outline = glm::vec2(2, 2);
    static constexpr glm::vec2 m_font_padding = glm::vec2(2, 2);

    void inline make_outline(uint8_t* temp_bitmap, int lasty);

    std::vector<unsigned int> m_indices;

    std::unordered_map<int, const MapLabel::CharData> m_char_data;

    QImage m_font_atlas;
    QImage m_icon;

    QSize m_font_atlas_size;
};
} // namespace nucleus
