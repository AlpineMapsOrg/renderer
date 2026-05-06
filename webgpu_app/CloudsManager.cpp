/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Wendelin Muth
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "CloudsManager.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace webgpu_app::clouds {

std::string TileSetInfo::format_string() const
{
    if (id.isEmpty())
        return "invalid";
    double mib = size / (1024.0 * 1024.0);
    return std::format("{:02d}.{:02d}.{:04d} {:02d}:00 (+{:02d}, {:.0f} MiB)", date.day, date.month, date.year, date.hour, step, mib);
}

APIService::APIService(QObject* parent)
    : QObject(parent)
    , m_network_manager(new QNetworkAccessManager(this))
{
}

const QVector<TileSetInfo>& APIService::get_slots() const { return m_slots; }

const QHash<QString, int>& APIService::get_slots_map() const { return m_id_to_index; }

TileSetInfo APIService::get_slot(const QString& id) const
{
    if (!m_id_to_index.contains(id))
        return {};
    return m_slots[m_id_to_index[id]];
}

DateComponents APIService::parse_timestamp_id(const QString& id)
{
    // ID Format: YYYYMMDDHH (10 chars)
    if (id.length() != 10)
        return { 0, 0, 0, 0 };
    return { id.mid(0, 4).toInt(), id.mid(4, 2).toInt(), id.mid(6, 2).toInt(), id.mid(8, 2).toInt() };
}

void APIService::refresh_tileset_list()
{
    qDebug() << "[CloudAPI] Fetching tileset list...";
    QNetworkRequest request(QUrl(m_server_url + "/tilesets?status=ready"));
    QNetworkReply* reply = m_network_manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[CloudAPI] Failed to fetch tilesets:" << reply->errorString();
            emit tileset_list_loaded(false);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit tileset_list_loaded(false);
            return;
        }

        m_slots.clear();
        m_id_to_index.clear();

        QJsonArray items = doc.object()["items"].toArray();
        for (const auto& val : items) {
            QJsonObject item = val.toObject();
            QString id = item["id"].toString();
            if (id.length() != 10)
                continue;

            TileSetInfo slot;
            slot.id = id;
            slot.date = parse_timestamp_id(id);
            slot.folder = item["folder"].toString();
            slot.size = item["size"].toVariant().toLongLong();

            int underscore = slot.folder.lastIndexOf('_');
            if (underscore >= 0)
                slot.step = slot.folder.mid(underscore + 1).toInt();

            m_id_to_index[id] = m_slots.size();
            m_slots.push_back(slot);
        }

        qDebug() << "[CloudAPI] Loaded" << m_slots.size() << "ready tilesets.";
        emit tileset_list_loaded(true);
    });
}

void APIService::fetch_shadow_texture(const QString& id)
{
    if (!m_id_to_index.contains(id)) {
        qWarning() << "[CloudAPI] fetch_shadow_texture called with unknown ID:" << id;
        return;
    }

    TileSetInfo slot = m_slots[m_id_to_index[id]]; // copy to avoid dangling ref in lambda

    if (slot.folder.isEmpty()) {
        qWarning() << "[CloudAPI] Slot folder is empty for:" << id;
        return;
    }

    qDebug() << "[CloudAPI] Fetching shadow texture for" << id << "from" << slot.folder;
    QString full_url = m_server_url + "/" + slot.folder + "/shadow.ktx2";
    QNetworkRequest request(full_url);
    QNetworkReply* reply = m_network_manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, slot]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[CloudAPI] Shadow texture download failed:" << reply->errorString();
            return;
        }
        emit shadow_texture_loaded(slot, reply->readAll());
    });
}

Manager::Manager(QObject* parent)
    : QObject(parent)
    , m_api_service(std::make_unique<APIService>())
{
    connect(m_api_service.get(), &APIService::shadow_texture_loaded, this, [this](const TileSetInfo& slot, const QByteArray& data) {
        if (m_selected_slot_id == slot.id) {
            emit shadow_texture_ready(data);
        }
    });

    connect(m_api_service.get(), &APIService::tileset_list_loaded, this, [this](bool ok) {
        m_loading = false;
        if (!ok)
            return;

        const auto& tilesets = m_api_service->get_slots();
        auto current_date = QDateTime::currentDateTimeUtc();
        auto ymd = QCalendar().partsFromDate(current_date.date());
        int hour = current_date.time().hour();
        for (const auto& slot : tilesets) {
            if (slot.date.year == ymd.year && slot.date.month == ymd.month && slot.date.day == ymd.day && slot.date.hour == hour) {
                select_time_slot(slot);
                break;
            }
        }
    });

    m_api_service->refresh_tileset_list();
}

void Manager::select_time_slot(const TileSetInfo& slot)
{
    if (m_selected_slot_id == slot.id)
        return;
    m_selected_slot_id = slot.id;
    m_api_service->fetch_shadow_texture(slot.id);
    emit slot_ready(slot);
}

void Manager::refresh_tileset_list()
{
    m_loading = true;
    m_api_service->refresh_tileset_list();
}

TileSetInfo Manager::selected_time_slot() const { return m_api_service->get_slot(m_selected_slot_id); }

const QVector<TileSetInfo>& Manager::get_slots() const { return m_api_service->get_slots(); }

const QString& Manager::server_url() const { return m_api_service->server_url(); }

} // namespace webgpu_app::clouds
