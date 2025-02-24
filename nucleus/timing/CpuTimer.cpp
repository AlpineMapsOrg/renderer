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

#include "CpuTimer.h"

namespace nucleus::timing {

CpuTimer::CpuTimer(const QString& name, const QString& group, int queue_size, const float average_weight)
    : TimerInterface(name, group, queue_size, average_weight)
{
}

void CpuTimer::_start() {
    m_ticks[0] = std::chrono::high_resolution_clock::now();
}

void CpuTimer::_stop() {
    m_ticks[1] = std::chrono::high_resolution_clock::now();
}

float CpuTimer::_fetch_result() {
    std::chrono::duration<double> diff = m_ticks[1] - m_ticks[0];
    return ((float)(diff.count() * 1000.0));
}

}
