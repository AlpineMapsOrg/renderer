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

#include "nucleus/utils/DeltaTime.h"

#include <catch2/catch.hpp>
#include <QThread>

//using test_helpers::equals;

TEST_CASE("nucleus/utils/DeltaTime")
{
    SECTION("get")
    {
        auto dt = DeltaTime();
        dt.get();
        auto t1 = dt.get();
        CHECK(t1.count() == 0);

        dt.get();
        QThread::msleep(100);
        auto t2 = dt.get();
        CHECK(t2.count() - 100 >= 0);
        CHECK(t2.count() - 100 < 10);
    }
    SECTION("reset")
    {
        auto dt = DeltaTime();
        dt.get();
        QThread::msleep(100);
        dt.reset();
        auto t1 = dt.get();
        CHECK(t1.count() == 0);
    }
}
