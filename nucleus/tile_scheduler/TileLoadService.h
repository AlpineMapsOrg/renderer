/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <memory>

#include <QObject>

#include "constants.h"
#include "tile_types.h"

class QNetworkAccessManager;

namespace nucleus::tile_scheduler {

class TileLoadService : public QObject {
    Q_OBJECT
public:
    enum class UrlPattern {
        ZXY,
        ZYX, // y=0 is southern most tile
        ZXY_yPointingSouth,
        ZYX_yPointingSouth // y=0 is the northern most tile
    };
    using LoadBalancingTargets = std::vector<QString>;

    TileLoadService(const QString& base_url, UrlPattern url_pattern, const QString& file_ending, const LoadBalancingTargets& load_balancing_targets = {});
    ~TileLoadService() override;
    [[nodiscard]] QString build_tile_url(tile::Id tile_id) const;

    [[nodiscard]] unsigned int transfer_timeout() const;
    void set_transfer_timeout(unsigned int new_transfer_timeout);

public slots:
    void load(const tile::Id& tile_id) const;

signals:
    void load_finished(tile_types::TileLayer tile) const;

private:
    unsigned m_transfer_timeout = tile_scheduler::constants::default_network_timeout;
    std::shared_ptr<QNetworkAccessManager> m_network_manager;
    QString m_base_url;
    UrlPattern m_url_pattern;
    QString m_file_ending;
    LoadBalancingTargets m_load_balancing_targets;
};
}
