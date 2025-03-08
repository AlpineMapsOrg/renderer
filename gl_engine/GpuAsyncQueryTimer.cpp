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

// android implementation below
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

#ifdef __ANDROID__

#include "GpuAsyncQueryTimer.h"

#include <QDebug>
#include <QOpenGLExtraFunctions>

namespace gl_engine {

GpuAsyncQueryTimer::GpuAsyncQueryTimer(const QString& name, const QString& group, int queue_size, float average_weight)
    : nucleus::timing::TimerInterface(name, group, queue_size, average_weight)
{
    _initializeExtensionFunctions();

    if (!m_hasDisjointTimerQueryEXT) {
        qWarning() << "GL_EXT_disjoint_timer_query not available on this system."
                   << "GPU timing will be disabled for timer" << name;
        return;
    }

    // Create two sets of query IDs
    m_glGenQueriesEXT(1, &m_query[0].start);
    m_glGenQueriesEXT(1, &m_query[0].end);
    m_glGenQueriesEXT(1, &m_query[1].start);
    m_glGenQueriesEXT(1, &m_query[1].end);

    // Indices: front = 0, back = 1
    m_frontIndex = 0;
    m_backIndex = 1;
}

GpuAsyncQueryTimer::~GpuAsyncQueryTimer()
{
    if (m_hasDisjointTimerQueryEXT) {
        m_glDeleteQueriesEXT(1, &m_query[0].start);
        m_glDeleteQueriesEXT(1, &m_query[0].end);
        m_glDeleteQueriesEXT(1, &m_query[1].start);
        m_glDeleteQueriesEXT(1, &m_query[1].end);
    }
}

void GpuAsyncQueryTimer::_initializeExtensionFunctions()
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qWarning() << "No current OpenGL context. Extension not available.";
        return;
    }

    // Check for extension
    m_hasDisjointTimerQueryEXT = ctx->hasExtension("GL_EXT_disjoint_timer_query");
    if (!m_hasDisjointTimerQueryEXT)
        return;

    // Retrieve function pointers if extension is present
    m_glGenQueriesEXT = reinterpret_cast<PFNGLGENQUERIESEXTPROC>(ctx->getProcAddress("glGenQueriesEXT"));
    m_glDeleteQueriesEXT = reinterpret_cast<PFNGLDELETEQUERIESEXTPROC>(ctx->getProcAddress("glDeleteQueriesEXT"));
    m_glQueryCounterEXT = reinterpret_cast<PFNGLQUERYCOUNTEREXTPROC>(ctx->getProcAddress("glQueryCounterEXT"));
    m_glGetQueryObjectui64vEXT = reinterpret_cast<PFNGLGETQUERYOBJECTUI64VEXTPROC>(ctx->getProcAddress("glGetQueryObjectui64vEXT"));

    if (!m_glGenQueriesEXT || !m_glDeleteQueriesEXT || !m_glQueryCounterEXT || !m_glGetQueryObjectui64vEXT) {
        qWarning() << "Failed to resolve some gl*EXT function pointers. "
                   << "Disabling GPU timer for " << m_name;
        m_hasDisjointTimerQueryEXT = false;
    }
}

void GpuAsyncQueryTimer::_start()
{
    if (!m_hasDisjointTimerQueryEXT) {
        // Extension unavailable, no-op
        return;
    }
    // Insert a GPU timestamp for the "start" of the frame
    m_glQueryCounterEXT(m_query[m_frontIndex].start, 0x8E28 /* GL_TIMESTAMP_EXT */);
}

void GpuAsyncQueryTimer::_stop()
{
    if (!m_hasDisjointTimerQueryEXT) {
        // Extension unavailable, no-op
        return;
    }
    // Insert a GPU timestamp for the "end" of the frame
    m_glQueryCounterEXT(m_query[m_frontIndex].end, 0x8E28 /* GL_TIMESTAMP_EXT */);

    // Swap front/back
    std::swap(m_frontIndex, m_backIndex);
}

float GpuAsyncQueryTimer::_fetch_result()
{
    if (!m_hasDisjointTimerQueryEXT) {
        // Extension not available => no GPU timing
        return 0.0f;
    }

    // We'll read the queries from the "back" frame
    GLuint64 startTime = 0;
    GLuint64 endTime = 0;

    // Retrieve the start timestamp
    m_glGetQueryObjectui64vEXT(m_query[m_backIndex].start, 0x8866 /*GL_QUERY_RESULT_EXT*/, &startTime);
    // Retrieve the end timestamp
    m_glGetQueryObjectui64vEXT(m_query[m_backIndex].end, 0x8866 /*GL_QUERY_RESULT_EXT*/, &endTime);

    // Compute difference in nanoseconds, convert to ms
    const double diffNs = static_cast<double>(endTime - startTime);
    const double diffMs = diffNs / 1.0e6;

    return static_cast<float>(diffMs);
}

} // namespace gl_engine

#endif
