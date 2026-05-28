/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Patrick Komon
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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <string>

namespace webgpu_app {

struct SearchResult {
    std::string name;

    // TODO use classes that already exist in nucleus if there is any?
    double longitude;
    double latitude;
};

class SearchService : public QObject {
    Q_OBJECT

public:
    SearchService();

public slots:
    void search(const std::string& search_term);

signals:
    void search_results_arrived(const std::vector<webgpu_app::SearchResult>& results);

private slots:
    void http_reply_received(QNetworkReply* reply);

private:
    std::unique_ptr<QNetworkAccessManager> m_network_manager;
    size_t m_limit = 15;
};

} // namespace webgpu_app
