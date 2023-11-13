#include <catch2/catch_test_macros.hpp>

#include "nucleus/Track.h"
#include <QString>
#include <iostream>

using namespace nucleus::gpx;

TEST_CASE("nucleus/Track")
{
    SECTION("Parse wikipedia.gpx file")
    {
        // https://en.wikipedia.org/wiki/GPS_Exchange_Format
        
        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "wikipedia.gpx");

        std::unique_ptr<Gpx> gpx = parse(filepath);

        CHECK(gpx->track.size() == 1); // there is one segment
        CHECK(gpx->track[0].size() == 3); // there are three trackpoints
    }

    SECTION("Parse husarentempel.gpx file")
    {
        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "husarentempel.gpx");

        std::unique_ptr<Gpx> gpx = parse(filepath);

        CHECK(gpx->track.size() == 1);
        CHECK(gpx->track[0].size() == 891);
    }
}
