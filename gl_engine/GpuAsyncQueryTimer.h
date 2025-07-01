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

// android implementation below
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

#ifdef __ANDROID__

#include "nucleus/timing/TimerInterface.h"
#include <QOpenGLContext>

namespace gl_engine {

/// The GpuAsyncQueryTimer class uses QueryCounterEXT(GL_TIMESTAMP_EXT)
/// to measure GPU time between _start() and _stop(). It swaps front/back
/// so that _fetch_result() reads last frame’s queries.
class GpuAsyncQueryTimer : public nucleus::timing::TimerInterface {
    struct GlTimerQuery {
        GLuint start = 0; ///< Query object for "start timestamp"
        GLuint end = 0; ///< Query object for "end timestamp"
    };
    using PFNGLGENQUERIESEXTPROC = void (*)(GLsizei n, GLuint* ids);
    using PFNGLDELETEQUERIESEXTPROC = void (*)(GLsizei n, const GLuint* ids);
    using PFNGLQUERYCOUNTEREXTPROC = void (*)(GLuint id, GLenum target);
    using PFNGLGETQUERYOBJECTUI64VEXTPROC = void (*)(GLuint id, GLenum pname, GLuint64* params);

public:
    GpuAsyncQueryTimer(const QString& name, const QString& group, int queue_size, float average_weight);
    ~GpuAsyncQueryTimer() override;

protected:
    void _start() override; ///< Issue timestamp for the "start" of this frame
    void _stop() override; ///< Issue timestamp for the "end" of this frame + swap
    float _fetch_result() override; ///< Return time in ms from previous frame

private:
    // Two sets of query IDs for front/back frames.
    GlTimerQuery m_query[2];
    int m_frontIndex = 0; ///< "this frame"
    int m_backIndex = 1; ///< "previous frame"

    // True if the GL_EXT_disjoint_timer_query extension is available
    bool m_hasDisjointTimerQueryEXT = false;

    // Function pointers from the extension
    // Function pointers from the extension
    PFNGLGENQUERIESEXTPROC m_glGenQueriesEXT = nullptr;
    PFNGLDELETEQUERIESEXTPROC m_glDeleteQueriesEXT = nullptr;
    PFNGLQUERYCOUNTEREXTPROC m_glQueryCounterEXT = nullptr;
    PFNGLGETQUERYOBJECTUI64VEXTPROC m_glGetQueryObjectui64vEXT = nullptr;

    // Helper to load extension pointers
    void _initializeExtensionFunctions();
};

} // namespace gl_engine

#endif
