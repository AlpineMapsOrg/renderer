 /*****************************************************************************
 * Alpine Renderer
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

#pragma once

#include <QUrl>
#include <QUrlQuery>
#include <QMap>
#include <QString>
#include <QTimer>
#include <QDateTime>
#include <glm/glm.hpp>

#define URL_PARAMETER_KEY_CONFIG "config"
#define URL_PARAMETER_KEY_CAM_POS "campos"
#define URL_PARAMETER_KEY_CAM_LOOKAT "lookat"
#define URL_PARAMETER_KEY_DATETIME "date"
#define URL_PARAMETER_KEY_QUALITY "quality"

//#define URLQUERY_DEFAULT "AAABEHjas__AAAb26PQFKP0AQu-H0c8WzEvAph6dfgCjBQQUsNEwdT8gtAMHAwpwcMBurkO9A4o6hD0MuAAjEobxFdDk4AAAEGIdhQ"
//#define URLQUERY_DEFAULT "?campos=48.207237_16.373009_337.51&lookat=48.212005_16.373096_-65.41" // stephansdom
//#define URLQUERY_DEFAULT "?date=2022-11-14T11%3A45%3A02"
//#define URLQUERY_DEFAULT "?campos=47.370311_11.452865_2679.02&config=AAABUHjas__AAAb26PTB1xMfgOizXN8WgOj7Z5sUgPT-V2c3KzAggD0u_Zf6gxpA9AMGAQYsNFz_DwjtwMGAAhwcUM2DiycdQOFjyNtBaREo3Q8xyCGTAYWGu_MBKo0FMKLRMLYClEaRBwDpcSV9&date=2023-08-08T14%3A15%3A00&lookat=47.376271_11.452865_2543.19"
//#define URLQUERY_DEFAULT "?config=AAABEHjas__AAAb26PQFBgEw_bp_D5h-YD37AJDeP3dDzAIGBLDHpf8BVP8j4UkKYFr7E5i-q3pXAVndDwjtwAHl38hKBtnj4IBqHgw4JB1A4QPtQaGxAEY0GsZWgNIo8gCq2CLk"
#define URLQUERY_DEFAULT ""
#define URL_WRITEOUT_DELAY 100
namespace nucleus::utils {

class UrlModifier : public QObject {
    Q_OBJECT

public:

    static QString b64_to_urlsafe_b64(const QString& b64string);
    static QString urlsafe_b64_to_b64(const QString& urlsafeb64);

    static QString dvec3_to_urlsafe_string(const glm::dvec3& vec, int precision = 10);
    static glm::dvec3 urlsafe_string_to_dvec3(const QString& str);

    static QString latlonalt_to_urlsafe_string(const glm::dvec3& vec);

    static QString qdatetime_to_urlsafe_string(const QDateTime& date);
    static QDateTime urlsafe_string_to_qdatetime(const QString& str);


    UrlModifier(QObject* parent = nullptr);
    ~UrlModifier();

    QString get_query_item(const QString& name, bool* parameter_found = nullptr);
    void set_query_item(const QString& name, const QString& value);
    QUrl get_url();

private slots:
    void write_out_url();

private:
    QUrl base_url = QUrl(QString("https://fancymaps.org/") + URLQUERY_DEFAULT);
    QMap<QString, QString> parameters;
    // We don't want to writeout the parameters every single time they change,
    // because that might be every frame. So lets use a timer to delay that.
    QTimer write_out_delay_timer;
};

}
