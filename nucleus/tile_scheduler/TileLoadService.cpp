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

#include "TileLoadService.h"

#include <QDebug>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QtVersionChecks>

#include "../srs.h"

using namespace nucleus::tile_scheduler;

TileLoadService::TileLoadService(const QString& base_url, UrlPattern url_pattern, const QString& file_ending, const LoadBalancingTargets& load_balancing_targets)
    : m_network_manager(new QNetworkAccessManager(this))
    , m_base_url(base_url)
    , m_url_pattern(url_pattern)
    , m_file_ending(file_ending)
    , m_load_balancing_targets(load_balancing_targets)
{
}

TileLoadService::~TileLoadService() = default;

void TileLoadService::load(const tile::Id& tile_id)
{
    QNetworkRequest request(QUrl(build_tile_url(tile_id)));
    request.setTransferTimeout(int(m_transfer_timeout));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    request.setAttribute(QNetworkRequest::UseCredentialsAttribute, false);
#endif

    QNetworkReply* reply = m_network_manager->get(request);
    connect(reply, &QNetworkReply::finished, [tile_id, reply, this]() {
        const auto error = reply->error();
        const auto timestamp = utils::time_since_epoch();
        if (error == QNetworkReply::NoError) {
            auto tile = std::make_shared<QByteArray>(reply->readAll());
            emit load_finished({tile_id, {tile_types::NetworkInfo::Status::Good, timestamp}, tile});
        } else if (error == QNetworkReply::ContentNotFoundError) {
            auto tile = std::make_shared<QByteArray>();
            emit load_finished({tile_id, {tile_types::NetworkInfo::Status::NotFound, timestamp}, tile});
        } else {
            //            qDebug() << reply->url() << ": " << error;
            auto tile = std::make_shared<QByteArray>();
            emit load_finished({tile_id, {tile_types::NetworkInfo::Status::NetworkError, timestamp}, tile});
        }
        reply->deleteLater();
    });
}

QString TileLoadService::build_tile_url(const tile::Id& tile_id) const
{
    QString tile_address;
    const auto n_y_tiles = srs::number_of_vertical_tiles_for_zoom_level(tile_id.zoom_level);
    switch (m_url_pattern) {
    case UrlPattern::ZXY:
        tile_address = QString("%1/%2/%3").arg(tile_id.zoom_level).arg(tile_id.coords.x).arg(tile_id.coords.y);
        break;
    case UrlPattern::ZYX:
        tile_address = QString("%1/%3/%2").arg(tile_id.zoom_level).arg(tile_id.coords.x).arg(tile_id.coords.y);
        break;
    case UrlPattern::ZXY_yPointingSouth:
        tile_address = QString("%1/%2/%3").arg(tile_id.zoom_level).arg(tile_id.coords.x).arg(n_y_tiles - tile_id.coords.y - 1);
        break;
    case UrlPattern::ZYX_yPointingSouth:
        tile_address = QString("%1/%3/%2").arg(tile_id.zoom_level).arg(tile_id.coords.x).arg(n_y_tiles - tile_id.coords.y - 1);
        break;
    }
    if (!m_load_balancing_targets.empty()) {
        const unsigned hash = qHash(tile_address) % 1024;
        const auto index = unsigned((float(hash) / 1024.1f) * float(m_load_balancing_targets.size()));
        assert(index < m_load_balancing_targets.size());
        return m_base_url.arg(m_load_balancing_targets[index]) + tile_address + m_file_ending;
    }
    return m_base_url + tile_address + m_file_ending;
}

unsigned int TileLoadService::transfer_timeout() const
{
    return m_transfer_timeout;
}

void TileLoadService::set_transfer_timeout(unsigned int new_transfer_timeout)
{
    assert(new_transfer_timeout < unsigned(std::numeric_limits<int>::max()));
    m_transfer_timeout = new_transfer_timeout;
}
