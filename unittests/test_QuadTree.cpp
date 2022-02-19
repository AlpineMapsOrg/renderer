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

#include "alpine_renderer/utils/QuadTree.h"

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
    REQUIRE(root.hasChildren() == true);
    CHECK(root.data() == 42);

    CHECK(root[0].data() == 1);
    CHECK(root[1].data() == 2);
    CHECK(root[2].data() == 3);
    CHECK(root[3].data() == 4);

    CHECK(!root[0].hasChildren());
    root[0].addChildren({5, 6, 7, 8});
    REQUIRE(root[0].hasChildren());

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
  SECTION("refine and reduce") {
    QuadTreeNode<int> root(0);
    root.addChildren({42, 1, 6, -1});
    root[0].addChildren({43, 44, 45, 46});
    root[0][0].addChildren({-1, -2, -3, -4});
    root[0][0][0].addChildren({-5, -6, -7, -8});
    root[0][1].addChildren({-1, -2, -3, -4});
    root[0][2].addChildren({-5, -6, -7, -8});
    root[0][3].addChildren({0, 0, 0, -1});

    const auto refine_predicate = [](const auto& node_value) {
      if (node_value <= 0) return false;
      return true;
    };
    const auto generate_children_function = [](const auto& node_value) {
      const auto t = node_value / 4;
      const auto t2 = node_value % 4 - 1;
      return std::array<int, 4>({t + t2, t, t, t});
    };
    quad_tree::refine(&root,  refine_predicate, generate_children_function);

    REQUIRE(root.hasChildren());

    // children of root[0] were not changed
    REQUIRE(root[0].hasChildren());
    REQUIRE(root[0][0].hasChildren());
    REQUIRE(root[0][0][0].hasChildren());
    REQUIRE(root[0][1].hasChildren());
    REQUIRE(root[0][2].hasChildren());
    REQUIRE(root[0][3].hasChildren());

    REQUIRE(root[0][0][0].hasChildren());
    CHECK(!root[0][0][1].hasChildren());
    CHECK(!root[0][0][2].hasChildren());
    CHECK(!root[0][0][3].hasChildren());
    for (unsigned i = 0; i < 4; ++i) {
      CHECK(!root[0][0][0][i].hasChildren());
      CHECK(!root[0][1][i].hasChildren());
      CHECK(!root[0][2][i].hasChildren());
      CHECK(!root[0][3][i].hasChildren());
    }

    const auto check_nodes123 = [&root]() {
         // root[1] got its children
      REQUIRE(root[1].hasChildren());
      CHECK(!root[1][0].hasChildren());
      CHECK(!root[1][1].hasChildren());
      CHECK(!root[1][2].hasChildren());
      CHECK(!root[1][3].hasChildren());


         // root[2] got its children
      CHECK(root[2].data() == 6);
      REQUIRE(root[2].hasChildren());
      CHECK(root[2][0].data() == 2);
      REQUIRE(root[2][0].hasChildren());
      CHECK(root[2][0][0].data() == 1);
      CHECK(root[2][0][1].data() == 0);
      CHECK(root[2][0][2].data() == 0);
      CHECK(root[2][0][3].data() == 0);
      REQUIRE(root[2][0][0].hasChildren());
      CHECK(!root[2][0][0][0].hasChildren());
      CHECK(!root[2][0][0][1].hasChildren());
      CHECK(!root[2][0][0][2].hasChildren());
      CHECK(!root[2][0][0][3].hasChildren());
      CHECK(!root[2][0][1].hasChildren());
      CHECK(!root[2][0][2].hasChildren());
      CHECK(!root[2][0][3].hasChildren());
      for (unsigned j = 1; j < 4; ++j) {
        CHECK(root[2][j].data() == 1);
        REQUIRE(root[2][j].hasChildren());
        for (unsigned i = 0; i < 4; ++i) {
          CHECK(root[2][j][i].data() == 0);
          CHECK(!root[2][j][i].hasChildren());
        }
      }

         // root[3] got no children
      CHECK(root[3].data() == -1);
      CHECK(!root[3].hasChildren());
    };
    check_nodes123();

    const auto reduce_predicate = [](const auto& node_value) {
      if (node_value < 0) return true;
      return false;
    };

    quad_tree::reduce(&root, reduce_predicate);
    CHECK(root[0].data() == 42);
    REQUIRE(root[0].hasChildren());
    for (unsigned i = 0; i < 4; ++i) {
      CHECK(root[0][i].data() == 43 + int(i));
      if (i < 3) {
        CHECK(!root[0][i].hasChildren());
      }
      else {
        CHECK(root[0][i].data() == 46);
        REQUIRE(root[0][i].hasChildren());
        for (unsigned j = 0; j < 4; ++j) {
          CHECK(!root[0][i][j].hasChildren());
          if (j < 3)
            CHECK(root[0][i][j].data() == 0);
          else
            CHECK(root[0][i][j].data() == -1);
        }
      }
    }
    check_nodes123();
//    root.addChildren({0, 1, 6, -1});
//    root[0].addChildren({0, 0, 0, 0});
//    root[0][0].addChildren({-1, -1, -1, -1});
//    root[0][0][0].addChildren({-1, -1, -1, -1});
//    root[0][1].addChildren({-1, -1, -1, -1});
//    root[0][2].addChildren({-1, -1, -1, -1});
//    root[0][3].addChildren({0, 0, 0, -1});
  }
}

TEST_CASE("on the fly QuadTree") {

  const auto refine_predicate = [](const auto& node_value) {
    if (node_value <= 0) return false;
    return true;
  };
  const auto generate_children_function = [](const auto& node_value) {
    const auto t = node_value / 4;
    const auto t2 = node_value % 4 - 1;
    return std::array<int, 4>({t + t2, t, t, t});
  };
  SECTION("do not create") {
    const auto leaves = quad_tree::onTheFlyTraverse(42,  [](const auto&) { return false; }, generate_children_function);
    CHECK(leaves.size() == 1);
    CHECK(leaves.front() == 42);
  }
  SECTION("create one level") {
    bool created = false;
    const auto leaves = quad_tree::onTheFlyTraverse(42,
        [&created](const auto&) {
          if (created)
            return false;
          created = true;
          return true;
        },
        [](const auto&) {
          return std::array<int, 4>({1, 2, 3, 4});
        });
    CHECK(leaves.size() == 4);
    CHECK(leaves == std::vector({1, 2, 3, 4}));
  }
  SECTION("create 2 levels") {
    int predicate_call = 0;
    int generate_call = 0;
    const auto leaves = quad_tree::onTheFlyTraverse(42,
        [&predicate_call](const auto&) {
          ++predicate_call;
          return predicate_call == 1 || predicate_call == 2;
        },
        [&generate_call](const auto&) {
          generate_call++;
          if (generate_call == 1)
            return std::array<int, 4>({1, 2, 3, 4});
          if (generate_call == 2)
            return std::array<int, 4>({5, 6, 7, 8});
          CHECK(false);
          return std::array<int, 4>({11, 11, 11, 11});
        });
    CHECK(leaves.size() == 7);
    CHECK(std::ranges::find(leaves, 1) == leaves.end());
    for (int i = 2; i < 9; ++i) {
      CHECK(std::ranges::find(leaves, i) != leaves.end());
    }
    CHECK(std::ranges::find(leaves, 11) == leaves.end());
  }

}
