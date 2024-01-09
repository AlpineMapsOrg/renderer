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

QString UrlModifier::dvec3_to_urlsafe_string(const glm::dvec3& vec, int precision) {
    return QString("%1_%2_%3").arg(QString::number(vec.x, 'f', precision), QString::number(vec.y, 'f', precision), QString::number(vec.z, 'f', precision));
}

QString UrlModifier::latlonalt_to_urlsafe_string(const glm::dvec3& vec) {
    return QString("%1_%2_%3").arg(QString::number(vec.x, 'f', 6), QString::number(vec.y, 'f', 6), QString::number(vec.z, 'f', 2));
}

glm::dvec3 UrlModifier::urlsafe_string_to_dvec3(const QString& str) {
    QStringList parts = str.split('_');

    // Make sure we have exactly 3 parts, corresponding to x, y, and z
    if (parts.size() != 3) {
        throw std::invalid_argument("The input string does not represent a glm::dvec3");
    }

    bool okX, okY, okZ;
    double x = parts[0].toDouble(&okX);
    double y = parts[1].toDouble(&okY);
    double z = parts[2].toDouble(&okZ);

    // Check if all conversions were successful
    if (!okX || !okY || !okZ) {
        throw std::invalid_argument("One of the components is not a valid double value");
    }

    return glm::dvec3(x, y, z);
}

QString UrlModifier::qdatetime_to_urlsafe_string(const QDateTime& date) {
    auto str = date.toString(Qt::DateFormat::ISODate);
    return QUrl::toPercentEncoding(str);
}

QDateTime UrlModifier::urlsafe_string_to_qdatetime(const QString& str) {
    auto decstr = QUrl::fromPercentEncoding(str.toUtf8());
    return QDateTime::fromString(decstr, Qt::DateFormat::ISODate);
}


UrlModifier::UrlModifier(QObject* parent)
    :QObject(parent)
{
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("nucleus::utils::UrlModifier()");
#endif
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

    write_out_delay_timer.setSingleShot(true);
    connect(&write_out_delay_timer, &QTimer::timeout, this, &UrlModifier::write_out_url);
}

UrlModifier::~UrlModifier() {
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("~nucleus::utils::UrlModifier()");
#endif
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
    // Note: If you get the error that the timer can't be started from
    // a different thread, PLS think about if its absolutely necessary to
    // make this class thread safe. If so the easiest solution would prob.
    // be to make a signal/slot for set_query_item. But then again: Is it
    // necessary? Maybe you can resolve the thread issue on a higher level.
    write_out_delay_timer.start(URL_WRITEOUT_DELAY);
    //write_out_url();
}

QUrl UrlModifier::get_url() {
    QUrlQuery newQuery;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        newQuery.addQueryItem(it.key(), it.value());
    }
    base_url.setQuery(newQuery);
    return QUrl(base_url);
}

void UrlModifier::write_out_url() {
#ifdef __EMSCRIPTEN__
    auto resID = get_resource_identifier_from_url(get_url());
    EM_ASM({
        var newPath = UTF8ToString($0);
        history.pushState({}, "", newPath);
    },
        resID.toStdString().c_str());
#else
    auto resID = get_resource_identifier_from_url(get_url());
    qDebug() << "url:" << resID;
#endif
}

}
