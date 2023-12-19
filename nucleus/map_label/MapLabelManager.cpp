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

#include "MapLabelManager.h"

#include <QFile>
#include <QIcon>
#include <QSize>

#include "CharUtils.h"

#include <iostream>
#include <string>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_slim/stb_truetype.h"

namespace nucleus {

MapLabelManager::MapLabelManager()
    : m_font_atlas(m_font_atlas_size, QImage::Format_RGB32)
{
    //    const char8_t text;
    //    double latitude;
    //    double longitude;
    //    float altitude;
    //    float importance;  // 1 -> most important; 0 -> least important
    m_labels.push_back({ "Großglockner", 47.07455, 12.69388, 3798, 1 });
    m_labels.push_back({ "Piz Buin", 46.84412, 10.11889, 3312, 0.56 });
    m_labels.push_back({ "Hoher Dachstein", 47.47519, 13.60569, 2995, 0.56 });
    m_labels.push_back({ "Großer Priel", 47.71694, 14.06325, 2515, 0.56 });
    m_labels.push_back({ "Hermannskogel", 48.27072, 16.29456, 544, 0.56 });
    m_labels.push_back({ "Klosterwappen", 47.76706, 15.80450, 2076, 0.56 });
    m_labels.push_back({ "Ötscher", 47.86186, 15.20251, 1893, 0.56 });
    m_labels.push_back({ "Ellmauer Halt", 47.5616377, 12.3025296, 2342, 0.56 });
    m_labels.push_back({ "Wildspitze", 46.88524, 10.86728, 3768, 0.56 });
    m_labels.push_back({ "Großvenediger", 47.10927, 12.34534, 3657, 0.56 });
    m_labels.push_back({ "Hochalmspitze", 47.01533, 13.32050, 3360, 0.56 });
    m_labels.push_back({ "Geschriebenstein", 47.35283, 16.43372, 884, 0.56 });

    m_labels.push_back({ "Ackerlspitze", 47.559125, 12.347188, 2329, 0.33 });
    m_labels.push_back({ "Scheffauer", 47.5573214, 12.2418396, 2111, 0.33 });
    m_labels.push_back({ "Maukspitze", 47.5588954, 12.3563668, 2231, 0.33 });
    m_labels.push_back({ "Schönfeldspitze", 47.45831, 12.93774, 2653, 0.33 });
    m_labels.push_back({ "Hochschwab", 47.61824, 15.14245, 2277, 0.33 });

    m_labels.push_back({ "Valluga", 47.15757, 10.21309, 2811, 0.11 });
    m_labels.push_back({ "Birkkarspitze", 47.41129, 11.43765, 2749, 0.11 });
    m_labels.push_back({ "Schafberg", 47.77639, 13.43389, 1783, 0.11 });

    m_labels.push_back({ "Grubenkarspitze", 47.38078, 11.52211, 2663, 0.11 });
    m_labels.push_back({ "Gimpel", 47.50127, 10.61249, 2176, 0.11 });
    m_labels.push_back({ "Seekarlspitze", 47.45723, 11.77804, 2261, 0.11 });
    m_labels.push_back({ "Furgler", 47.04033, 10.51186, 3004, 0.11 });

    m_labels.push_back({ "Westliche Hochgrubachspitze", 47.5583658, 12.3433997, 2277, 0 });
    m_labels.push_back({ "Östliche Hochgrubachspitze", 47.5587933, 12.3450985, 2284, 0 });
}

MapLabelManager::~MapLabelManager()
{
    delete[] m_font_bitmap;
}

void MapLabelManager::init()
{
    m_indices.push_back(0);
    m_indices.push_back(1);
    m_indices.push_back(2);

    m_indices.push_back(0);
    m_indices.push_back(2);
    m_indices.push_back(3);

    create_font();

    for (auto& label : m_labels) {
        label.init(m_char_data, &m_fontinfo);
    }

    // paint svg icon into the an image of appropriate size
    m_icon = QIcon(QString(":/qt/qml/app/icons/peak.svg")).pixmap(QSize(MapLabel::icon_size.x, MapLabel::icon_size.y)).toImage();
}

void inline MapLabelManager::make_outline(uint8_t* temp_bitmap, int lasty)
{
    // create the final font bitmap (3 channels -> 1 for font; 1 for outline; the last channel is empty)
    const int channels = 3;
    m_font_bitmap = new uint8_t[m_font_atlas_size.width() * m_font_atlas_size.height() * channels];
    // set everything to 0
    memset(m_font_bitmap, 0, m_font_atlas_size.width() * m_font_atlas_size.height() * channels);

    const int outline_bit_count = 4 * (m_font_outline.x - 1) * (m_font_outline.y - 1) + 2 * (m_font_outline.x - 1) + 2 * (m_font_outline.y - 1) + 1;
    int* outline_mask = new int[outline_bit_count];
    int index = 0;
    for (int i = -m_font_outline.y + 1; i < m_font_outline.y; i++) {
        for (int j = -m_font_outline.x + 1; j < m_font_outline.x; j++) {
            outline_mask[index++] = (i * m_font_atlas_size.width() + j) * channels + 1;
        }
    }

    // value that is added to the outline
    constexpr uint8_t outline_damp_factor = 50;

    for (int i = 0; i < lasty; ++i) {
        for (int j = 0; j < m_font_atlas_size.width(); ++j) {
            const uint8_t value = temp_bitmap[(i * m_font_atlas_size.width() + j)];
            const int current_index = (i * m_font_atlas_size.width() + j) * channels;
            m_font_bitmap[current_index] = value;

            // add the outline to the outline channel
            if (value != 0) {
                // if the current pixel is part of the font itself -> be fully visible
                m_font_bitmap[current_index + 1] = 255;
                // calculate the outline for the outer parts
                for (int k = 0; k < outline_bit_count; k++) {
                    // increase the value by a factor
                    uint8_t current_outline_value = m_font_bitmap[outline_mask[k] + current_index] + outline_damp_factor;
                    if (current_outline_value < outline_damp_factor) // clamp the value (since we are working with uint8 -> values over 255 will flow over -> we therefore check for a value below the damp_factor)
                        current_outline_value = 255;

                    m_font_bitmap[outline_mask[k] + current_index] = current_outline_value;
                }
            }
        }
    }

    delete[] outline_mask;
}

void MapLabelManager::create_font()
{
    // load ttf file
    QFile file(":/fonts/SourceSans3-Medium.ttf");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    const QByteArray data = file.readAll();

    // init font and get info about the dimensions
    const auto font_init = stbtt_InitFont(&m_fontinfo, reinterpret_cast<const uint8_t*>(data.constData()), stbtt_GetFontOffsetForIndex(reinterpret_cast<const uint8_t*>(data.constData()), 0));
    assert(font_init);

    uint8_t* temp_bitmap = new uint8_t[m_font_atlas_size.width() * m_font_atlas_size.height()];

    const std::vector<int> safe_chars = CharUtils::string_to_unicode_int_list(all_char_list);

    float scale = stbtt_ScaleForPixelHeight(&m_fontinfo, MapLabel::font_size);
    STBTT_memset(temp_bitmap, 0, m_font_atlas_size.width() * m_font_atlas_size.height()); // background of 0 around pixel

    int x = m_font_outline.x + m_font_padding.x;
    int y = m_font_outline.y + m_font_padding.y;
    int bottom_y = m_font_outline.y + m_font_padding.y;

    for (const int& c : safe_chars) {
        // code adapted from stbtt_BakeFontBitmap()
        int x0, y0, x1, y1, glyph_width, glyph_height;
        const int glyph_index = stbtt_FindGlyphIndex(&m_fontinfo, c);
        stbtt_GetGlyphBitmapBox(&m_fontinfo, glyph_index, scale, scale, &x0, &y0, &x1, &y1);

        glyph_width = x1 - x0;
        glyph_height = y1 - y0;
        if (x + glyph_width + 2 * m_font_outline.x + m_font_padding.x >= m_font_atlas_size.width())
            y = bottom_y, x = 2 * m_font_outline.x + m_font_padding.x; // advance to next row
        if (y + glyph_height + m_font_outline.y + m_font_padding.y >= m_font_atlas_size.height()) // check if it fits vertically AFTER potentially moving to next row
        {
            std::cout << "Font doesnt fit into bitmap" << std::endl;
            assert(false);
            break; // doesnt fit in image
        }

        stbtt_MakeGlyphBitmap(&m_fontinfo, temp_bitmap + x + y * m_font_atlas_size.width(), glyph_width, glyph_height, m_font_atlas_size.width(), scale, scale, glyph_index);
        m_char_data.emplace(c, MapLabel::CharData { (unsigned short)(x - m_font_outline.x), (unsigned short)(y - m_font_outline.y), (unsigned short)(glyph_width + m_font_outline.x * 2), (unsigned short)(glyph_height + m_font_outline.y * 2), (float)x0 - m_font_outline.x, (float)y0 - m_font_outline.y });

        x = x + glyph_width + 2 * m_font_outline.x + m_font_padding.x;
        if (y + glyph_height + m_font_outline.y + m_font_padding.y > bottom_y)
            bottom_y = y + glyph_height + 2 * m_font_outline.y + m_font_padding.y;
    }

    make_outline(temp_bitmap, bottom_y);

    delete[] temp_bitmap;

    // create a qimage with the data
    m_font_atlas = QImage(m_font_bitmap, m_font_atlas_size.width(), m_font_atlas_size.height(), QImage::Format_RGB888);
    //    m_font_atlas.save("font_atlas.png");
}

const std::vector<MapLabel>& MapLabelManager::labels() const
{
    return m_labels;
}

const std::vector<unsigned int>& MapLabelManager::indices() const
{
    return m_indices;
}

const QImage& MapLabelManager::icon() const
{
    return m_icon;
}
const QImage& MapLabelManager::font_atlas() const
{
    return m_font_atlas;
}

} // namespace nucleus
