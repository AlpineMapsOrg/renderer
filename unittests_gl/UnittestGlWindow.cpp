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

#include "UnittestGlWindow.h"

#include <QOpenGLDebugLogger>
#include <cstdio>

#include <QTimer>
#include <QGuiApplication>
#include <catch2/catch_session.hpp>

UnittestGlWindow::UnittestGlWindow(int argc, char** argv) : QOpenGLWindow{}, m_argc(argc), m_argv(argv)
{

}


void UnittestGlWindow::initializeGL()
{
    QOpenGLDebugLogger logger;
    logger.initialize();
    logger.disableMessages(QList<GLuint>({ 131185 }));
    QObject::connect(&logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
        qDebug() << message;
    });
    logger.startLogging(QOpenGLDebugLogger::SynchronousLogging);

#ifdef __ANDROID__
    std::vector<char*> argv_2;
    for (int i = 0; i < m_argc; ++i) {
        argv_2.push_back(m_argv[i]);
    }
    std::array<char, 20> logcat_switch = {"-o %debug"};
    argv_2.push_back(logcat_switch.data());
    int argc_2 = m_argc + 1;
    int result = Catch::Session().run( argc_2, argv_2.data() );
#else
    int result = Catch::Session().run( m_argc, m_argv );
#endif
    std::fflush(stdout);

    if (result != 0)
        exit(result);

#ifndef __EMSCRIPTEN__
    QTimer::singleShot(0, QGuiApplication::instance(), &QCoreApplication::quit); // memory problems on webassembly and qt 6.5.1
#endif
}
