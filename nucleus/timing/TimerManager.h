/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2025 Adam Celarek
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

#include <memory>
#include <QList>
#include <vector>
#include <map>
#include <QDebug>

#ifdef QT_DEBUG
#include <set>
#endif

#include "TimerInterface.h"
#include <memory>

namespace nucleus::timing {

struct TimerReport {
    // value inside timer could be different by now, thats why we send a copy of the value
    float value;
    // the following could be a shared_ptr to an info object as well, but not a shared_ptr to TimerInterface as that might
    // contain objects that can be destroyed only inside the opengl/webgpu context
    // QString is implicitly shared, so a copy here is not that bad.
    QString name;
    QString group;
    int queue_size = 0;
    float average_weight = 0;
};

class TimerManager {
public:
    // adds the given timer
    std::shared_ptr<TimerInterface> add_timer(std::shared_ptr<TimerInterface> tmr);

    // Start timer with given name
    void start_timer(const QString& name);

    // Stops the currently running timer
    void stop_timer(const QString& name);

    // Fetches the results of all timers and returns the new values
    QList<TimerReport> fetch_results();

    TimerManager();

#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    ~TimerManager() {
        qDebug("nucleus::timing::~TimerManager()");
    }
#endif

private:
    // Contains the timer as list for the correct order
    std::vector<std::shared_ptr<TimerInterface>> m_timer_in_order;
    // Contains the timer as map for fast access by name
    std::map<QString, std::shared_ptr<TimerInterface>> m_timer;

#ifdef QT_DEBUG
    // Contains the timer name if a warning for this timer was already published
    std::set<QString> m_timer_already_warned_about;
    // Writes a warning out on the screen for the given timer (only once)
    void warn_about_timer(const QString& name);
#endif
};
}
