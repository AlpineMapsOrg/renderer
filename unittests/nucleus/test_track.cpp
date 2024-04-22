#include <catch2/catch_test_macros.hpp>

#include "nucleus/utils/GPX.h"
#include <QString>
#include <iostream>

using namespace nucleus::gpx;

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
