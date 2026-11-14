#include <catch2/catch_test_macros.hpp>

#include "nucleus/track/GPX.h"
#include <QString>
#include <iostream>

using namespace nucleus::track;

TEST_CASE("GPX")
{
    SECTION("Parse example.gpx file")
    {
        QString filepath  = QString("%1%2").arg(ALP_TEST_DATA_DIR, "example.gpx");

        std::unique_ptr<Gpx> gpx = parse(filepath);

        CHECK(gpx != nullptr);

        CHECK(gpx->track.size() == 2); // two segements
        CHECK(gpx->track[0].size() == 3); // there are three trackpoints in segment 1
        CHECK(gpx->track[1].size() == 2); // there are two trackpoints in segment 2
    }
}

// the route planner lists the points of all segments flat and passes those
// indices back to the editing operations, so they must resolve them across
// segment boundaries
TEST_CASE("GPX editing on flattened indices")
{
    // two segments: [p0, p1, p2] and [p3, p4], identified by their latitude
    Gpx gpx;
    gpx.add_new_segment();
    for (int i = 0; i < 3; ++i) {
        gpx.add_new_point({double(i), 0.0, 0.0});
    }
    gpx.add_new_segment();
    for (int i = 3; i < 5; ++i) {
        gpx.add_new_point({double(i), 0.0, 0.0});
    }
    REQUIRE(gpx.point_count() == 5);
    REQUIRE(gpx.track.size() == 2);

    SECTION("delete resolves flattened indices across segments")
    {
        gpx.delete_point(1); // p1 in the first segment
        REQUIRE(gpx.point_count() == 4);
        CHECK(gpx.track[0].size() == 2);
        CHECK(gpx.track[0][0].latitude == 0.0);
        CHECK(gpx.track[0][1].latitude == 2.0);
        CHECK(gpx.track[1][0].latitude == 3.0);

        gpx.delete_point(2); // p3, the first point of the second segment
        REQUIRE(gpx.point_count() == 3);
        CHECK(gpx.track[1][0].latitude == 4.0);

        gpx.delete_point(0);
        gpx.delete_point(0);
        gpx.delete_point(0);
        CHECK(gpx.point_count() == 0);
        CHECK(gpx.track.empty()); // emptied segments are dropped
    }

    SECTION("delete with an out-of-range index is a no-op")
    {
        gpx.delete_point(5);
        gpx.delete_point(100);
        CHECK(gpx.point_count() == 5);
        CHECK(gpx.track.size() == 2);
    }

    SECTION("move within a segment")
    {
        CHECK(gpx.move_point(1, -1) == 0);
        CHECK(gpx.track[0][0].latitude == 1.0);
        CHECK(gpx.track[0][1].latitude == 0.0);
        CHECK(gpx.move_point(0, 1) == 1);
        CHECK(gpx.track[0][0].latitude == 0.0);
        CHECK(gpx.track[0][1].latitude == 1.0);
    }

    SECTION("move down across a segment boundary")
    {
        CHECK(gpx.move_point(2, 1) == 3); // p2 becomes the second segment's second point
        REQUIRE(gpx.track.size() == 2);
        CHECK(gpx.track[0].size() == 2);
        CHECK(gpx.track[1].size() == 3);
        CHECK(gpx.track[1][0].latitude == 3.0);
        CHECK(gpx.track[1][1].latitude == 2.0);
        CHECK(gpx.track[1][2].latitude == 4.0);

        // moving back up restores the flat order; a flat swap does not track
        // segment membership, so p2 stays in the second segment
        CHECK(gpx.move_point(3, -1) == 2);
        CHECK(gpx.track[0].size() == 2);
        CHECK(gpx.track[1].size() == 3);
        CHECK(gpx.track[1][0].latitude == 2.0);
        CHECK(gpx.track[1][1].latitude == 3.0);
        CHECK(gpx.track[1][2].latitude == 4.0);
    }

    SECTION("move up across a segment boundary")
    {
        CHECK(gpx.move_point(3, -1) == 2); // p3 joins the first segment
        REQUIRE(gpx.track.size() == 2);
        CHECK(gpx.track[0].size() == 4);
        CHECK(gpx.track[0][2].latitude == 3.0);
        CHECK(gpx.track[0][3].latitude == 2.0);
        CHECK(gpx.track[1].size() == 1);
    }

    SECTION("move collapsing a one-point segment")
    {
        gpx.delete_point(3);
        REQUIRE(gpx.point_count() == 4);
        REQUIRE(gpx.track.size() == 2);
        REQUIRE(gpx.track[1].size() == 1);

        CHECK(gpx.move_point(3, -1) == 2); // p4 moves up, its segment empties
        CHECK(gpx.track.size() == 1); // the emptied segment is dropped
        CHECK(gpx.track[0].size() == 4);
        CHECK(gpx.track[0][2].latitude == 4.0);
        CHECK(gpx.track[0][3].latitude == 2.0);
    }

    SECTION("move at the boundaries or with an invalid index is a no-op")
    {
        CHECK(gpx.move_point(0, -1) == 0);
        CHECK(gpx.move_point(4, 1) == 4);
        CHECK(gpx.move_point(5, -1) == 5);
        CHECK(gpx.point_count() == 5);
        CHECK(gpx.track.size() == 2);
    }

    SECTION("insert at flattened indices")
    {
        gpx.insert_point(0, {10.0, 0.0, 0.0}); // before the first point
        CHECK(gpx.track[0][0].latitude == 10.0);
        gpx.insert_point(4, {11.0, 0.0, 0.0}); // before p3, the second segment's first point
        CHECK(gpx.track[1][0].latitude == 11.0);
        gpx.insert_point(7, {12.0, 0.0, 0.0}); // at the very end
        CHECK(gpx.track.back().back().latitude == 12.0);
        CHECK(gpx.point_count() == 8);
    }

    SECTION("delete_last_point skips empty trailing segments")
    {
        gpx.add_new_segment(); // e.g. an empty <trkseg/> from the parser
        REQUIRE(gpx.point_count() == 5);
        gpx.delete_last_point(); // p4 from the second segment
        REQUIRE(gpx.point_count() == 4);
        CHECK(gpx.track.size() == 2);
        CHECK(gpx.track[1].back().latitude == 3.0);

        Gpx single;
        single.add_new_segment();
        single.add_new_point({1.0, 0.0, 0.0});
        single.add_new_segment();
        single.delete_last_point();
        CHECK(single.point_count() == 0);
        CHECK(single.track.empty());
    }

    SECTION("flatten_segments merges into one leg and drops empty ones")
    {
        gpx.flatten_segments();
        REQUIRE(gpx.track.size() == 1);
        REQUIRE(gpx.point_count() == 5);
        for (unsigned i = 0; i < 5; ++i) {
            CHECK(gpx.track[0][i].latitude == double(i));
        }

        Gpx empty;
        empty.add_new_segment();
        empty.add_new_segment();
        empty.flatten_segments();
        CHECK(empty.track.empty());
    }
}
