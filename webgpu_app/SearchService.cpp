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

#include "SearchService.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

namespace webgpu_app {

SearchService::SearchService()
    : m_network_manager(std::make_unique<QNetworkAccessManager>(this))
{
    connect(m_network_manager.get(), &QNetworkAccessManager::finished, this, &SearchService::http_reply_received);
}

void SearchService::search(const std::string& search_term)
{
    const std::string format = "geojson";

    auto url = QUrl("https://nominatim.openstreetmap.org/search");
    const auto url_query = QUrlQuery({
        { "q", QString::fromStdString(search_term) },
        { "limit", QString::number(m_limit) },
        { "format", QString::fromStdString(format) },
    });
    url.setQuery(url_query);

    auto request = QNetworkRequest(url);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, "webigeo/1.0");
    m_network_manager->get(request);
}

void SearchService::http_reply_received(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // TODO maybe emit some signal on error?
        qWarning() << "network request returned error " << reply->error() << ", request content: " << reply->readAll();
        return;
    }

    QJsonParseError parse_error {};

    const auto responseContent = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseContent, &parse_error);
    if (doc.isNull()) {
        // TODO maybe emit some signal on error?
        qWarning() << "json parsing returned error at offset " << parse_error.offset << ": " << parse_error.errorString()
                   << ", response content: " << responseContent;
        return;
    }

    // TODO refactor: move geojson parsing/validation elsewhere

    if (!doc.isObject()) {
        // TODO maybe emit some signal on error?
        qWarning() << "error: expected json object, got " << responseContent;
        return;
    }
    const auto jsonObject = doc.object();
    const auto features = jsonObject.value("features");
    if (features.isUndefined() || !features.isArray()) {
        qWarning() << "error: expected key \"features\" to be present and contain an array, got" << responseContent;
        return;
    }

    auto features_array = features.toArray();
    std::vector<SearchResult> results;
    results.reserve(features_array.size());
    for (auto it = features_array.begin(); it != features_array.end(); it++) {
        // TODO validate
        const auto feature = it->toObject();
        const auto properties = feature.value("properties").toObject();
        const QString display_name = properties.value("display_name").toString();
        const auto geometry = feature.value("geometry").toObject();
        if (geometry.value("type").toString() != "Point") {
            continue;
        }
        const double longitude = geometry.value("coordinates").toArray().begin()->toDouble();
        const double latitude = (geometry.value("coordinates").toArray().begin() + 1)->toDouble();
        results.emplace_back(display_name.toStdString(), longitude, latitude);
    }

    for (auto it = results.begin(); it != results.end(); it++) {
        qInfo() << it->name << it->longitude << it->latitude;
    }

    emit search_results_arrived(results);
}

} // namespace webgpu_app
