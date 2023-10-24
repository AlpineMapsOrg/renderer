/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>

class FileDownloader : public QObject {
    Q_OBJECT

public:
    FileDownloader(const QUrl &url, QObject *parent = nullptr) : QObject(parent) {
        manager = new QNetworkAccessManager(this);
        connect(manager, &QNetworkAccessManager::finished, this, &FileDownloader::onDownloadFinished);

        // Start the download
        manager->get(QNetworkRequest(url));
    }

private slots:
    void onDownloadFinished(QNetworkReply *reply) {
        if (reply->error() == QNetworkReply::NoError) {
            content = reply->readAll();
            qDebug() << "Downloaded content:" << content;
            // Here you can further process the downloaded content or pass it to another function
        } else {
            qWarning() << "Download error:" << reply->errorString();
        }
        reply->deleteLater();

        // For the sake of this example, we'll exit the event loop once the download is done.
        QCoreApplication::quit();
    }

private:
    QNetworkAccessManager *manager;
    QString content;
};
