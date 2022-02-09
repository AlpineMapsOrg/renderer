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

#include "render_backend/utils/QuadTree.h"

#include <catch2/catch.hpp>

namespace {
struct DeletionChecker {
  static unsigned counter;
  DeletionChecker() {
    counter++;
  }
  DeletionChecker(const DeletionChecker&) {
    counter++;
  }
  ~DeletionChecker() {
    counter--;
  }
  DeletionChecker& operator=(const DeletionChecker&) { return *this; }
};
unsigned DeletionChecker::counter = 0;
}

TEST_CASE("QuadTree") {
  SECTION("construction and basics") {
    QuadTreeNode<unsigned> root(42);
    CHECK(root.hasChildren() == false);
    CHECK(root.data() == 42);
    root.data() = 43;
    CHECK(root.data() == 43);
  }
  SECTION("adding children") {
    QuadTreeNode<unsigned> root(42);
    root.addChildren({1, 2, 3, 4});
    CHECK(root.hasChildren() == true);
    CHECK(root.data() == 42);

    CHECK(root[0].data() == 1);
    CHECK(root[1].data() == 2);
    CHECK(root[2].data() == 3);
    CHECK(root[3].data() == 4);

    CHECK(!root[0].hasChildren());
    root[0].addChildren({5, 6, 7, 8});
    CHECK(root[0].hasChildren());

    CHECK(root[0][0].data() == 5);
    CHECK(root[0][1].data() == 6);
    CHECK(root[0][2].data() == 7);
    CHECK(root[0][3].data() == 8);
  }
  SECTION("adding and removing children") {
    QuadTreeNode<DeletionChecker> root({});
    CHECK(DeletionChecker::counter == 1);
    root.addChildren({});
    CHECK(DeletionChecker::counter == 5);
    root[0].addChildren({});
    CHECK(DeletionChecker::counter == 9);
    root.removeChildren();
    CHECK(root.hasChildren() == false);
    CHECK(DeletionChecker::counter == 1);
  }
}
