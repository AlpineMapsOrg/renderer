/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2025 Joerg-Christian Reiher
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

#include "UIntIdManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <extern/tl_expected/include/tl/expected.hpp>
#include <iostream>

namespace nucleus::avalanche {
UIntIdManager::UIntIdManager(const QDate& reference_date)
    : m_reference_date(reference_date)
{
    // intern_id = 0 means "no region"
    m_region_id_to_internal_id[QString("")] = 0;
    m_internal_id_to_region_id[0] = QString("");
    assert(m_max_internal_id == 0);
}

uint UIntIdManager::convert_region_id_to_internal_id(const QString& region_id)
{
    // If Key exists return its value, else add it and return its newly created value
    auto entry = m_region_id_to_internal_id.find(region_id);
    if (entry == m_region_id_to_internal_id.end()) {
        m_region_id_to_internal_id[region_id] = ++m_max_internal_id;
        return m_max_internal_id;
    } else
        return entry->second;
}

QString UIntIdManager::convert_internal_id_to_region_id(const uint& internal_id) const
{
    auto entry = m_internal_id_to_region_id.find(internal_id);
    if (entry == m_internal_id_to_region_id.end())
        return QString("");
    else
        return entry->second;
}

QColor UIntIdManager::convert_region_id_to_color(const QString& region_id)
{
    const uint& internal_id = this->convert_region_id_to_internal_id(region_id);
    uint red = internal_id / 256;
    uint green = internal_id % 256;
    return QColor::fromRgb(red, green, 0);
}

QString UIntIdManager::convert_color_to_region_id(const QColor& color) const
{
    uint internal_id = color.red() * 256 + color.green();
    auto entry = m_internal_id_to_region_id.find(internal_id);
    if (entry == m_internal_id_to_region_id.end())
        return QString("");
    return m_internal_id_to_region_id.at(internal_id);
}

uint UIntIdManager::convert_color_to_internal_id(const QColor& color) const
{
    QString region_id = this->convert_color_to_region_id(color);
    auto entry = m_region_id_to_internal_id.find(region_id);
    if (entry == m_region_id_to_internal_id.end())
        return 0;
    else
        return entry->second;
}

std::vector<QString> UIntIdManager::get_all_registered_region_ids() const
{
    std::vector<QString> region_ids(m_internal_id_to_region_id.size());
    for (const auto& [internal_id, region_id] : m_internal_id_to_region_id)
        region_ids[internal_id] = region_id;
    return region_ids;
}

bool UIntIdManager::contains(const QString& region_id) const { return m_region_id_to_internal_id.contains(region_id); }

} // namespace nucleus::avalanche
