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

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)

#include "GpuAsyncQueryTimer.h"
#include "QOpenGLContext"

namespace gl_engine {

GpuAsyncQueryTimer::GpuAsyncQueryTimer(const QString& name, const QString& group, int queue_size, const float average_weight)
    : nucleus::timing::TimerInterface(name, group, queue_size, average_weight)
{
    for (int i = 0; i < 4; i++) {
        m_qTmr[i] = new QOpenGLTimerQuery(QOpenGLContext::currentContext());
        m_qTmr[i]->create();
    }
    // Lets record a timestamp for the backbuffer such that we can fetch it and
    // don't have to implement special treatment for the first fetch
    m_qTmr[m_current_bb_offset]->recordTimestamp();
    m_qTmr[m_current_bb_offset + 1]->recordTimestamp();
}

GpuAsyncQueryTimer::~GpuAsyncQueryTimer() {
    for (int i = 0; i < 4; i++) {
        delete m_qTmr[i];
    }
}

void GpuAsyncQueryTimer::_start() {
    m_qTmr[m_current_fb_offset]->recordTimestamp();
}

void GpuAsyncQueryTimer::_stop() {
    m_qTmr[m_current_fb_offset + 1]->recordTimestamp();
}

float GpuAsyncQueryTimer::_fetch_result() {
#ifdef QT_DEBUG
    if (!m_qTmr[m_current_bb_offset]->isResultAvailable() || !m_qTmr[m_current_bb_offset + 1]->isResultAvailable()) {
        qWarning() << "A timer result is not available yet for timer" << m_name << ". The Thread will be blocked.";
    }
#endif
    GLuint64 elapsed_time = m_qTmr[m_current_bb_offset + 1]->waitForResult() - m_qTmr[m_current_bb_offset]->waitForResult();
    std::swap(m_current_fb_offset, m_current_bb_offset);
    return elapsed_time / 1000000.0f;
}

}

#endif
