/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Wendelin Muth
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

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QDebug>

namespace webgpu_app::clouds {

std::string TimeSlot::format_string() const
{
    if (id.isEmpty()) return "invalid";
    return std::format("{:04d}.{:02d}.{:02d} {:02d}+{:02d}", date.year, date.month, date.day, run_hour, step);
}

APIService::APIService(QObject* parent, int poll_interval_msec)
    : QObject(parent)
    , m_network_manager(new QNetworkAccessManager(this))
    , m_poll_timer(new QTimer(this))
{
    m_poll_timer->setInterval(poll_interval_msec);
    m_poll_timer->setSingleShot(false);
    connect(m_poll_timer, &QTimer::timeout, this, &APIService::check_pending_items);
}

const QVector<TimeSlot>& APIService::get_slots() const
{
    return m_slots;
}

const QHash<QString, int>& APIService::get_slots_map() const
{
    return m_id_to_index;
}

TimeSlot APIService::get_slot(const QString& id) const
{
    if (!m_id_to_index.contains(id)) return {};
    return m_slots[m_id_to_index[id]];
}

DateComponents APIService::parse_timestamp_id(const QString& id)
{
    // ID Format: YYYYMMDDHH (10 chars)
    if (id.length() != 10) return {0, 0, 0, 0};
    return {
        id.mid(0, 4).toInt(),
        id.mid(4, 2).toInt(),
        id.mid(6, 2).toInt(),
        id.mid(8, 2).toInt()
    };
}

void APIService::refresh_manifest()
{
    qDebug() << "[CloudAPI] Refreshing manifest...";
    QNetworkRequest request(QUrl(m_server_url + "/available"));
    QNetworkReply* reply = m_network_manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[CloudAPI] Failed to fetch manifest:" << reply->errorString();
            emit manifest_loaded(false);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;

        m_slots.clear();
        m_id_to_index.clear();
        m_pending_ids.clear();
        m_poll_timer->stop();

        QJsonArray items = doc.object()["items"].toArray();

        for (const auto& val : items) {
            QJsonObject item = val.toObject();
            QString id = item["id"].toString();

            if (id.length() != 10) continue;

            TimeSlot slot;
            slot.id = id;
            slot.date = parse_timestamp_id(id);

            // Map Status String to Enum
            QString status_str = item["status"].toString();
            if (status_str == "ready") slot.status = SlotStatus::Ready;
            else if (status_str == "stale") slot.status = SlotStatus::Stale;
            else if (status_str == "pending") slot.status = SlotStatus::Pending;
            else slot.status = SlotStatus::Empty;

            // Populate Metadata
            slot.run_id = item["run"].toString();
            slot.run_hour = slot.run_id.mid(8,2).toInt();
            slot.step = item["step"].toInt();
            if (slot.status == SlotStatus::Ready || slot.status == SlotStatus::Stale) {
                slot.path = item["path"].toString();
            }

            // Automatic Polling: If manifest says it's pending, track it.
            if (slot.status == SlotStatus::Pending) {
                m_pending_ids.insert(id);
            }

            m_id_to_index[id] = m_slots.size();
            m_slots.push_back(slot);
        }

        // Restart timer if we found pending items
        if (!m_pending_ids.isEmpty()) {
            m_poll_timer->start();
        }

        emit manifest_loaded(true);
    });
}

void APIService::request_generate_slot(const QString& timestamp_id)
{
    if (!m_id_to_index.contains(timestamp_id)) {
        qWarning() << "[CloudAPI] Invalid ID selected:" << timestamp_id;
        return;
    }

    auto& slot = m_slots[m_id_to_index[timestamp_id]];
    if (!slot.is_generation_requestable()) {
        qDebug() << "[CloudAPI] Slot generation requested but status" << to_string(slot.status) << "does not allow it:" << timestamp_id;
        return;
    }

    slot.status = SlotStatus::Pending;
    slot.progress = {};
    emit slot_updated(slot);

    QUrl url(m_server_url + "/generate");
    QUrlQuery query;
    query.addQueryItem("time", timestamp_id);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply* reply = m_network_manager->post(request, QByteArray());

    connect(reply, &QNetworkReply::finished, this, [this, reply, timestamp_id]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[CloudAPI] Generation request error for" << timestamp_id << ":" << reply->errorString();
            qDebug() << reply->readAll().toStdString();

            // Update UI to Error state
            if (m_id_to_index.contains(timestamp_id)) {
                auto slot = m_slots[m_id_to_index[timestamp_id]];
                slot.status = SlotStatus::Error;
                emit slot_updated(slot);
            }
            return;
        }
        handle_status_response(timestamp_id, reply->readAll());
    });
}

void APIService::handle_status_response(const QString& timestamp_id, const QByteArray& data)
{
    if (!m_id_to_index.contains(timestamp_id)) {
        qWarning() << "[CloudAPI] handle_status_response received data for an unknown ID:" << timestamp_id;
        return;
    }

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[CloudAPI] JSON parse error for" << timestamp_id << ":" << parse_error.errorString();
        // Set to error state and notify UI
        TimeSlot& slot = m_slots[m_id_to_index[timestamp_id]];
        if (slot.status != SlotStatus::Error) {
            slot.status = SlotStatus::Error;
            emit slot_updated(slot);
        }
        return;
    }
    QJsonObject obj = doc.object();

    // Extract data
    TimeSlot& slot = m_slots[m_id_to_index[timestamp_id]];
    const TimeSlot original_slot = slot; // Keep a copy for change detection

    QString status_str = obj["status"].toString();
    SlotStatus new_status;

    // Map server string to client enum
    if (status_str == "ready") new_status = SlotStatus::Ready;
    else if (status_str == "stale") new_status = SlotStatus::Stale;
    else if (status_str == "pending") new_status = SlotStatus::Pending;
    else new_status = SlotStatus::Error; // Treat "unknown" or other errors as Error

    // Update slot metadata based on status
    slot.status = new_status;

    if (new_status == SlotStatus::Ready || new_status == SlotStatus::Stale) {
        slot.path = obj["path"].toString();
        slot.run_id = obj["run"].toString();
        slot.step = obj["step"].toInt();
        slot.run_hour = slot.run_id.mid(8,2).toInt();
    }
    else if (new_status == SlotStatus::Pending) {
        if (obj.contains("progress") && obj["progress"].isObject()) {
            QJsonObject progress_obj = obj["progress"].toObject();
            slot.progress.stage = progress_obj["stage"].toString();
            slot.progress.detail = progress_obj["detail"].toString();
            slot.progress.percent = progress_obj["percent"].toInt();
        }
        // Also update the expected run/step, which is available even in pending state
        slot.run_id = obj["run"].toString();
        slot.step = obj["step"].toInt();
        slot.run_hour = slot.run_id.mid(8,2).toInt();

    } else {
        // For Error or Unknown status, clear potentially stale data
        slot.path.clear();
        slot.run_id.clear();
        slot.step = 0;
    }

    // Manage polling state
    if (new_status == SlotStatus::Pending) {
        m_pending_ids.insert(timestamp_id);
    } else {
        m_pending_ids.remove(timestamp_id);
    }

    // Manage the global timer's state
    if (m_pending_ids.isEmpty()) {
        m_poll_timer->stop();
    } else if (!m_poll_timer->isActive()) {
        m_poll_timer->start();
    }

    // Emit update signal
    bool has_changed = (original_slot.status != slot.status) ||
                       (original_slot.path != slot.path) ||
                       (original_slot.run_id != slot.run_id) ||
                       !(original_slot.progress == slot.progress);

    if (has_changed) {
        emit slot_updated(slot);
    }
}

void APIService::check_pending_items()
{
    if (m_pending_ids.isEmpty()) {
        m_poll_timer->stop();
        return;
    }

    // Copy the set because select_or_refresh_slot might modify the set in the reply handler
    // (though reply is async, so it's technically safe, but good practice).
    QSet<QString> current_pending = m_pending_ids;

    for (const QString& id : current_pending) {
        QUrl url(m_server_url + "/status");
        QUrlQuery query;
        query.addQueryItem("time", id);
        url.setQuery(query);

        QNetworkRequest request(url);
        QNetworkReply* reply = m_network_manager->get(request);

        // The handler remains the same, as the JSON format is identical.
        connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                handle_status_response(id, reply->readAll());
            } else {
                 qWarning() << "[CloudAPI] Polling status failed for" << id << ":" << reply->errorString();
            }
        });
    }
}

void APIService::fetch_shadow_texture(const QString& timestamp_id)
{
    if (!m_id_to_index.contains(timestamp_id)) {
        qWarning() << "[CloudAPI] fetch_shadow_texture called with unknown ID:" << timestamp_id;
        return;
    }

    const TimeSlot& slot = m_slots[m_id_to_index[timestamp_id]];

    if (slot.status != SlotStatus::Ready && slot.status != SlotStatus::Stale) {
        qWarning() << "[CloudAPI] Attempted to fetch texture for non-ready slot:" << timestamp_id;
        return;
    }

    if (slot.path.isEmpty()) {
        qWarning() << "[CloudAPI] Slot URL is empty for:" << timestamp_id;
        return;
    }

    QString full_url = m_server_url + slot.path + "shadow.ktx2";
    QNetworkRequest request(full_url);
    QNetworkReply* reply = m_network_manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, &slot]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[CloudAPI] Shadow texture download failed:" << reply->errorString();
            return;
        }
        emit shadow_texture_loaded(slot, reply->readAll());
    });
}

Manager::Manager(QObject* parent) : QObject(parent), m_api_service(std::make_unique<APIService>())
{

    connect(m_api_service.get(), &APIService::shadow_texture_loaded, this, [this](const TimeSlot& slot, const QByteArray& data) {
        if (m_selected_cloud_slot_id == slot.id && slot.status == SlotStatus::Ready) {
            emit shadow_texture_ready(data);
        }
    });

    connect(m_api_service.get(), &APIService::manifest_loaded, this, [this](bool ok) {
        if (!ok) {
            m_manifest_status = ManifestStatus::Error;
            return;
        }
        m_manifest_status = ManifestStatus::Ready;

        const auto& time_slots = m_api_service->get_slots();
        auto current_date = QDateTime::currentDateTimeUtc();
        auto ymd = QCalendar().partsFromDate(current_date.date());
        int hour = current_date.time().hour();
        for (size_t i = 0; i < time_slots.length(); i++) {
            auto& slot = time_slots[i];
            if (slot.date.year == ymd.year && slot.date.month == ymd.month && slot.date.day == ymd.day && slot.date.hour == hour) {
                select_time_slot(slot);
                break;
            }
        }
    });

    connect(m_api_service.get(), &APIService::slot_updated, this, [this](const TimeSlot& slot) {
        if (m_selected_cloud_slot_id == slot.id && slot.is_complete()) {
            m_api_service->fetch_shadow_texture(slot.id);
            emit slot_ready(slot);
        }
    });

    m_api_service->refresh_manifest();
}

void Manager::select_time_slot(const TimeSlot& slot)
{
    if (m_selected_cloud_slot_id == slot.id) {
        return;
    }
    m_selected_cloud_slot_id = slot.id;

    if (slot.is_complete()) {
        m_api_service->fetch_shadow_texture(slot.id);
        emit slot_ready(slot);
    }
}

void Manager::generate_selected_slot() const
{
    if (m_selected_cloud_slot_id.isEmpty()) {
        return;
    }
    m_api_service->request_generate_slot(m_selected_cloud_slot_id);
}

void Manager::refresh_manifest()
{
    m_manifest_status = ManifestStatus::Pending;
    m_api_service->refresh_manifest();
}

TimeSlot Manager::selected_time_slot() const { return m_api_service->get_slot(m_selected_cloud_slot_id); }

const QVector<TimeSlot>& Manager::get_slots() const { return m_api_service->get_slots(); }
ManifestStatus Manager::get_manifest_status() const { return m_manifest_status; }

} // namespace webgpu_app