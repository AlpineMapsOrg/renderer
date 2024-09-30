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

#pragma once

#include "TimerFrontendObject.h"
#include <QList>
#include <QMap>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <nucleus/timing/TimerManager.h>

class TimerFrontendManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    TimerFrontendManager(QObject* parent = nullptr);

public:
    TimerFrontendManager(TimerFrontendManager const&) = delete;
    ~TimerFrontendManager() override;
    void operator=(TimerFrontendManager const&) = delete;

    static TimerFrontendManager* create(QQmlEngine*, QJSEngine*)
    {
        QJSEngine::setObjectOwnership(instance(), QJSEngine::CppOwnership);
        return instance();
    }
    static TimerFrontendManager* instance();

public slots:
    void receive_measurements(QList<nucleus::timing::TimerReport> values);
    // this is called by qml in the loader, triggering the initialisation. it's necessary, so that it's available in c++ via instance()
    void initialise() const;

signals:
    void updateTimingList(QList<TimerFrontendObject*> data);

private:
    QList<TimerFrontendObject*> m_timer;
    QMap<QString, TimerFrontendObject*> m_timer_map;
    int m_current_frame = 0;
};
