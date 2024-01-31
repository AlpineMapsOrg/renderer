/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "UnittestGLContext.h"

#include <cstdio>
#include <memory>

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLDebugLogger>
#include <QTimer>
#include <catch2/catch_test_macros.hpp>

UnittestGLContext::UnittestGLContext()
{
    QSurfaceFormat surface_format;
    surface_format.setDepthBufferSize(24);
    surface_format.setOption(QSurfaceFormat::DebugContext);

    // Request OpenGL 3.3 core or OpenGL ES 3.0.
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        qDebug("Requesting 3.3 core context");
        surface_format.setVersion(3, 3);
        surface_format.setProfile(QSurfaceFormat::CoreProfile);
    } else {
        qDebug("Requesting 3.0 context");
        surface_format.setVersion(3, 0);
    }

    QSurfaceFormat::setDefaultFormat(surface_format);
    m_context.setFormat(surface_format);

    surface.create();
    const auto r = m_context.create();
    assert(r);
    Q_UNUSED(r);
    m_context.makeCurrent(&surface);

    QOpenGLDebugLogger logger;
    const auto debug_logger_supported = logger.initialize();
#if !(defined(__EMSCRIPTEN__) || (defined(__ANDROID__) && (defined(__i386__) || defined(__x86_64__))))
    // not supported by webassembly and it's ok. errors are printed to the java script console
    // also not supported by the android emulator (and i wouldn't know what to do about it)
    CHECK(debug_logger_supported);
#endif

    if (debug_logger_supported) {
        logger.disableMessages(QList<GLuint>({131185}));
        QObject::connect(&logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
            qDebug() << message;
        });
        logger.startLogging(QOpenGLDebugLogger::SynchronousLogging);
    }
}

void UnittestGLContext::initialise()
{
    static std::unique_ptr<UnittestGLContext> s_instance = std::unique_ptr<UnittestGLContext>(new UnittestGLContext());
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::destroyed, []() { s_instance.reset(); });
}
