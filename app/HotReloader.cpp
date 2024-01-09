/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "HotReloader.h"

#include <QFileSystemWatcher>
#include <QQmlApplicationEngine>

HotReloader::HotReloader(QQmlApplicationEngine* engine, QString directory, QObject* parent)
    : QObject { parent }
    , m_engine(engine)
{
    m_watcher = new QFileSystemWatcher(this);
    directory.replace("file:/", "");
    m_watcher->addPath(directory);
    qDebug("watching %s", directory.toStdString().c_str());

    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString&) {
        qDebug("watched_source_changed");
        emit watched_source_changed();
    });
}

void HotReloader::clear_cache()
{
    m_engine->clearComponentCache();
}
