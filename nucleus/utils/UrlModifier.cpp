/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "UrlModifier.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/val.h>
#include <emscripten.h>
#endif

#include <string>
#include <QDebug>

namespace nucleus::utils {

UrlModifier* UrlModifier::singleton = nullptr;

UrlModifier* UrlModifier::get() {
    assert(singleton);
    return singleton;
}

QString UrlModifier::b64_to_urlsafe_b64(const QString& b64string) {
    QString urlSafe = b64string;
    urlSafe.replace('+', '-');
    urlSafe.replace('/', '_');
    urlSafe.remove('=');
    return urlSafe;
}

QString UrlModifier::urlsafe_b64_to_b64(const QString& urlsafeb64) {
    QString base64 = urlsafeb64;
    base64.replace('-', '+');
    base64.replace('_', '/');

    // Add padding '=' characters if necessary to make the length a multiple of 4
    while (base64.length() % 4 != 0) {
        base64.append('=');
    }

    return base64;
}

void UrlModifier::init() {
    singleton = new UrlModifier();
}


UrlModifier::UrlModifier() {
#ifdef __EMSCRIPTEN__
    emscripten::val location = emscripten::val::global("location");
    auto href = location["href"].as<std::string>();
    base_url = QUrl(href.c_str());
#endif
    // Get paramter
    auto query = QUrlQuery(base_url.query(QUrl::PrettyDecoded));

    parameters.clear();
    for (auto& pair : query.queryItems()){
        parameters[pair.first] = pair.second;
    }
}

QString UrlModifier::get_query_item(const QString& name, bool* parameter_found) {
    if (parameters.contains(name)) {
        if (parameter_found) *parameter_found = true;
        return parameters.value(name);
    }
    return "";
}

QString get_resource_identifier_from_url(const QUrl& url) {
    // Get the last part of the path
    QString lastPartOfPath = url.path(QUrl::PrettyDecoded).split("/").last();
    QString queryParams = url.query(QUrl::PrettyDecoded);
    QString result;
    if (!queryParams.isEmpty()) result = lastPartOfPath + "?" + queryParams;
    else result = lastPartOfPath;
    return result;
}

void UrlModifier::set_query_item(const QString& name, const QString& value) {
    parameters[name] = value;

#ifdef __EMSCRIPTEN__
    auto resID = get_resource_identifier_from_url(get_url());
    EM_ASM_({
        var newPath = UTF8ToString($0);
        history.pushState({}, "", newPath);
    }, resID.toStdString().c_str());
#else
    //auto resID = get_resource_identifier_from_url(get_url());
    //qDebug() << "url:" << resID;
#endif
}

QUrl UrlModifier::get_url() {
    QUrlQuery newQuery;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        newQuery.addQueryItem(it.key(), it.value());
    }
    base_url.setQuery(newQuery);
    return QUrl(base_url);
}


}
