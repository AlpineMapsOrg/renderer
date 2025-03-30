/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adam Celarek
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

#include "TileStatistics.h"

#include <QVariant>

TileStatistics::TileStatistics(QObject* parent)
    : QObject { parent }
{
}

void TileStatistics::update_scheduler_stats(const QString& scheduler_name, const QVariantMap& new_stats)
{
    auto current = scheduler_stats();
    for (const auto& [key, value] : new_stats.asKeyValueRange()) {
        current[QString("%1_%2").arg(scheduler_name, key)] = value.toString();
    }
    set_scheduler_stats(current);
}

const QVariantMap& TileStatistics::gpu_stats() const { return m_gpu_stats; }

void TileStatistics::set_gpu_stats(const QVariantMap& new_gpu_stats)
{
    if (m_gpu_stats == new_gpu_stats)
        return;
    m_gpu_stats = new_gpu_stats;
    emit gpu_stats_changed(m_gpu_stats);
}

const QVariantMap& TileStatistics::scheduler_stats() const { return m_scheduler_stats; }

void TileStatistics::set_scheduler_stats(const QVariantMap& new_scheduler_stats)
{
    if (m_scheduler_stats == new_scheduler_stats)
        return;
    m_scheduler_stats = new_scheduler_stats;
    emit scheduler_stats_changed(m_scheduler_stats);
}
