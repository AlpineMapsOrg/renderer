/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Adam Celarek
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
#include <QSize>
#include <QStringLiteral>

#include "nucleus/Raster.h"
#include "nucleus/picker/types.h"
#include "nucleus/utils/image_loader.h"
#include <nucleus/utils/bit_coding.h>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_slim/stb_truetype.h>

using namespace Qt::Literals::StringLiterals;

namespace nucleus::maplabel {

AtlasData LabelFactory::init_font_atlas()
{
    m_font_renderer.init();
    for (const auto ch : uR"( !"#$%&'()*+,-./0123456789:;<=>@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~§°´ÄÖÜßáâäéìíóöúüýČčěňőřŠšŽž€)") {
        m_new_chars.emplace(ch);
    }
    return renew_font_atlas();
}

AtlasData LabelFactory::renew_font_atlas()
{
    // check if any new chars have been added
    if (!m_new_chars.empty()) {
        m_font_renderer.render(m_new_chars, m_font_size);
        m_rendered_chars.insert(m_new_chars.begin(), m_new_chars.end());
        m_new_chars.clear();
        m_font_data = m_font_renderer.font_data();
        return {true, m_font_renderer.font_atlas()};
    }

    // nothing changed -> return empty
    AtlasData lm;

    lm.changed = false;
    return lm;
}

/**
 * this function needs to be called before create_labels
 */
Raster<glm::u8vec4> LabelFactory::label_icons()
{
    using PoiType = vector_tile::PointOfInterest::Type;
    auto icons = std::unordered_map<PoiType, Raster<glm::u8vec4>>();
    icons[PoiType::Peak] = nucleus::utils::image_loader::rgba8(":/map_icons/peak.png");
    icons[PoiType::Settlement] = nucleus::utils::image_loader::rgba8(":/map_icons/city.png");
    icons[PoiType::AlpineHut] = nucleus::utils::image_loader::rgba8(":/map_icons/alpinehut.png");
    icons[PoiType::Webcam] = nucleus::utils::image_loader::rgba8(":/map_icons/camera.png");
    icons[PoiType::Unknown] = nucleus::Raster<glm::u8vec4>(icons[PoiType::Peak].size(), glm::u8vec4(255, 0, 128, 255));

    size_t combined_height(0);

    for (int i = 0; i < int(PoiType::NumberOfElements); i++) {
        PoiType type = PoiType(i);
        combined_height += icons[type].height();
    }

    auto combined_icons = Raster<glm::u8vec4>({ icons[PoiType::Peak].width(), 0 });

    for (int i = 0; i < int(PoiType::NumberOfElements); i++) {
        PoiType type = PoiType(i);
        // vec4(10.0f,...) is an uv_offset to indicate that the icon texture should be used.
        m_icon_uvs[type]
            = glm::vec4(10.0f, 10.0f + float(combined_icons.height()) / float(combined_height), 1.0f, float(icons[type].height()) / float(combined_height));
        combined_icons.combine(icons[type]);
    }

    return combined_icons;
}

std::vector<VertexData> LabelFactory::create_labels(const vector_tile::PointOfInterestCollection& pois)
{
    std::vector<VertexData> labelData;

    for (const auto& f : pois) {
        for (const auto ch : f.name) {
            if (m_rendered_chars.contains(ch.unicode()))
                continue;
            m_new_chars.insert(ch.unicode());
        }
    }

    for (const auto& p : pois) {
        QString display_name = p.name;
        float importance = p.importance;
        switch (p.type) {
        case LabelType::Peak: {
            auto ele = p.attributes.value("ele");
            if (ele.size() == 0)
                ele = QString::number(p.lat_long_alt.z, 'f', 0);
            display_name = QString("%1 (%2m)").arg(p.name, ele);
            break;
        }
        case LabelType::AlpineHut:
        case LabelType::Webcam:
            display_name = "";
            importance = 0.3;
            break;
        default:
            break;
        }
        create_label(display_name, p.world_space_pos, p.type, p.id, importance, labelData);
    }

    return labelData;
}

void LabelFactory::create_label(
    const QString& text, const glm::vec3& position, LabelType type, const uint32_t id, const float importance, std::vector<VertexData>& vertex_data)
{
    float text_offset_y = -m_font_size / 2.0f + 60.0f;
    float icon_offset_y = 0.0f;

    auto safe_chars = text.toStdU16String();
    float text_width = 0;
    std::vector<float> kerningOffsets = create_text_meta(&safe_chars, &text_width);

    // center the text around the center
    const auto offset_x = -text_width / 2.0f;

    glm::vec4 picker_color = nucleus::utils::bit_coding::u32_to_f8_4(id);
    picker_color.x = (float(nucleus::picker::FeatureType::PointOfInterest) / 255.0f); // set the first bit to the type

    // label icon
    vertex_data.push_back({ glm::vec4(-m_icon_size.x / 2.0f, m_icon_size.y / 2.0f + icon_offset_y, m_icon_size.x, -m_icon_size.y + 1), // vertex position + offset
        m_icon_uvs[type], // vec4 defined as uv position + offset
        picker_color, position, importance, 0 });

    for (unsigned long long i = 0; i < safe_chars.size(); i++) {

        const CharData b = m_font_data.char_data.at(safe_chars[i]);

        vertex_data.push_back({ glm::vec4(offset_x + kerningOffsets[i] + b.xoff, text_offset_y - b.yoff, b.width, -b.height), // vertex position + offset
            glm::vec4(b.x * m_font_data.uv_width_norm, b.y * m_font_data.uv_width_norm, b.width * m_font_data.uv_width_norm,
                b.height * m_font_data.uv_width_norm), // uv position + offset
            picker_color, position, importance, b.texture_index });
    }
}

// calculate char offsets and text width
std::vector<float> inline LabelFactory::create_text_meta(std::u16string* safe_chars, float* text_width)
{
    // case no text in label
    if (safe_chars->size() == 0)
        return std::vector<float>();

    std::vector<float> kerningOffsets;
    
    float scale = stbtt_ScaleForPixelHeight(&m_font_data.fontinfo, m_font_size);
    float xOffset = 0;
    for (unsigned long long i = 0; i < safe_chars->size(); i++) {
        if (!m_font_data.char_data.contains(safe_chars->at(i))) {
            safe_chars->at(i) = 32;
        }

        assert(m_font_data.char_data.contains(safe_chars->at(i)));

        int advance, lsb;
        stbtt_GetCodepointHMetrics(&m_font_data.fontinfo, int(safe_chars->at(i)), &advance, &lsb);

        kerningOffsets.push_back(xOffset);

        xOffset += float(advance) * scale;
        if (i + 1 < safe_chars->size())
            xOffset += scale * float(stbtt_GetCodepointKernAdvance(&m_font_data.fontinfo, int(safe_chars->at(i)), int(safe_chars->at(i + 1))));
    }
    kerningOffsets.push_back(xOffset);

    { // get width of last char
        if (!m_font_data.char_data.contains(safe_chars->back())) {
            safe_chars->back() = 32; // replace with space character
        }
        const CharData b = m_font_data.char_data.at(safe_chars->back());

        *text_width = xOffset + b.width;
    }

    return kerningOffsets;
}

} // namespace nucleus::maplabel
