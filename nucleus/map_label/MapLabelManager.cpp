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

#include <QDirIterator>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QSize>
#include <QSvgRenderer>
#include <QThread>
#include <QTimer>
#include <glm/gtx/matrix_decompose.hpp>

#include <iostream>
#include <string>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_slim/stb_truetype.h"

namespace nucleus {

MapLabelManager::MapLabelManager()
{
    //    const char8_t text;
    //    double latitude;
    //    double longitude;
    //    float altitude;
    //    float importance;
    m_labels.push_back({ "Großglockner", 47.07455, 12.69388, 3798, 14 });
    m_labels.push_back({ "Piz Buin", 46.84412, 10.11889, 3312, 10 });
    m_labels.push_back({ "Hoher Dachstein", 47.47519, 13.60569, 2995, 10 });
    m_labels.push_back({ "Großer Priel", 47.71694, 14.06325, 2515, 10 });
    m_labels.push_back({ "Hermannskogel", 48.27072, 16.29456, 544, 10 });
    m_labels.push_back({ "Klosterwappen", 47.76706, 15.80450, 2076, 10 });
    m_labels.push_back({ "Ötscher", 47.86186, 15.20251, 1893, 10 });
    m_labels.push_back({ "Ellmauer Halt", 47.5616377, 12.3025296, 2342, 10 });
    m_labels.push_back({ "Wildspitze", 46.88524, 10.86728, 3768, 10 });
    m_labels.push_back({ "Großvenediger", 47.10927, 12.34534, 3657, 10 });
    m_labels.push_back({ "Hochalmspitze", 47.01533, 13.32050, 3360, 10 });
    m_labels.push_back({ "Geschriebenstein", 47.35283, 16.43372, 884, 10 });

    m_labels.push_back({ "Ackerlspitze", 47.559125, 12.347188, 2329, 8 });
    m_labels.push_back({ "Scheffauer", 47.5573214, 12.2418396, 2111, 8 });
    m_labels.push_back({ "Maukspitze", 47.5588954, 12.3563668, 2231, 8 });
    m_labels.push_back({ "Schönfeldspitze", 47.45831, 12.93774, 2653, 8 });
    m_labels.push_back({ "Hochschwab", 47.61824, 15.14245, 2277, 8 });

    m_labels.push_back({ "Valluga", 47.15757, 10.21309, 2811, 6 });
    m_labels.push_back({ "Birkkarspitze", 47.41129, 11.43765, 2749, 6 });
    m_labels.push_back({ "Schafberg", 47.77639, 13.43389, 1783, 6 });

    m_labels.push_back({ "Grubenkarspitze", 47.38078, 11.52211, 2663, 6 });
    m_labels.push_back({ "Gimpel", 47.50127, 10.61249, 2176, 6 });
    m_labels.push_back({ "Seekarlspitze", 47.45723, 11.77804, 2261, 6 });
    m_labels.push_back({ "Furgler", 47.04033, 10.51186, 3004, 6 });

    m_labels.push_back({ "Westliche Hochgrubachspitze", 47.5583658, 12.3433997, 2277, 5 });
    m_labels.push_back({ "Östliche Hochgrubachspitze", 47.5587933, 12.3450985, 2284, 5 });
}

void MapLabelManager::init()
{
    m_indices.push_back(0);
    m_indices.push_back(1);
    m_indices.push_back(2);

    m_indices.push_back(0);
    m_indices.push_back(2);
    m_indices.push_back(3);

    createFont();

    for (auto& label : m_labels) {
        label.init(m_character_data, &m_fontinfo, 32, 255);
    }

    // paint svg icon into the an image of appropriate size
    m_icon = QIcon(QString(":/qt/qml/app/icons/peak.svg")).pixmap(QSize(MapLabel::icon_size.x, MapLabel::icon_size.y)).toImage();
}

void MapLabelManager::createFont()
{
    QFile file(":/fonts/SourceSans3-Medium.ttf");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    const QByteArray data = file.readAll();

    const auto font_init = stbtt_InitFont(&m_fontinfo, reinterpret_cast<const uint8_t*>(data.constData()), stbtt_GetFontOffsetForIndex(reinterpret_cast<const uint8_t*>(data.constData()), 0));
    assert(font_init);

    m_font_bitmap = new uint8_t[512 * 512];
    // renders 223 ascii characters (characters 32-255) into temp_bitmap -> should include all commonly used german characters
    // additionally stores font info (coordinates + size) in m_character_data
    stbtt_BakeFontBitmap(reinterpret_cast<const uint8_t*>(data.constData()), 0, MapLabel::font_size, m_font_bitmap, 512, 512, 32, 223, m_character_data);
}

const uint8_t* MapLabelManager::font_bitmap() const
{
    return m_font_bitmap;
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

} // namespace nucleus
