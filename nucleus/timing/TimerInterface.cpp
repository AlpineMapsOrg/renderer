/*****************************************************************************
 * AlpineMaps.org
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

#include "TimerInterface.h"

namespace nucleus::timing {

TimerInterface::TimerInterface(const QString& name, const QString& group, int queue_size, float average_weight)
    : m_name(name)
    , m_group(group)
    , m_queue_size(queue_size)
    , m_average_weight(average_weight)
{
}

void TimerInterface::start() {
    //assert(m_state == TimerStates::READY);
    _start();
    m_state = TimerStates::RUNNING;
}

void TimerInterface::stop() {
    //assert(m_state == TimerStates::RUNNING);
    _stop();
    m_state = TimerStates::STOPPED;
}

bool TimerInterface::fetch_result() {
    if (m_state == TimerStates::STOPPED) {
        float val = _fetch_result();
        this->m_last_measurement = val;
        m_state = TimerStates::READY;
        return true;
    }
    return false;
}

}

