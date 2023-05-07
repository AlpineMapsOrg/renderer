/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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
#include <limits>
#include <iostream>

#include <QGuiApplication>
#include <QTimer>
#include <QtTest/QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

class QtEventLoopCallAdapter : public Catch::EventListenerBase {
public:
    using EventListenerBase::EventListenerBase;

    static std::string getDescription() {
        return "Reporter that calls QCoreApplication::process events after every test case.";
    }

    void sectionStarting(const Catch::SectionInfo&) override {
        QGuiApplication::processEvents(QEventLoop::AllEvents, 1);
    }

    void sectionEnded(const Catch::SectionStats&) override {
        QGuiApplication::processEvents(QEventLoop::AllEvents, 1);
    }
};

CATCH_REGISTER_LISTENER(QtEventLoopCallAdapter)

int main( int argc, char* argv[] ) {
    static QGuiApplication app = {argc, argv};

    int result = Catch::Session().run( argc, argv );

    QTimer::singleShot(0, &app, &QCoreApplication::quit);

    return QGuiApplication::exec() + result;
}

#ifdef NDEBUG
constexpr bool asserts_are_enabled = false;
#else
constexpr bool asserts_are_enabled = true;
#endif

TEST_CASE("nucleus/main: check that asserts are enabled")
{
    CHECK(asserts_are_enabled);
}

TEST_CASE("nucleus/main: check that NaNs are enabled (-ffast-math removes support, -fno-finite-math-only puts it back in)")
{
    CHECK(std::isnan(std::numeric_limits<float>::quiet_NaN() * float(std::chrono::system_clock::now().time_since_epoch().count())));
    CHECK(std::isnan(double(std::numeric_limits<float>::quiet_NaN() * float(std::chrono::system_clock::now().time_since_epoch().count()))));
}

TEST_CASE("nucleus/main: qt signals and slots")
{
    QTimer t;
    t.setSingleShot(true);
    t.setInterval(1);
    QSignalSpy spy(&t, &QTimer::timeout);
    t.start();
    spy.wait(20);
    CHECK(spy.size() == 1);
}
