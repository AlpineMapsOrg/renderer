/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
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

#include <chrono>
#include <cmath>
#include <limits>

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QScreen>
#include <catch2/catch_test_macros.hpp>

#include "unittests_gl/UnittestGlWindow.h"

#ifdef __EMSCRIPTEN__
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <fmt/core.h>

class ProgressPrinter : public Catch::EventListenerBase {
public:
    using EventListenerBase::EventListenerBase;

    static std::string getDescription() {
        return "Reporter that reports the progress after every test case.";
    }

    void testCaseEnded(const Catch::TestCaseStats& status) override {
        fmt::print("test case: {:<140}", status.testInfo->name);
        if (status.totals.testCases.allOk())
            fmt::println("\033[0;32mpassed {:>4} of {:>4}\033[0m", status.totals.assertions.passed, status.totals.assertions.total());
        else
            fmt::println("\033[0;31mfailed {:>4} of {:>4}\033[0m", status.totals.assertions.failed, status.totals.assertions.total());
        std::fflush(stdout);
    }
};

CATCH_REGISTER_LISTENER(ProgressPrinter)
#endif

int main( int argc, char* argv[] ) {
    std::fflush(stdout);
    int argc_qt = 0;
    QGuiApplication app = {argc_qt, argv};

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setOption(QSurfaceFormat::DebugContext);

    // Request OpenGL 3.3 core or OpenGL ES 3.0.
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        qDebug("Requesting 3.3 core context");
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
    } else {
        qDebug("Requesting 3.0 context");
        fmt.setVersion(3, 0);
    }
    QSurfaceFormat::setDefaultFormat(fmt);

    // Catch::Session().run(m_argc, m_argv); is in UnittestGlWindow::initializeGL()
    // to my understanding this is necessary for webassembly, because stuff is started
    // asynchronously, and the gl context is not yet available when main is running.
    UnittestGlWindow gl_window(argc, argv);
#ifdef __EMSCRIPTEN__
    gl_window.showMaximized();
    return app.exec();
#else
    QOffscreenSurface surface;
    surface.create();
    QOpenGLContext c;
    const auto r = c.create();
    assert(r);
    c.makeCurrent(&surface);
    gl_window.initializeGL();
    return 0;
#endif
}

#ifdef NDEBUG
constexpr bool asserts_are_enabled = false;
#else
constexpr bool asserts_are_enabled = true;
#endif

TEST_CASE("gl/main: check that asserts are enabled")
{
    CHECK(asserts_are_enabled);
}

TEST_CASE("gl/main: check that NaNs are enabled (-ffast-math removes support, -fno-finite-math-only puts it back in)")
{
    CHECK(std::isnan(std::numeric_limits<float>::quiet_NaN() * float(std::chrono::system_clock::now().time_since_epoch().count())));
    CHECK(std::isnan(double(std::numeric_limits<float>::quiet_NaN() * float(std::chrono::system_clock::now().time_since_epoch().count()))));
}
