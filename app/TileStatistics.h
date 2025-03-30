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

#pragma once

#include <QObject>
#include <QVariantMap>

class TileStatistics : public QObject {
    Q_OBJECT
public:
    explicit TileStatistics(QObject* parent = nullptr);
    Q_PROPERTY(QVariantMap scheduler READ scheduler_stats NOTIFY scheduler_stats_changed FINAL)
    Q_PROPERTY(QVariantMap gpu READ gpu_stats NOTIFY gpu_stats_changed FINAL)

    const QVariantMap& scheduler_stats() const;
    void set_scheduler_stats(const QVariantMap& new_scheduler_stats);

    [[nodiscard]] const QVariantMap& gpu_stats() const;

public slots:
    void set_gpu_stats(const QVariantMap& new_gpu_stats);
    void update_scheduler_stats(const QString& scheduler_name, const QVariantMap& stats);

signals:
    void scheduler_stats_changed(const QVariantMap& scheduler_stats);
    void gpu_stats_changed(const QVariantMap& gpu_stats);

private:
    QVariantMap m_scheduler_stats;
    QVariantMap m_gpu_stats;
};
