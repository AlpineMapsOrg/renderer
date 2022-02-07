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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QImage>


TileLoadService::TileLoadService(const QString& base_url, UrlPattern url_pattern, const QString& file_ending)
    : m_network_manager(new QNetworkAccessManager()),
      m_base_url(base_url),
      m_url_pattern(url_pattern),
      m_file_ending(file_ending)
{

}

TileLoadService::~TileLoadService()
{
}

void TileLoadService::load(const srs::TileId& tile_id)
{
  QNetworkReply* reply = m_network_manager->get(QNetworkRequest(QUrl(build_tile_url(tile_id))));
  connect(reply, &QNetworkReply::finished, [tile_id, reply, this]() {
    auto tile = std::make_shared<QImage>(QImage::fromData(reply->readAll()));
    tile->save("/home/madam/Documents/work/tuw/alpinemaps/build-alpine-renderer-Desktop_Qt_6_2_3_GCC_64bit-Debug/tls.jpeg");
    emit loadReady(tile_id, std::move(tile));
    reply->deleteLater();
  });
}

QString TileLoadService::build_tile_url(const srs::TileId& tile_id) const
{
  QString tile_address;
  switch (m_url_pattern) {
  case UrlPattern::ZXY:
    tile_address = QString("%1/%2/%3").arg(tile_id.zoom_level).arg(tile_id.coords.x).arg(tile_id.coords.y);
    break;
  case UrlPattern::ZYX:
    tile_address = QString("%1/%2/%3").arg(tile_id.zoom_level).arg(tile_id.coords.y).arg(tile_id.coords.x);
    break;
  }
  return m_base_url + tile_address + m_file_ending;
}
