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

#include "TimerInterface.h"
#include "chrono"

namespace nucleus::timing {

/// The CpuTimer class measures times on the c++ side using the std::chronos library
class CpuTimer : public TimerInterface {
public:
    CpuTimer(const std::string& name, const std::string& group, int queue_size, float average_weight);

protected:
    // starts front-buffer query
    void _start() override;
    // stops front-buffer query and toggles indices
    void _stop() override;
    // fetches back-buffer query
    float _fetch_result() override;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_ticks[2];
};

}

