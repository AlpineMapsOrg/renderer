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
#include <vector>

#include "stb_slim/stb_truetype.h"

namespace nucleus {
class MapLabelManager {
public:
    explicit MapLabelManager();

    void init();
    void createFont();

    const uint8_t* font_bitmap() const;
    const std::vector<MapLabel>& labels() const;
    const std::vector<unsigned int>& indices() const;

private:
    std::vector<MapLabel> m_labels;
    stbtt_fontinfo m_fontinfo;
    stbtt_bakedchar m_character_data[223]; // stores 223 ascii characters (characters 32-255) -> should include all commonly used german characters
    uint8_t* m_font_bitmap;
    std::vector<unsigned int> m_indices;
};
} // namespace nucleus
