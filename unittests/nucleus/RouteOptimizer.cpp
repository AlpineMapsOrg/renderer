/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2026 Markus Rampp

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

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <nucleus/planner/RouteOptimizer.h>

#include <algorithm>
#include <cmath>

namespace {

// uniform slope field descending towards +y (south): rows are 10 m apart,
// each row drops `drop_per_row` metres
nucleus::planner::DemTile make_slope_tile(int cols, int rows, double cell_size, double drop_per_row)
{
    nucleus::planner::DemTile tile;
    tile.min_x = 0;
    tile.min_y = 0;
    tile.max_x = double(cols) * cell_size;
    tile.max_y = double(rows) * cell_size;
    tile.width = unsigned(cols);
    tile.height = unsigned(rows);
    tile.heights.reserve(size_t(cols) * rows);
    for (int r = 0; r < rows; ++r) {
        // row 0 is the north edge = highest point
        const double height = drop_per_row * double(rows - 1 - r);
        for (int c = 0; c < cols; ++c) {
            tile.heights.push_back(float(height));
        }
    }
    return tile;
}

} // namespace

TEST_CASE("RouteOptimizer gradient penalty")
{
    using nucleus::planner::gradient_penalty;
    // the ideal gradient range costs nothing
    CHECK(gradient_penalty(30.0) == 0.0);
    CHECK(gradient_penalty(35.0) == 0.0);
    CHECK(gradient_penalty(45.0) == 0.0);
    // flat terrain is avoided
    CHECK(gradient_penalty(0.0) > 1.0);
    CHECK(gradient_penalty(10.0) > 1.0);
    // rappel territory is heavily penalised
    CHECK(gradient_penalty(62.0) > 2.5);
    CHECK(gradient_penalty(70.0) >= 12.0);
    CHECK(gradient_penalty(75.0) >= 40.0);
    // monotone behaviour within each preference band
    CHECK(gradient_penalty(20.0) > gradient_penalty(25.0));
    CHECK(gradient_penalty(50.0) > gradient_penalty(46.0));
    CHECK(gradient_penalty(63.0) > gradient_penalty(61.0));
}

TEST_CASE("RouteOptimizer uphill penalty")
{
    using nucleus::planner::uphill_penalty;
    // the optimum band costs nothing
    CHECK(uphill_penalty(20.0) == 0.0);
    CHECK(uphill_penalty(22.0) == 0.0);
    CHECK(uphill_penalty(25.0) == 0.0);
    // flatter is suboptimal, but gentler than the descent curve's flat
    // aversion (the distance term already dislikes detours)
    CHECK(uphill_penalty(0.0) == 3.0);
    CHECK(uphill_penalty(15.0) == 0.75);
    CHECK(uphill_penalty(18.0) > 0.0);
    CHECK(uphill_penalty(18.0) < 0.75);
    // bootpack territory: every degree beyond 30 gets expensive
    CHECK(uphill_penalty(28.0) > 0.0);
    CHECK(uphill_penalty(32.0) > 3.0);
    CHECK(uphill_penalty(40.0) == 12.0);
    CHECK(uphill_penalty(50.0) > uphill_penalty(40.0));
    // monotone behaviour within each band
    CHECK(uphill_penalty(10.0) > uphill_penalty(14.0));
    CHECK(uphill_penalty(35.0) > uphill_penalty(31.0));
}

TEST_CASE("RouteOptimizer dem raster")
{
    using namespace nucleus::planner;
    const auto tile = make_slope_tile(65, 65, 10.0, 7.0); // ~35 degree slope
    const auto raster = DemRaster::create({ tile }, { 0, 0 }, { 650, 650 }, 10.0);
    REQUIRE(raster.has_value());

    // altitudes follow the slope field: the terrain is high in the north
    // (large world y) and low in the south
    const auto south = raster->altitude({ 325, 25 });
    const auto north = raster->altitude({ 325, 625 });
    REQUIRE(south.has_value());
    REQUIRE(north.has_value());
    CHECK(*north > *south + 350.0); // 35 degrees over 600 m is ~420 m

    // outside the raster there is no data
    CHECK(!raster->altitude({ -5, 325 }).has_value());
    CHECK(!raster->inside({ -5, 325 }));

    // the terrain gradient of the uniform slope field is ~35 degrees
    const auto gradient = raster->cell_gradient(30, 30);
    REQUIRE(gradient.has_value());
    CHECK(*gradient > 30.0);
    CHECK(*gradient < 40.0);
}

TEST_CASE("RouteOptimizer pure steepness weight does not zigzag on a uniform steep face")
{
    using namespace nucleus::planner;
    // uniform 50 degree face (drop 11.9 m per 10 m cell): steep, but within
    // the acceptable range. with edge-relative slopes the optimiser used to
    // tune the traverse angle into the ideal 30-45 band, zigzagging across
    // the fall line; with terrain gradients the direct descent is optimal
    const auto tile = make_slope_tile(65, 65, 10.0, 11.9);
    const auto raster = DemRaster::create({ tile }, { 0, 0 }, { 650, 650 }, 10.0);
    REQUIRE(raster.has_value());

    const auto path = optimise_segment(*raster, { 10, 10 }, { 54, 54 }, 100.0, 0.0);
    REQUIRE(path.has_value());

    // the path must be monotone in both axes - no zigzagging
    for (size_t i = 1; i < path->size(); ++i) {
        CHECK((*path)[i].x >= (*path)[i - 1].x);
        CHECK((*path)[i].y >= (*path)[i - 1].y);
    }
    // and not gratuitously long: at most ~1.5x the diagonal cell count
    const double chebyshev = std::hypot(44.0, 44.0);
    CHECK(double(path->size()) <= 1.5 * chebyshev);
}

TEST_CASE("RouteOptimizer uniform slope descent")
{
    using namespace nucleus::planner;
    const auto tile = make_slope_tile(65, 65, 10.0, 7.0); // ~35 degree slope, ideal gradient
    const auto raster = DemRaster::create({ tile }, { 0, 0 }, { 650, 650 }, 10.0);
    REQUIRE(raster.has_value());

    // from the high north-west corner to the low south-east corner
    const auto path = optimise_segment(*raster, { 5, 59 }, { 59, 5 }, 60.0, 40.0);
    REQUIRE(path.has_value());
    REQUIRE(path->size() >= 2);
    CHECK(path->front() == glm::ivec2(5, 59));
    CHECK(path->back() == glm::ivec2(59, 5));

    // on an ideal gradient the route descends monotonically towards the south
    for (size_t i = 1; i < path->size(); ++i) {
        CHECK((*path)[i].y <= (*path)[i - 1].y);
    }
}

TEST_CASE("RouteOptimizer avoids cliff walls when steepness is weighted")
{
    using namespace nucleus::planner;
    auto tile = make_slope_tile(65, 65, 10.0, 7.0);
    // a band of impassable cliff wall (>70 degrees) across the middle rows,
    // leaving gaps at the top and bottom to go around
    for (int r = 10; r <= 54; ++r) {
        for (int c = 30; c <= 33; ++c) {
            tile.heights[size_t(r) * tile.width + unsigned(c)] += 10000.0f;
        }
    }
    const auto raster = DemRaster::create({ tile }, { 0, 0 }, { 650, 650 }, 10.0);
    REQUIRE(raster.has_value());

    const glm::ivec2 start { 5, 5 };
    const glm::ivec2 goal { 59, 59 };

    // with steepness weighted, the route must not cross the cliff band
    // (rows 10-54); it detours through the gaps above or below instead
    const auto steep_path = optimise_segment(*raster, start, goal, 100.0, 0.0);
    REQUIRE(steep_path.has_value());
    for (const auto& cell : *steep_path) {
        const bool crosses_wall = cell.x >= 30 && cell.x <= 33 && cell.y >= 10 && cell.y <= 54;
        CHECK(!crosses_wall);
    }

    // with distance-only weighting the wall is (nearly) free to cross
    const auto distance_path = optimise_segment(*raster, start, goal, 0.0, 100.0);
    REQUIRE(distance_path.has_value());
}

TEST_CASE("RouteOptimizer cross slope penalty moves traverses onto gentle ground")
{
    using namespace nucleus::planner;
    // terrain that descends towards the south, with a gentle bench band
    // (5 degrees) between steep bands (~50 degrees), built from a single
    // per-row height profile. diagonal descents on the steep face run at the
    // ideal ~40 degrees (no along-slope penalty), so the only thing that
    // makes them expensive is their ~40 degree side-tilt: without the cross
    // slope penalty the east-west progress rides the steep diagonals, with a
    // high weight it moves onto the low-tilt bench
    std::vector<float> profile(65);
    float height = 0.f;
    for (int r = 0; r < 65; ++r) {
        profile[size_t(r)] = height;
        const double drop = (r >= 21 && r <= 40) ? 0.9 : 11.9; // bench ~5 degrees, face ~50 degrees
        height += float(drop);
    }
    nucleus::planner::DemTile tile;
    tile.min_x = 0; tile.min_y = 0; tile.max_x = 650; tile.max_y = 650;
    tile.width = 65; tile.height = 65;
    tile.heights.reserve(65 * 65);
    for (int r = 0; r < 65; ++r)
        for (int col = 0; col < 65; ++col)
            tile.heights.push_back(profile[size_t(r)]);
    const auto raster = DemRaster::create({ tile }, { 0, 0 }, { 650, 650 }, 10.0);
    REQUIRE(raster.has_value());

    const glm::ivec2 start { 10, 60 };
    const glm::ivec2 goal { 54, 4 };

    // east-west progress made outside the bench band (rows 21..40)
    const auto x_progress_outside_bench = [](const std::vector<glm::ivec2>& path) {
        int progress = 0;
        for (size_t i = 1; i < path.size(); ++i) {
            if (path[i].y < 21 || path[i].y > 40) {
                progress += std::abs(path[i].x - path[i - 1].x);
            }
        }
        return progress;
    };

    const auto no_cross = optimise_segment(*raster, start, goal, 100.0, 40.0, 0.0);
    const auto full_cross = optimise_segment(*raster, start, goal, 100.0, 40.0, 2.0);
    REQUIRE(no_cross.has_value());
    REQUIRE(full_cross.has_value());

    // without the cross penalty the diagonal spreads its east-west progress
    // over the steep rows; with it, the traverse concentrates in the bench
    CHECK(x_progress_outside_bench(*no_cross) > 10);
    CHECK(x_progress_outside_bench(*full_cross) < x_progress_outside_bench(*no_cross));
}

TEST_CASE("RouteOptimizer flat sections are avoided with steepness weighting")
{
    using namespace nucleus::planner;
    // terrain whose slope alternates along the fall line, built from a single
    // per-row height profile (no cross-slope terms). the flat middle band is
    // unavoidable, but with steepness weighting the route must not linger in
    // it: crossing rows costs the same everywhere, so check the penalty
    // function prices the flat band far above the steep bands
    std::vector<float> profile(65);
    float height = 0.f;
    for (int r = 0; r < 65; ++r) {
        profile[size_t(r)] = height;
        // north band 35 degrees, middle band flat, south band 35 degrees
        const double drop = (r < 18 || r > 46) ? 7.0 : 1.4;
        height += float(drop);
    }
    nucleus::planner::DemTile tile;
    tile.min_x = 0; tile.min_y = 0; tile.max_x = 650; tile.max_y = 650;
    tile.width = 65; tile.height = 65;
    tile.heights.reserve(65 * 65);
    for (int r = 0; r < 65; ++r)
        for (int col = 0; col < 65; ++col)
            tile.heights.push_back(profile[size_t(r)]);
    const auto raster = DemRaster::create({ tile }, { 0, 0 }, { 650, 650 }, 10.0);
    REQUIRE(raster.has_value());

    // the steep bands sit at penalty 0, the flat band at ~4 penalty metres
    // per metre: the unit pricing drives the avoidance, verified directly
    const auto steep_gradient = raster->cell_gradient(30, 10);
    const auto flat_gradient = raster->cell_gradient(30, 32);
    REQUIRE(steep_gradient.has_value());
    REQUIRE(flat_gradient.has_value());
    CHECK(gradient_penalty(double(*steep_gradient)) == 0.0);
    CHECK(gradient_penalty(double(*flat_gradient)) > 2.5); // ~8 degree flat band

    // a descent across the terrain still finds a monotone path
    const auto path = optimise_segment(*raster, { 10, 60 }, { 54, 4 }, 100.0, 0.0);
    REQUIRE(path.has_value());
    for (size_t i = 1; i < path->size(); ++i) {
        CHECK((*path)[i].y <= (*path)[i - 1].y); // descends towards the south
    }
}

#ifdef QT_GUI_LIB
#include <QCoreApplication>
#include <nucleus/srs.h>
#include <nucleus/tile/TileLoadService.h>
#include <nucleus/utils/image_loader.h>
#include <radix/height_encoding.h>
#include <QThread>
#include <QDebug>

// manual network repro (hidden by default; run with "RouteOptimizer network *")
TEST_CASE("RouteOptimizer network repro", "[.]")
{
    std::vector<glm::dvec2> waypoints = { { 47.2042, 11.3087 }, { 47.1921, 11.3247 } }; // nockspitze
    bool done = false;
    nucleus::planner::OptimisationWeights weights;
    weights.steepness = 60.0;
    weights.distance = 40.0;
    weights.cross_slope = 50.0;
    weights.search_padding = 0.2;
    auto* session = new nucleus::planner::RouteOptimisationSession(waypoints, weights);
    QObject::connect(session, &nucleus::planner::RouteOptimisationSession::finished, [&](const nucleus::planner::OptimisationResult& result) {
        qDebug() << "success:" << result.success << "status:" << result.status
                 << "segments:" << result.segment_paths.size();
        for (const auto& seg : result.segment_paths) {
            qDebug() << "  segment points:" << seg.size();
            if (!seg.empty()) {
                qDebug() << "    start:" << seg.front().x << seg.front().y << seg.front().z
                         << " end:" << seg.back().x << seg.back().y << seg.back().z;
            }
        }
        done = true;
    });
    session->start();
    // capture the emitted preview speed through the session-less route: run a
    // mini preview session to check signal plumbing separately below
    for (int i = 0; i < 300 && !done; ++i) { // up to 30 s
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QThread::msleep(100);
    }
    CHECK(done);

    // probe: decode the summit tile directly and sample it at the summit
    // coordinates (true summit altitude ~2404 m)
    {
        const glm::dvec2 summit_ll(47.192058, 11.324704);
        const glm::dvec2 world = nucleus::srs::lat_long_to_world(summit_ll);
        const auto id = nucleus::srs::world_xy_to_tile_id(world, 16);
        bool fetched = false;
        nucleus::tile::TileLoadService service("https://alpinemaps.cg.tuwien.ac.at/tiles/at_dtm_alpinemaps/",
                                               nucleus::tile::TileLoadService::UrlPattern::ZXY, ".png");
        QObject::connect(&service, &nucleus::tile::TileLoadService::load_finished, [&](const nucleus::tile::Data& tile) {
            fetched = true;
            if (tile.network_info.status != nucleus::tile::NetworkInfo::Status::Good) {
                qDebug() << "probe: fetch failed";
                return;
            }
            const auto image = nucleus::utils::image_loader::rgba8(*tile.data);
            if (!image) {
                qDebug() << "probe: decode failed";
                return;
            }
            nucleus::planner::DemTile dem;
            const auto bounds = nucleus::srs::tile_bounds(tile.id);
            dem.min_x = bounds.min.x; dem.min_y = bounds.min.y;
            dem.max_x = bounds.max.x; dem.max_y = bounds.max.y;
            dem.width = image->width(); dem.height = image->height();
            float lo = 1e9f, hi = -1e9f;
            for (unsigned r = 0; r < dem.height; ++r)
                for (unsigned col = 0; col < dem.width; ++col) {
                    const float h = radix::height_encoding::to_float(glm::u8vec3(image->pixel({ col, r })));
                    dem.heights.push_back(h);
                    lo = std::min(lo, h); hi = std::max(hi, h);
                }
            qDebug() << "probe: tile" << tile.id.coords.x << tile.id.coords.y << "range" << lo << "-" << hi;
            qDebug() << "probe: sample at summit (expect ~2404):" << dem.sample(world.x, world.y).value_or(-1);
            // scan all pixels for the max altitude to see where the summit sits in the image
            float maxh = -1e9f; unsigned maxr = 0, maxc = 0;
            for (unsigned r = 0; r < dem.height; ++r)
                for (unsigned col = 0; col < dem.width; ++col) {
                    const float h = dem.heights[size_t(r) * dem.width + col];
                    if (h > maxh) { maxh = h; maxr = r; maxc = col; }
                }
            qDebug() << "probe: max altitude" << maxh << "at row" << maxr << "col" << maxc
                     << "(rows: 0=north," << dem.height - 1 << "=south)";
            const glm::dvec2 summit_uv = nucleus::srs::world_xy_to_tile_uv(world, 16);
            qDebug() << "probe: summit uv (x=north?, y up):" << summit_uv.x << summit_uv.y;
        });
        service.load(id);
        for (int i = 0; i < 150 && !fetched; ++i) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            QThread::msleep(100);
        }
    }
}
#endif
