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
#include <QSize>
#include <QStringLiteral>

#include "nucleus/Raster.h"
#include "nucleus/map_label/Charset.h"
#include "nucleus/picker/PickerTypes.h"
#include "nucleus/utils/image_loader.h"
#include <nucleus/utils/bit_coding.h>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_slim/stb_truetype.h>

using namespace Qt::Literals::StringLiterals;

namespace nucleus::maplabel {

const AtlasData LabelFactory::init_font_atlas()
{
    nucleus::maplabel::Charset& c = nucleus::maplabel::Charset::get_instance();

    m_all_chars = c.get_all_chars();
    m_last_char_amount = m_all_chars.size();

    m_font_renderer.init();
    m_font_renderer.render(m_all_chars, font_size);

    m_font_data = m_font_renderer.get_font_data();

    return {true, m_font_renderer.get_font_atlas()};
}

const AtlasData LabelFactory::renew_font_atlas()
{
    nucleus::maplabel::Charset& c = nucleus::maplabel::Charset::get_instance();

    // check if any new chars have been added
    if(c.is_update_necessary(m_last_char_amount))
    {
        // get the new chars
        auto new_chars = c.get_char_diff(m_all_chars);
        // add them to the complete list
        m_all_chars.insert(new_chars.begin(), new_chars.end());
        m_last_char_amount = m_all_chars.size();

        m_font_renderer.render(new_chars, font_size);

        m_font_data = m_font_renderer.get_font_data();

        return {true, m_font_renderer.get_font_atlas()};
    }

    // nothing changed -> return empty
    AtlasData lm;

    lm.changed = false;
    return lm;
}

/**
 * this function needs to be called before create_labels
 */
const Raster<glm::u8vec4> LabelFactory::get_label_icons()
{
    auto icons = std::unordered_map<FeatureType, Raster<glm::u8vec4>>();

    icons[FeatureType::Peak] = nucleus::utils::image_loader::rgba8(":/map_icons/peak.png");
    icons[FeatureType::City] = nucleus::utils::image_loader::rgba8(":/map_icons/city.png");
    icons[FeatureType::Cottage] = nucleus::utils::image_loader::rgba8(":/map_icons/alpinehut.png");
    icons[FeatureType::Webcam] = nucleus::utils::image_loader::rgba8(":/map_icons/viewpoint.png");

    size_t combined_height(0);

    for (int i = 0; i < FeatureType::ENUM_END; i++) {
        FeatureType type = (FeatureType)i;
        combined_height += icons[type].height();
    }

    auto combined_icons = Raster<glm::u8vec4>({ icons[FeatureType::Peak].width(), 0 });

    for (int i = 0; i < FeatureType::ENUM_END; i++) {
        FeatureType type = (FeatureType)i;
        // vec4(10.0f,...) is an uv_offset to indicate that the icon texture should be used.
        icon_uvs[type]
            = glm::vec4(10.0f, 10.0f + float(combined_icons.height()) / float(combined_height), 1.0f, float(icons[type].height()) / float(combined_height));
        combined_icons.combine(icons[type]);
    }

    return combined_icons;
}

const std::vector<VertexData> LabelFactory::create_labels(const VectorTile& features)
{
    std::vector<VertexData> labelData;

    for (const auto& feat : features) {
        create_label(feat->labelText(), feat->worldposition, feat->type, feat->internal_id, feat->importance, labelData);
    }

    return labelData;
}

void LabelFactory::create_label(const QString text, const glm::vec3 position, const FeatureType type, const uint32_t internal_id, const float importance,
    std::vector<VertexData>& vertex_data)
{
    float text_offset_y = -font_size / 2.0f + 75.0f;
    float icon_offset_y = 15.0f;
    if (type == FeatureType::City) {
        text_offset_y += 25.0f;
        icon_offset_y += 25.0f; // buildings might obstruct label -> we want to set it a bit above the city
    }

    auto safe_chars = text.toStdU16String();
    float text_width = 0;
    std::vector<float> kerningOffsets = create_text_meta(&safe_chars, &text_width);

    // center the text around the center
    const auto offset_x = -text_width / 2.0f;

    glm::vec4 picker_color = nucleus::utils::bit_coding::u32_to_f8_4(internal_id);
    picker_color.x = (float(nucleus::picker::PickTypes::feature) / 255.0f); // set the first bit to the type

    // label icon
    vertex_data.push_back({ glm::vec4(-icon_size.x / 2.0f, icon_size.y / 2.0f + icon_offset_y, icon_size.x, -icon_size.y + 1), // vertex position + offset
        icon_uvs[type], // vec4 defined as uv position + offset
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

    float scale = stbtt_ScaleForPixelHeight(&m_font_data.fontinfo, font_size);
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
