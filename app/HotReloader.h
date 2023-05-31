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

#pragma once

#include <QObject>

class QFileSystemWatcher;
class QQmlApplicationEngine;

class HotReloader : public QObject {
    Q_OBJECT
public:
    explicit HotReloader(QQmlApplicationEngine* engine, QString directory, QObject* parent = nullptr);

signals:
    void watched_source_changed();

public slots:
    void clear_cache();

private:
    QFileSystemWatcher* m_watcher;
    QQmlApplicationEngine* m_engine;
};
