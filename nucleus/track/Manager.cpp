/*****************************************************************************
 * Alpine Terrain Renderer
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

#include "Manager.h"

using namespace nucleus::track;

Manager::Manager(QObject* parent)
    : QObject { parent }
{
}

std::vector<nucleus::gpx::Gpx> Manager::tracks() const
{
    std::vector<nucleus::gpx::Gpx> v;
    v.reserve(m_data.size());
    std::transform(m_data.cbegin(), m_data.cend(), std::back_inserter(v), [](const auto& d) { return d.second; });
    return v;
}

nucleus::gpx::Gpx Manager::track(Id id) const
{
    if (!m_data.contains(id))
        return {};
    return m_data.at(id);
}

void Manager::add_or_replace(Id id, const nucleus::gpx::Gpx& gpx)
{
    m_data[id] = gpx;
    QVector<nucleus::gpx::Gpx> v;
    v.reserve(m_data.size());
    std::transform(m_data.cbegin(), m_data.cend(), std::back_inserter(v), [](const auto& d) { return d.second; });
    emit tracks_changed(v);
}

void Manager::remove(Id id)
{
    m_data.erase(id);
    QVector<nucleus::gpx::Gpx> v;
    v.reserve(m_data.size());
    std::transform(m_data.cbegin(), m_data.cend(), std::back_inserter(v), [](const auto& d) { return d.second; });
    emit tracks_changed(v);
}

unsigned int Manager::size() const { return m_data.size(); }
