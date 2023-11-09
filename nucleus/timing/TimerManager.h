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

#include <string>
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
#include <mutex>

namespace nucleus::timing {

struct TimerReport {
    // value inside timer could be different by now, thats why we send a copy of the value
    float value;
    // Note: Somehow I need the timer definition in the front-end, like name, group, queue_size.
    // I don't want to send all of those values every time, so I'll just send it as shared pointer.
    // Might not be as fast as sending just a pointer, but otherwise it's possible to have a
    // memory issue at deconstruction time of the app.
    std::shared_ptr<TimerInterface> timer;
};

class TimerManager
{

public:
    // adds the given timer
    std::shared_ptr<TimerInterface> add_timer(std::shared_ptr<TimerInterface> tmr);

    // Start timer with given name
    void start_timer(const std::string &name);

    // Stops the currently running timer
    void stop_timer(const std::string &name);

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
    std::map<std::string, std::shared_ptr<TimerInterface>> m_timer;

#ifdef QT_DEBUG
    // Contains the timer name if a warning for this timer was already published
    std::set<std::string> m_timer_already_warned_about;
    // Writes a warning out on the screen for the given timer (only once)
    void warn_about_timer(const std::string& name);
#endif

};

}
