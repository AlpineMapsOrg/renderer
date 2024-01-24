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

#include "MapLabelManager.h"

#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QSize>
#include <QStringLiteral>
#include <string>

#include "Raster.h"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_slim/stb_truetype.h"

using namespace Qt::Literals::StringLiterals;

namespace nucleus {

MapLabelManager::MapLabelManager()
{
    //    const char8_t text;
    //    double latitude;
    //    double longitude;
    //    float altitude;
    //    float importance;  // 1 -> most important; 0 -> least important
    m_labels.push_back({ u"Großglockner"_s, 47.07455, 12.69388, 3798, 1 });
    m_labels.push_back({ u"Piz Buin"_s, 46.84412, 10.11889, 3312, 0.56f });
    m_labels.push_back({ u"Hoher Dachstein"_s, 47.47519, 13.60569, 2995, 0.56f });
    m_labels.push_back({ u"Großer Priel"_s, 47.71694, 14.06325, 2515, 0.56f });
    m_labels.push_back({ u"Hermannskogel"_s, 48.27072, 16.29456, 544, 0.56f });
    m_labels.push_back({ u"Klosterwappen"_s, 47.76706, 15.80450, 2076, 0.56f });
    m_labels.push_back({ u"Ötscher"_s, 47.86186, 15.20251, 1893, 0.56f });
    m_labels.push_back({ u"Ellmauer Halt"_s, 47.5616377, 12.3025296, 2342, 0.56f });
    m_labels.push_back({ u"Wildspitze"_s, 46.88524, 10.86728, 3768, 0.56f });
    m_labels.push_back({ u"Großvenediger"_s, 47.10927, 12.34534, 3657, 0.56f });
    m_labels.push_back({ u"Hochalmspitze"_s, 47.01533, 13.32050, 3360, 0.56f });
    m_labels.push_back({ u"Geschriebenstein"_s, 47.35283, 16.43372, 884, 0.56f });

    m_labels.push_back({ u"Ackerlspitze"_s, 47.559125, 12.347188, 2329, 0.33f });
    m_labels.push_back({ u"Scheffauer"_s, 47.5573214, 12.2418396, 2111, 0.33f });
    m_labels.push_back({ u"Maukspitze"_s, 47.5588954, 12.3563668, 2231, 0.33f });
    m_labels.push_back({ u"Schönfeldspitze"_s, 47.45831, 12.93774, 2653, 0.33f });
    m_labels.push_back({ u"Hochschwab"_s, 47.61824, 15.14245, 2277, 0.33f });

    m_labels.push_back({ u"Valluga"_s, 47.15757, 10.21309, 2811, 0.11f });
    m_labels.push_back({ u"Birkkarspitze"_s, 47.41129, 11.43765, 2749, 0.11f });
    m_labels.push_back({ u"Schafberg"_s, 47.77639, 13.43389, 1783, 0.11f });

    m_labels.push_back({ u"Grubenkarspitze"_s, 47.38078, 11.52211, 2663, 0.11f });
    m_labels.push_back({ u"Gimpel"_s, 47.50127, 10.61249, 2176, 0.11f });
    m_labels.push_back({ u"Seekarlspitze"_s, 47.45723, 11.77804, 2261, 0.11f });
    m_labels.push_back({ u"Furgler"_s, 47.04033, 10.51186, 3004, 0.11f });

    m_labels.push_back({ u"Westliche Hochgrubachspitze"_s, 47.5583658, 12.3433997, 2277, 0 });
    m_labels.push_back({ u"Östliche Hochgrubachspitze"_s, 47.5587933, 12.3450985, 2284, 0 });

    init();
}

void MapLabelManager::init()
{
    m_indices.push_back(0);
    m_indices.push_back(1);
    m_indices.push_back(2);

    m_indices.push_back(0);
    m_indices.push_back(2);
    m_indices.push_back(3);

    m_font_atlas = make_outline(make_font_raster());
    {
        Raster<glm::u8vec4> rgba_raster = { m_font_atlas.size(), { 255, 255, 0, 255 } };
        std::transform(m_font_atlas.cbegin(), m_font_atlas.cend(), rgba_raster.begin(), [](const auto& v) { return glm::u8vec4(v.x, v.y, 0, 255); });

        const auto debug_out = QImage(rgba_raster.bytes(), m_font_atlas_size.width(), m_font_atlas_size.height(), QImage::Format_RGBA8888);
        debug_out.save("font_atlas.png");
    }

    for (auto& label : m_labels) {
        label.init(m_char_data, &m_fontinfo, uv_width_norm);
    }

    // paint svg icon into the an image of appropriate size
    m_icon = QIcon(QString(":/qt/qml/app/icons/peak.svg")).pixmap(QSize(MapLabel::icon_size.x, MapLabel::icon_size.y)).toImage();
}

Raster<uint8_t> MapLabelManager::make_font_raster()
{
    // load ttf file
    QFile file(":/fonts/SourceSans3-Bold.ttf");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    m_font_file = file.readAll();

    // init font and get info about the dimensions
    const auto font_init = stbtt_InitFont(&m_fontinfo, reinterpret_cast<const uint8_t*>(m_font_file.constData()),
        stbtt_GetFontOffsetForIndex(reinterpret_cast<const uint8_t*>(m_font_file.constData()), 0));
    assert(font_init);

    auto raster = Raster<uint8_t>({ m_font_atlas_size.width(), m_font_atlas_size.height() }, uint8_t(0));

    const auto safe_chars = all_char_list.toStdU16String();

    float scale = stbtt_ScaleForPixelHeight(&m_fontinfo, MapLabel::font_size);

    int outline_margin = int(std::ceil(m_font_outline));
    int x = outline_margin + m_font_padding.x;
    int y = outline_margin + m_font_padding.y;
    int bottom_y = outline_margin + m_font_padding.y;

    for (const char16_t& c : safe_chars) {
        // code adapted from stbtt_BakeFontBitmap()
        int x0, y0, x1, y1;
        const int glyph_index = stbtt_FindGlyphIndex(&m_fontinfo, c);
        stbtt_GetGlyphBitmapBox(&m_fontinfo, glyph_index, scale, scale, &x0, &y0, &x1, &y1);

        const auto glyph_width = x1 - x0;
        const auto glyph_height = y1 - y0;
        if (x + glyph_width + 2 * outline_margin + m_font_padding.x >= m_font_atlas_size.width()) {
            y = bottom_y;
            x = 2 * outline_margin + m_font_padding.x; // advance to next row
        }
        if (y + glyph_height + outline_margin + m_font_padding.y
            >= m_font_atlas_size.height()) // check if it fits vertically AFTER potentially moving to next row
        {
            qDebug() << "Font doesnt fit into bitmap";
            assert(false);
            break; // doesnt fit in image
        }

        // clang-format off
        stbtt_MakeGlyphBitmap(&m_fontinfo, raster.data() + x + y * m_font_atlas_size.width(), glyph_width, glyph_height, m_font_atlas_size.width(), scale, scale, glyph_index);
        m_char_data.emplace(c, MapLabel::CharData {
                uint16_t(x - outline_margin),
                uint16_t(y - outline_margin),
                uint16_t(glyph_width + outline_margin * 2),
                uint16_t(glyph_height + outline_margin * 2),
                float(x0 - outline_margin),
                float(y0 - outline_margin) });
        // clang-format on

        x = x + glyph_width + 2 * outline_margin + m_font_padding.x;
        if (y + glyph_height + outline_margin + m_font_padding.y > bottom_y)
            bottom_y = y + glyph_height + 2 * outline_margin + m_font_padding.y;
    }

    return raster;
}

Raster<glm::u8vec2> MapLabelManager::make_outline(const Raster<uint8_t>& input)
{
    auto font_bitmap = Raster<glm::u8vec2>(input.size(), { 0, 0 });
    unsigned outline_margin = unsigned(std::ceil(m_font_outline));

    const auto aa_circle = [&](const glm::uvec2& centre, const glm::uvec2& px) {
        float distance = glm::distance(glm::vec2(centre), glm::vec2(px));
        float v = glm::smoothstep(m_font_outline - 1.5f, m_font_outline, distance);
        return uint8_t((1 - v) * 255);
    };

    for (unsigned y = 0; y < font_bitmap.height(); ++y) {
        for (unsigned x = 0; x < m_font_atlas_size.width(); ++x) {

            const uint8_t value = input.pixel({ x, y });

            font_bitmap.pixel({ x, y }).x = value;
            if (value < 120)
                continue;

            for (unsigned j = y - outline_margin; j < y + outline_margin; ++j) {
                if (j >= font_bitmap.height())
                    continue;
                for (unsigned i = x - outline_margin; i < x + outline_margin; ++i) {
                    if (i >= font_bitmap.width())
                        continue;
                    font_bitmap.pixel({ i, j }).y = std::max(aa_circle({ x, y }, { i, j }), font_bitmap.pixel({ i, j }).y);
                }
            }
        }
    }
    return font_bitmap;
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
const Raster<glm::u8vec2>& MapLabelManager::font_atlas() const { return m_font_atlas; }

} // namespace nucleus
