/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#include <QThread>
#include <catch2/catch_test_macros.hpp>

#include "nucleus/utils/Stopwatch.h"

#ifdef __EMSCRIPTEN__
constexpr auto timing_multiplicator = 10ll;
#elif defined _MSC_VER
constexpr auto timing_multiplicator = 10ll;
#elif defined(__ANDROID__) && (defined(__i386__) || defined(__x86_64__))
constexpr auto timing_multiplicator = 100ll;
#else
constexpr auto timing_multiplicator = 5ll;
#endif

TEST_CASE("nucleus/utils/Stopwatch")
{
    SECTION("lap")
    {
        auto dt = nucleus::utils::Stopwatch();
#if defined(__ANDROID__) && (defined(__i386__) || defined(__x86_64__))
        CHECK(dt.lap().count() < 100); // for some reason the emulator needs around 60msec to get consecutive time measurements!
#else
        CHECK(dt.lap().count() == 0);
#endif
        QThread::msleep(5 * timing_multiplicator);
        auto t2 = dt.lap();
        CHECK(t2.count() >= 5 * timing_multiplicator);
        CHECK(t2.count() < 7 * timing_multiplicator);
    }

    SECTION("total")
    {
        auto dt = nucleus::utils::Stopwatch();
        CHECK(dt.total().count() == 0);
        QThread::msleep(5 * timing_multiplicator);
        CHECK(dt.total().count() >= 5 * timing_multiplicator);
        CHECK(dt.total().count() < 7 * timing_multiplicator);
        QThread::msleep(5 * timing_multiplicator);
        CHECK(dt.total().count() >= 10 * timing_multiplicator);
        CHECK(dt.total().count() < 14 * timing_multiplicator);
    }

    SECTION("restart")
    {
        auto dt = nucleus::utils::Stopwatch();
        QThread::msleep(5);
        dt.restart();
#if defined(__ANDROID__) && (defined(__i386__) || defined(__x86_64__))
        CHECK(dt.lap().count() < 100); // for some reason the emulator needs around 60msec to get consecutive time measurements!
#else
        CHECK(dt.lap().count() == 0);
#endif
    }
}
