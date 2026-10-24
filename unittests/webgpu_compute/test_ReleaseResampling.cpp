/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <webgpu/compute/nodes/AvaFrameReleaseNode.h>
#include <webgpu/compute/nodes/AvaFrameSandboxNode.h>

#include <cmath>

using namespace webgpu_compute::nodes;
using Catch::Approx;

namespace {

/// A 100 x 80 cell dataset of 5 m cells with a 50 m x 50 m release of 2 m depth: cells 20-29 in
/// x and 30-39 in y counted from the south. 5000 m^3 on 2500 m^2.
SandboxTerrain test_terrain()
{
    SandboxTerrain t;
    t.name = "test";
    t.scenario = "rel";
    t.grid_size = glm::uvec2(100, 80);
    t.cellsize = 5.0;
    t.size_metres = glm::dvec2(500.0, 400.0);
    t.release_depth = nucleus::Raster<float>(t.grid_size, 0.0f);
    for (uint32_t y = 30; y < 40; y++)
        for (uint32_t x = 20; x < 30; x++)
            t.release_depth.pixel(glm::uvec2(x, t.grid_size.y - 1 - y)) = 2.0f; // row 0 is north
    t.release_volume = 5000.0;
    t.release_area = 2500.0;
    t.release_max_depth = 2.0f;
    return t;
}

/// world = origin + R(angle) * sandbox / metres_per_world_unit, as glm's column-major dmat3.
glm::dmat3 placement(glm::dvec2 origin, double angle_degrees, double metres_per_world_unit)
{
    const double a = glm::radians(angle_degrees);
    const double s = 1.0 / metres_per_world_unit;
    return glm::dmat3(glm::dvec3(std::cos(a) * s, std::sin(a) * s, 0.0), glm::dvec3(-std::sin(a) * s, std::cos(a) * s, 0.0), glm::dvec3(origin, 1.0));
}

radix::geometry::Aabb<2, double> region(glm::dvec2 min, glm::dvec2 max) { return { min, max }; }

} // namespace

TEST_CASE("webgpu_compute::resample_release")
{
    SandboxTerrain terrain = test_terrain();
    const glm::dvec2 origin(1.2e6, 5.9e6); // somewhere in Austria, in Web Mercator units

    SECTION("aligned grids keep volume, area and bounds exactly")
    {
        terrain.sandbox_to_world = placement(origin, 0.0, 1.0);
        const auto r = resample_release(terrain, region(origin, origin + glm::dvec2(500.0, 400.0)), 1.0, 5.0);
        CHECK(r.depth.width() == 100u);
        CHECK(r.depth.height() == 80u);
        CHECK(r.volume == Approx(5000.0));
        CHECK(r.area == Approx(2500.0));
        CHECK(r.max_depth == 2.0f);
        CHECK(r.min.x == Approx(100.0));
        CHECK(r.min.y == Approx(150.0));
        CHECK(r.max.x == Approx(150.0));
        CHECK(r.max.y == Approx(200.0));
        // Row 0 of the result is the northern edge, like the height texture.
        CHECK(r.depth.pixel(glm::uvec2(20, 80 - 1 - 30)) == 2.0f);
        CHECK(r.depth.pixel(glm::uvec2(20, 30)) == 0.0f);
    }

    SECTION("a region elsewhere shifts the release, not its size")
    {
        terrain.sandbox_to_world = placement(origin, 0.0, 1.0);
        const glm::dvec2 offset(-37.5, 12.5);
        const auto r = resample_release(terrain, region(origin + offset, origin + offset + glm::dvec2(600.0, 400.0)), 1.0, 5.0);
        CHECK(r.volume == Approx(5000.0).epsilon(0.05));
        CHECK(r.min.x == Approx(100.0 - offset.x).margin(5.0));
        CHECK(r.min.y == Approx(150.0 - offset.y).margin(5.0));
    }

    SECTION("Web Mercator units: metres are units times cos(latitude)")
    {
        const double metres_per_unit = std::cos(glm::radians(47.0));
        terrain.sandbox_to_world = placement(origin, 0.0, metres_per_unit);
        const glm::dvec2 size_units = glm::dvec2(500.0, 400.0) / metres_per_unit;
        const auto r = resample_release(terrain, region(origin, origin + size_units), metres_per_unit, 5.0);
        CHECK(r.volume == Approx(5000.0).epsilon(0.05));
        CHECK(r.area == Approx(2500.0).epsilon(0.05));
        CHECK(r.min.x == Approx(100.0).margin(5.0));
        CHECK(r.max.y == Approx(200.0).margin(5.0));
    }

    SECTION("a rotated placement keeps the volume")
    {
        // Austria Lambert and Web Mercator differ by a rotation of up to a few degrees; 15 is ample.
        terrain.sandbox_to_world = placement(origin, 15.0, 1.0);
        const auto r = resample_release(terrain, region(origin - glm::dvec2(200.0), origin + glm::dvec2(700.0)), 1.0, 5.0);
        CHECK(r.volume == Approx(5000.0).epsilon(0.05));
        CHECK(r.area == Approx(2500.0).epsilon(0.05));
        CHECK(r.max_depth == 2.0f);
    }

    SECTION("a coarser grid keeps the volume")
    {
        terrain.sandbox_to_world = placement(origin, 0.0, 1.0);
        const auto r = resample_release(terrain, region(origin, origin + glm::dvec2(500.0, 400.0)), 1.0, 12.5);
        CHECK(r.depth.width() == 40u);
        CHECK(r.volume == Approx(5000.0).epsilon(0.15));
    }

    SECTION("without a placement or outside the region there is no release")
    {
        const auto unplaced = resample_release(terrain, region(origin, origin + glm::dvec2(500.0, 400.0)), 1.0, 5.0);
        CHECK(unplaced.volume == 0.0);
        terrain.sandbox_to_world = placement(origin, 0.0, 1.0);
        const auto elsewhere = resample_release(terrain, region(origin + glm::dvec2(1000.0), origin + glm::dvec2(1500.0)), 1.0, 5.0);
        CHECK(elsewhere.volume == 0.0);
        CHECK(elsewhere.max_depth == 0.0f);
    }
}
