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

#include <unordered_map>
#include <vector>

#include <QSize>
#include <stb_slim/stb_truetype.h>

#include <nucleus/Raster.h>
#include <nucleus/map_label/FontRenderer.h>
#include <nucleus/map_label/MapLabelData.h>
#include <nucleus/vector_tiles/VectorTileFeature.h>

using namespace nucleus::vectortile;

namespace nucleus::maplabel {

class LabelFactory {
public:

    const AtlasData init_font_atlas();
    const AtlasData renew_font_atlas();

    const Raster<glm::u8vec4> get_label_icons();

    const std::vector<VertexData> create_labels(const VectorTile& features);
    void create_label(const QString text, const glm::vec3 position, FeatureType type, const float importance, std::vector<VertexData>& vertex_data);

    static const inline std::vector<unsigned int> indices = { 0, 1, 2, 0, 2, 3 };

private:
    constexpr static float font_size = 48.0f;
    constexpr static glm::vec2 icon_size = glm::vec2(32.0f);

    std::unordered_map<FeatureType, glm::vec4> icon_uvs;

    std::vector<float> inline create_text_meta(std::u16string* safe_chars, float* text_width);

    FontData m_font_data;

    size_t m_last_char_amount;
    std::set<char16_t> m_all_chars;

    FontRenderer m_font_renderer;

};
} // namespace nucleus::maplabel
