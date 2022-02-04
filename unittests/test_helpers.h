/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <catch2/catch.hpp>

namespace test_helpers {
class FailOnCopy {
  int v = 0;
public:
  FailOnCopy() = default;
  FailOnCopy(const FailOnCopy& other) : v(other.v) { CHECK(false); }
  FailOnCopy(FailOnCopy&& other) noexcept : v(other.v) { CHECK(true); }
  FailOnCopy& operator=(const FailOnCopy& other) { v = other.v; CHECK(false); return *this; }
  FailOnCopy& operator=(FailOnCopy&& other) noexcept { v = other.v; CHECK(true); return *this; }
  ~FailOnCopy() = default;
};

}
