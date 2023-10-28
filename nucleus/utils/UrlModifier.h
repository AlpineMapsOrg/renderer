/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Jakob Lindner
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

#include <QUrl>
#include <QUrlQuery>
#include <QMap>
#include <QString>

#define URL_PARAMETER_KEY_CONFIG "config"

//#define URLQUERY_DEFAULT "AAABEHjas__AAAb26PQFKP0AQu-H0c8WzEvAph6dfgCjBQQUsNEwdT8gtAMHAwpwcMBurkO9A4o6hD0MuAAjEobxFdDk4AAAEGIdhQ"
#define URLQUERY_DEFAULT ""
namespace nucleus::utils {

class UrlModifier {
public:
    static UrlModifier* get();
    static void init();

    static QString b64_to_urlsafe_b64(const QString& b64string);
    static QString urlsafe_b64_to_b64(const QString& urlsafeb64);

    UrlModifier(const UrlModifier&) = delete;
    UrlModifier& operator=(const UrlModifier&) = delete;

    QString get_query_item(const QString& name, bool* parameter_found = nullptr);
    void set_query_item(const QString& name, const QString& value);
    QUrl get_url();

protected:
    UrlModifier();

    static UrlModifier* singleton;

private:
    QUrl base_url = QUrl(QString("https://fancymaps.org/") + URLQUERY_DEFAULT);
    QMap<QString, QString> parameters;
};

}
