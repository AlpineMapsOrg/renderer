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

#include <QCoreApplication>
#include <QTimer>
#include <QtTest/QSignalSpy>

#define CATCH_CONFIG_MAIN
#ifdef __MINGW32__
#define DO_NOT_USE_WMAIN
#endif
#include <catch2/catch.hpp>

#ifdef NDEBUG
constexpr bool asserts_are_enabled = false;
#else
constexpr bool asserts_are_enabled = true;
#endif

namespace {
int argc = 0;
static QCoreApplication app = {argc, nullptr};
} // namespace

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
