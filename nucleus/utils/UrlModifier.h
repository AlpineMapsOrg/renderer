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
#include <QTimer>
#include <glm/glm.hpp>

#define URL_PARAMETER_KEY_CONFIG "config"
#define URL_PARAMETER_KEY_CAM_POS "campos"
#define URL_PARAMETER_KEY_CAM_LOOKAT "lookat"

//#define URLQUERY_DEFAULT "AAABEHjas__AAAb26PQFKP0AQu-H0c8WzEvAph6dfgCjBQQUsNEwdT8gtAMHAwpwcMBurkO9A4o6hD0MuAAjEobxFdDk4AAAEGIdhQ"
//#define URLQUERY_DEFAULT "?campos=48.207237_16.373009_337.51&lookat=48.212005_16.373096_-65.41" // stephansdom
#define URLQUERY_DEFAULT ""
#define URL_WRITEOUT_DELAY 100
namespace nucleus::utils {

class UrlModifier : public QObject {
    Q_OBJECT

public:
    static UrlModifier* get();
    static void init();

    static QString b64_to_urlsafe_b64(const QString& b64string);
    static QString urlsafe_b64_to_b64(const QString& urlsafeb64);

    static QString dvec3_to_urlsafe_string(const glm::dvec3& vec, int precision = 10);
    static glm::dvec3 urlsafe_string_to_dvec3(const QString& str);

    static QString latlonalt_to_urlsafe_string(const glm::dvec3& vec);

    UrlModifier(const UrlModifier&) = delete;
    UrlModifier& operator=(const UrlModifier&) = delete;

    QString get_query_item(const QString& name, bool* parameter_found = nullptr);
    void set_query_item(const QString& name, const QString& value);
    QUrl get_url();

protected:
    UrlModifier();

    static UrlModifier* singleton;

signals:
    void start_writeout_delay(int duration);

private slots:
    // gets called by write_out_delay_timer
    void start_writeout_delay_slot(int duration);
    void write_out_url();

private:
    QUrl base_url = QUrl(QString("https://fancymaps.org/") + URLQUERY_DEFAULT);
    QMap<QString, QString> parameters;
    // We don't want to writeout the parameters every single time they change,
    // because that might be every frame. So lets use a timer to delay that.
    QTimer write_out_delay_timer;
};

}
