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

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QTimer>


namespace webgpu_app::clouds {
     struct DateComponents {
        int year;
        int month;
        int day;
        int hour;
    };

    enum class SlotStatus {
        Empty, // Exists in manifest, but haven't checked availability yet
        Pending, // Server is currently generating this (polling active)
        Ready, // Data exists and is valid
        Stale, // Data exists but is old (server is regenerating)
        Error // Request failed or out of window
    };

    enum class ManifestStatus {
        Pending,
        Ready,
        Error
    };

    inline std::string to_string(SlotStatus status) {
        switch (status) {
        case SlotStatus::Empty:   return "empty";
        case SlotStatus::Pending: return "pending";
        case SlotStatus::Ready:   return "ready";
        case SlotStatus::Stale:   return "stale";
        case SlotStatus::Error:   return "error";
        default:                  return "invalid status";
        }
    }

    struct TimeSlot {
        QString id; // "2026020412"
        DateComponents date;
        SlotStatus status = SlotStatus::Empty;

        // Populated when status is Ready/Stale
        QString run_id; // "2026020412"
        int run_hour;
        int step = 0;
        QString path; // "/2026020412_000/"

        struct Progress {
            QString stage;
            QString detail;
            int percent = 0;

            bool operator==(const Progress& other) const {
                return stage == other.stage && detail == other.detail && percent == other.percent;
            }
        } progress;

        [[nodiscard]] std::string format_string() const;
        [[nodiscard]] bool is_complete() const
        {
            return status == SlotStatus::Ready || status == SlotStatus::Stale;
        }
        [[nodiscard]] bool is_generation_requestable() const
        {
            return status == SlotStatus::Empty || status == SlotStatus::Stale || status == SlotStatus::Error;
        }
    };

    class APIService : public QObject {
        Q_OBJECT
    public:
        explicit APIService(QObject* parent = nullptr, int poll_interval_msec = 1000);

        // Populates the internal list of slots with "Unknown" status
        void refresh_manifest();

        [[nodiscard]] const QVector<TimeSlot>& get_slots() const;
        [[nodiscard]] const QHash<QString, int>& get_slots_map() const;
        [[nodiscard]] TimeSlot get_slot(const QString& id) const;

        // Call this when the user clicks a time in the UI.
        // If "Unknown", it queries server. If "Pending", it ensures polling is active.
        void request_generate_slot(const QString& timestamp_id);

        // Looks up the slot by ID. If Ready/Stale, fetches the file.
        void fetch_shadow_texture(const QString& timestamp_id);

    signals:
        // Fired when the list size/content changes significantly (full UI rebuild)
        void manifest_loaded(bool ok);

        // Fired when a specific slot changes status (e.g., Pending -> Ready)
        // UI should only redraw this specific row/item.
        void slot_updated(const TimeSlot& slot);

        // Fired when the actual binary data is downloaded
        void shadow_texture_loaded(const TimeSlot& slot, const QByteArray& data);

    private:
        // Internal helper to parse YYYYMMDDHH
        static DateComponents parse_timestamp_id(const QString& id) ;

        // Internal helper to process /request JSON response
        void handle_status_response(const QString& timestamp_id, const QByteArray& data);

        // Polling Logic
        void check_pending_items();

        QNetworkAccessManager* m_network_manager;
        QTimer* m_poll_timer;

        // Data Store
        QVector<TimeSlot> m_slots;
        QHash<QString, int> m_id_to_index; // Fast lookup for m_slots

        // Polling State
        QSet<QString> m_pending_ids; // IDs that we are currently polling

        const QString m_server_url = "http://127.0.0.1:8000";
    };

    class Manager : public QObject {
        Q_OBJECT
    public:
        explicit Manager(QObject* parent = nullptr);

        void select_time_slot(const TimeSlot& slot);
        void generate_selected_slot() const;
        void refresh_manifest();

        [[nodiscard]] TimeSlot selected_time_slot() const;

        [[nodiscard]] const QVector<TimeSlot>& get_slots() const;

        [[nodiscard]] ManifestStatus get_manifest_status() const;

    signals:
        void slot_ready(const TimeSlot& slot);
        void shadow_texture_ready(const QByteArray& data);

    private:
        std::unique_ptr<APIService> m_api_service;
        QString m_selected_cloud_slot_id = "";
        ManifestStatus m_manifest_status = ManifestStatus::Pending;
    };
} // namespace

