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

#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVector>

namespace webgpu_app::clouds {
struct DateComponents {
    int year;
    int month;
    int day;
    int hour;
};

struct TileSetInfo {
    QString id; // "2026040812" (YYYYMMDDHH, target hour)
    DateComponents date;
    QString folder; // "2026040809_003" (on-disk folder name)
    int step = 0; // extracted from folder suffix (_SSS)
    qint64 size = 0; // bytes

    [[nodiscard]] std::string format_string() const;
};

class APIService : public QObject {
    Q_OBJECT
public:
    explicit APIService(QObject* parent = nullptr);

    void refresh_tileset_list();

    [[nodiscard]] const QVector<TileSetInfo>& get_slots() const;
    [[nodiscard]] const QHash<QString, int>& get_slots_map() const;
    [[nodiscard]] TileSetInfo get_slot(const QString& id) const;

    [[nodiscard]] const QString& server_url() const { return m_server_url; }

    void fetch_shadow_texture(const QString& id);

signals:
    // fired once after refresh_tileset_list() completes; ok=false on network/parse error
    void tileset_list_loaded(bool ok);
    // fired when the shadow.ktx2 binary for slot has been fully downloaded
    void shadow_texture_loaded(const TileSetInfo& slot, const QByteArray& data);

private:
    static DateComponents parse_timestamp_id(const QString& id);

    QNetworkAccessManager* m_network_manager;

    // ordered list of ready tile sets (ascending target time)
    QVector<TileSetInfo> m_slots;
    // maps TileSetInfo::id -> index into m_slots
    QHash<QString, int> m_id_to_index;

    const QString m_server_url = "https://atlas.cg.tuwien.ac.at/webigeo-clouds/v2"; // http://localhost:8000/v2, https://atlas.cg.tuwien.ac.at/webigeo-clouds/v2
};

class Manager : public QObject {
    Q_OBJECT
public:
    explicit Manager(QObject* parent = nullptr);

    void select_time_slot(const TileSetInfo& slot);
    void refresh_tileset_list();

    [[nodiscard]] TileSetInfo selected_time_slot() const;
    [[nodiscard]] const QVector<TileSetInfo>& get_slots() const;
    [[nodiscard]] bool is_loading() const { return m_loading; }
    [[nodiscard]] const QString& server_url() const;

signals:
    void slot_ready(const TileSetInfo& slot);
    void shadow_texture_ready(const QByteArray& data);

private:
    std::unique_ptr<APIService> m_api_service;
    QString m_selected_slot_id = "";
    bool m_loading = true;
};
} // namespace webgpu_app::clouds
