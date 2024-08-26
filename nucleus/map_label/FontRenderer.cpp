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


#include "FontRenderer.h"

#include <QFile>
#include <QDebug>
#include <vector>
#include <QString>

namespace nucleus::maplabel {


void FontRenderer::init()
{
    // load ttf file
    QFile file(":/fonts/Roboto/Roboto-Bold.ttf");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    Q_UNUSED(open);
    m_font_file = file.readAll();

           // init font and get info about the dimensions
    const auto font_init = stbtt_InitFont(&m_font_data.fontinfo, reinterpret_cast<const uint8_t*>(m_font_file.constData()),
        stbtt_GetFontOffsetForIndex(reinterpret_cast<const uint8_t*>(m_font_file.constData()), 0));
    assert(font_init);
    Q_UNUSED(font_init);

    m_outline_margin = int(std::ceil(m_font_outline));
    m_x = m_outline_margin + m_font_padding.x;
    m_y = m_outline_margin + m_font_padding.y;
    m_bottom_y = m_outline_margin + m_font_padding.y;

    m_font_data.uv_width_norm = m_uv_width_norm;

    m_texture_index = 0;

    m_font_atlas.push_back(Raster<glm::u8vec2>({ m_font_atlas_size.width(), m_font_atlas_size.height() }, glm::u8vec2(0)));
}

void FontRenderer::render(std::set<char16_t> chars, float font_size)
{
    render_text(chars,font_size);
    make_outline(chars);


//    {
//        // debug output
//        static int counter = 1;
//        for(size_t i = 0; i < m_font_atlas.size(); i++)
//        {
//            Raster<glm::u8vec4> rgba_raster = { m_font_atlas[i].size(), { 255, 255, 0, 255 } };
//            std::transform(m_font_atlas[i].cbegin(), m_font_atlas[i].cend(), rgba_raster.begin(), [](const auto& v) { return glm::u8vec4(v.x, v.y, 0, 255); });
//            const auto debug_out = QImage(rgba_raster.bytes(), font_atlas_size.width(), font_atlas_size.height(), QImage::Format_RGBA8888);
//            debug_out.save(QString("font_atlas_%1_%2.png").arg(i).arg(counter));
//        }
//        counter++;
//    }

}

void FontRenderer::render_text(std::set<char16_t> chars, float font_size)
{
    float scale = stbtt_ScaleForPixelHeight(&m_font_data.fontinfo, font_size);

    // stb_truetype only supports one dimensional bitmap
    // we therefore have to create a 1d temp_raster that is later merged with the actual texture
    std::vector<Raster<uint8_t>> temp_raster = std::vector<Raster<uint8_t>>();
    int temp_texture_index = 0;
    temp_raster.push_back(Raster<uint8_t>({ m_font_atlas_size.width(), m_font_atlas_size.height() }, uint8_t(0)));

    for (const char16_t& c : chars) {
        // code adapted from stbtt_BakeFontBitmap()
        int x0, y0, x1, y1;
        const int glyph_index = stbtt_FindGlyphIndex(&m_font_data.fontinfo, c);
        stbtt_GetGlyphBitmapBox(&m_font_data.fontinfo, glyph_index, scale, scale, &x0, &y0, &x1, &y1);

        const auto glyph_width = x1 - x0;
        const auto glyph_height = y1 - y0;
        if (m_x + glyph_width + 2 * m_outline_margin + m_font_padding.x >= m_font_atlas_size.width()) {
            m_y = m_bottom_y;
            m_x = 2 * m_outline_margin + m_font_padding.x; // advance to next row
        }
        if (m_y + glyph_height + m_outline_margin + m_font_padding.y
            >= m_font_atlas_size.height()) // check if it fits vertically AFTER potentially moving to next row
        {
            // char doesnt fit on the current texture -> create a new texture and switch to this
            m_texture_index++;
            if (m_texture_index >= m_max_textures) {
                // !! there are too many chars !!
                // Ways to solve this (roughly from easy to difficult):
                // - increase the texture size
                // - reduce font size/outline size
                // - increase the the limit of the texture array (here and in labels.frag shader)
                //      - please note that some devices might not support this
                // - reduce the chars that are rendered
                // - make a bunch of texture arrays and bind the right array for the right reasons
                //      - e.g. if you created a bold version or a font with different font size create a separate draw-call with the separate texture array bound
                //      - note this requires a bit of refactoring
                qDebug() << "Font doesnt fit into bitmap";
                assert(false);
                break; // doesnt fit in image´
            }

            temp_texture_index++;
            temp_raster.push_back(Raster<uint8_t>({ m_font_atlas_size.width(), m_font_atlas_size.height() }, uint8_t(0)));
            m_font_atlas.push_back(Raster<glm::u8vec2>({ m_font_atlas_size.width(), m_font_atlas_size.height() }, glm::u8vec2(0)));

            m_y = m_outline_margin + m_font_padding.y;
            m_bottom_y = m_outline_margin + m_font_padding.y;
        }

        // clang-format off
        stbtt_MakeGlyphBitmap(&m_font_data.fontinfo, temp_raster[temp_texture_index].data() + m_x + m_y * m_font_atlas_size.width(), glyph_width, glyph_height, m_font_atlas_size.width(), scale, scale, glyph_index);
        m_font_data.char_data.emplace(c, CharData {
                                             uint16_t(m_x - m_outline_margin),
                                             uint16_t(m_y - m_outline_margin),
                                             uint16_t(glyph_width + m_outline_margin * 2),
                                             uint16_t(glyph_height + m_outline_margin * 2),
                                             float(x0 - m_outline_margin),
                                             float(y0 - m_outline_margin),
                                             m_texture_index });
        // clang-format on

        m_x = m_x + glyph_width + 2 * m_outline_margin + m_font_padding.x;
        if (m_y + glyph_height + m_outline_margin + m_font_padding.y > m_bottom_y)
            m_bottom_y = m_y + glyph_height + 2 * m_outline_margin + m_font_padding.y;
    }

    // merge temp_raster with font_atlas
    for(size_t i = 0; i < temp_raster.size(); i++)
    {
        // main index -> index of m_font_atlas
        const auto main_index = m_font_atlas.size()-temp_raster.size()+i;
        std::transform(m_font_atlas[main_index].cbegin(), m_font_atlas[main_index].cend(), temp_raster[i].cbegin(), m_font_atlas[main_index].begin(), [](const auto& v1,const auto& v2) { return glm::u8vec2(std::max(v1.x, v2), v1.y); });
    }

}

void FontRenderer::make_outline(std::set<char16_t> chars)
{
    unsigned outline_margin = unsigned(std::ceil(m_font_outline));

    const auto aa_circle = [&](const glm::uvec2& centre, const glm::uvec2& px) {
        float distance = glm::distance(glm::vec2(centre), glm::vec2(px));
        float v = glm::smoothstep(m_font_outline - 1.5f, m_font_outline, distance);
        return uint8_t((1 - v) * 255);
    };

    for(auto& c : chars)
    {
        auto char_data = m_font_data.char_data.at(c);

        // only visit the area the current char is located at
        for (uint16_t y = char_data.y; y < char_data.y+char_data.height; ++y) {
            for (uint16_t x = char_data.x; x < char_data.x+char_data.width; ++x) {

                if (m_font_atlas[char_data.texture_index].pixel({ x, y }).x < 120)
                    continue;

                // we found a pixel where the char is -> visit a block around this pixel and fill the outline using aa_circle
                for (uint16_t j = y - outline_margin; j < y + outline_margin; ++j) {
                    if (j >= m_font_atlas[char_data.texture_index].height())
                        continue;
                    for (uint16_t i = x - outline_margin; i < x + outline_margin; ++i) {
                        if (i >= m_font_atlas[char_data.texture_index].width())
                            continue;
                        m_font_atlas[char_data.texture_index].pixel({ i, j }).y = std::max(aa_circle({ x, y }, { i, j }), m_font_atlas[char_data.texture_index].pixel({ i, j }).y);
                    }
                }
            }
        }
    }
}

std::vector<Raster<glm::u8vec2>> FontRenderer::font_atlas() { return m_font_atlas; }

const FontData& FontRenderer::font_data() { return m_font_data; }
} // namespace nucleus::maplabel
