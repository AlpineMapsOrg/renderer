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

#pragma once

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)

#include "nucleus/timing/TimerInterface.h"


#include <QOpenGLTimerQuery>


namespace gl_engine {

/// The AsyncQueryTimer class works with a front- and a
/// back-queue to ensure no necessary waiting time for results.
/// Fetching returns the values of the backbuffer (=> the previous frame)
class GpuAsyncQueryTimer : public nucleus::timing::TimerInterface {

public:
    GpuAsyncQueryTimer(const QString& name, const QString& group, int queue_size, float average_weight);
    ~GpuAsyncQueryTimer();

protected:
    // starts front-buffer query
    void _start() override;
    // stops front-buffer query and toggles indices
    void _stop() override;
    // fetches back-buffer query and returns true if new
    float _fetch_result() override;

private:
    // four timers for front and backbuffer (à start-, endtime)
    QOpenGLTimerQuery* m_qTmr[4];
    int m_current_fb_offset = 0;
    int m_current_bb_offset = 2;

};

}

#endif
