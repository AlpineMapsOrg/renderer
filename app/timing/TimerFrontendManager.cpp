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

#include "TimerFrontendManager.h"

#include <QDebug>

TimerFrontendManager::TimerFrontendManager(QObject* parent)
    :QObject(parent)
{}

TimerFrontendManager::~TimerFrontendManager()
{
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("~TimerFrontendManager()");
#endif
}

int TimerFrontendManager::current_frame = 0;
void TimerFrontendManager::receive_measurements(QList<nucleus::timing::TimerReport> values)
{
    for (const auto& report : values) {
        const char* name = report.timer->get_name().c_str();
        if (!m_timer_map.contains(name)) {
            auto tfo = new TimerFrontendObject(this, name, report.timer->get_group().c_str(), report.timer->get_queue_size(), report.timer->get_average_weight(), report.value);
            m_timer.append(tfo);
            m_timer_map.insert(name, tfo);
        }
        m_timer_map[name]->add_measurement(report.value, current_frame++);
    }
    emit updateTimingList(m_timer);
}
