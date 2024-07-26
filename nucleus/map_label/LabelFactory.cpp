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

#include "LabelFactory.h"

#include <QDebug>
#include <QFile>
#include <QSize>
#include <QStringLiteral>

#include "nucleus/Raster.h"
#include "nucleus/utils/image_loader.h"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_slim/stb_truetype.h>

using namespace Qt::Literals::StringLiterals;

namespace nucleus::maplabel {

/**
 * this function needs to be called before create_labels
 */
const LabelMeta LabelFactory::create_label_meta()
{
    LabelMeta lm;

    lm.font_atlas = make_outline(make_font_raster());
    {
        Raster<glm::u8vec4> rgba_raster = { lm.font_atlas.size(), { 255, 255, 0, 255 } };
        std::transform(lm.font_atlas.cbegin(), lm.font_atlas.cend(), rgba_raster.begin(), [](const auto& v) { return glm::u8vec4(v.x, v.y, 0, 255); });

        // const auto debug_out = QImage(rgba_raster.bytes(), m_font_atlas_size.width(), m_font_atlas_size.height(), QImage::Format_RGBA8888);
        // debug_out.save("font_atlas.png");
    }

    // paint svg icon into the an image of appropriate size
    lm.icons[nucleus::vectortile::FeatureType::Peak] = nucleus::utils::image_loader::rgba8(":/map_icons/peak.png");
    // TODO add appropriate icon
    lm.icons[nucleus::vectortile::FeatureType::City] = nucleus::utils::image_loader::rgba8(":/map_icons/peak.png");

    // TODO add appropriate default icon
    lm.icons[nucleus::vectortile::FeatureType::ENUM_END] = nucleus::utils::image_loader::rgba8(":/map_icons/peak.png");

    return lm;
}

Raster<uint8_t> LabelFactory::make_font_raster()
{
    // load ttf file
    QFile file(":/fonts/Roboto/Roboto-Bold.ttf");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    Q_UNUSED(open);
    m_font_file = file.readAll();

    // init font and get info about the dimensions
    const auto font_init = stbtt_InitFont(&m_fontinfo, reinterpret_cast<const uint8_t*>(m_font_file.constData()),
        stbtt_GetFontOffsetForIndex(reinterpret_cast<const uint8_t*>(m_font_file.constData()), 0));
    assert(font_init);
    Q_UNUSED(font_init);

    auto raster = Raster<uint8_t>({ m_font_atlas_size.width(), m_font_atlas_size.height() }, uint8_t(0));

    float scale = stbtt_ScaleForPixelHeight(&m_fontinfo, font_size);

    int outline_margin = int(std::ceil(m_font_outline));
    int x = outline_margin + m_font_padding.x;
    int y = outline_margin + m_font_padding.y;
    int bottom_y = outline_margin + m_font_padding.y;

    for (const char16_t& c : all_char_list) {
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
        m_char_data.emplace(c, CharData {
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

Raster<glm::u8vec2> LabelFactory::make_outline(const Raster<uint8_t>& input)
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

const std::vector<VertexData> LabelFactory::create_labels(const std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>& features)
{
    std::vector<VertexData> labelData;

    for (const auto& feat : features) {
        // std::cout << "iii: " << feat->labelText().toStdString() << ": " << feat->worldposition.z << std::endl;

        create_label(feat->labelText(), feat->worldposition, 1.0, labelData);
    }

    return labelData;
}

void LabelFactory::create_label(const QString text, const glm::vec3 position, const float importance, std::vector<VertexData>& vertex_data)
{
    constexpr float offset_y = -font_size / 2.0f + 100.0f;
    constexpr float icon_offset_y = 15.0f;

    auto safe_chars = text.toStdU16String();
    float text_width = 0;
    std::vector<float> kerningOffsets = create_text_meta(&safe_chars, &text_width);

    // center the text around the center
    const auto offset_x = -text_width / 2.0f;

    // label icon
    vertex_data.push_back({ glm::vec4(-icon_size.x / 2.0f, icon_size.y / 2.0f + icon_offset_y, icon_size.x, -icon_size.y + 1), // vertex position + offset
        glm::vec4(10.0f, 10.0f, 1, 1), // uv position + offset
        position, importance });

    for (unsigned long long i = 0; i < safe_chars.size(); i++) {

        const CharData b = m_char_data.at(safe_chars[i]);

        vertex_data.push_back({ glm::vec4(offset_x + kerningOffsets[i] + b.xoff, offset_y - b.yoff, b.width, -b.height), // vertex position + offset
            glm::vec4(b.x * uv_width_norm, b.y * uv_width_norm, b.width * uv_width_norm, b.height * uv_width_norm), // uv position + offset
            position, importance });
    }
}

// calculate char offsets and text width
std::vector<float> inline LabelFactory::create_text_meta(std::u16string* safe_chars, float* text_width)
{
    std::vector<float> kerningOffsets;

    float scale = stbtt_ScaleForPixelHeight(&m_fontinfo, font_size);
    float xOffset = 0;
    for (unsigned long long i = 0; i < safe_chars->size(); i++) {
        if (!m_char_data.contains(safe_chars->at(i))) {
            qDebug() << "character with unicode index(Dec: " << safe_chars[i]
                     << ") cannot be shown -> please add it to nucleus/map_label/LabelFactory.h.all_char_list";
            safe_chars[i] = 32; // replace with space character
        }

        assert(m_char_data.contains(safe_chars->at(i)));

        int advance, lsb;
        stbtt_GetCodepointHMetrics(&m_fontinfo, int(safe_chars->at(i)), &advance, &lsb);

        kerningOffsets.push_back(xOffset);

        xOffset += float(advance) * scale;
        if (i + 1 < safe_chars->size())
            xOffset += scale * float(stbtt_GetCodepointKernAdvance(&m_fontinfo, int(safe_chars->at(i)), int(safe_chars->at(i + 1))));
    }
    kerningOffsets.push_back(xOffset);

    { // get width of last char
        if (!m_char_data.contains(safe_chars->back())) {
            qDebug() << "character with unicode index(Dec: " << safe_chars->back()
                     << ") cannot be shown -> please add it to nucleus/map_label/LabelFactory.h.all_char_list";
            safe_chars->back() = 32; // replace with space character
        }
        const CharData b = m_char_data.at(safe_chars->back());

        *text_width = xOffset + b.width;
    }

    return kerningOffsets;
}

} // namespace nucleus::maplabel
