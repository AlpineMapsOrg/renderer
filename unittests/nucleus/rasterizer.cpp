/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Adam Celarek
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

#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>

#include <CDT.h>

#include "nucleus/Raster.h"
// #include "nucleus/tile/conversion.h"
#include "nucleus/utils/rasterizer.h"

#include <radix/geometry.h>

TEST_CASE("nucleus/rasterizer")
{
    // NOTE: most tests compare the dda renderer with the sdf renderer
    // unfortunatly the sdf renderer can produce false positives (writing to cell that should be empty if near edge)
    // those errors have all been subverted by choosing parameters that work with both renderers
    // but future changes might cause problems, when there shouldn't be problems
    // so if a test fails check the debug images to see what exactly went wrong.

    SECTION("Triangulation")
    {
        // 5 point polygon
        // basically a square where one side contains an inward facing triangle
        // a triangulation algorithm should be able to discern that 3 triangles are needed to construct this shape
        const std::vector<glm::vec2> points = { glm::vec2(0, 0), glm::vec2(1, 1), glm::vec2(0, 2), glm::vec2(2, 2), glm::vec2(2, 0) };
        const std::vector<glm::ivec2> edges = { glm::ivec2(0, 1), glm::ivec2(1, 2), glm::ivec2(2, 3), glm::ivec2(3, 4), glm::ivec2(4, 0) };

        CDT::Triangulation<double> cdt;
        cdt.insertVertices(points.begin(), points.end(), [](const glm::vec2& p) { return p.x; }, [](const glm::vec2& p) { return p.y; });
        cdt.insertEdges(edges.begin(), edges.end(), [](const glm::ivec2& p) { return p.x; }, [](const glm::ivec2& p) { return p.y; });
        cdt.eraseOuterTrianglesAndHoles();

        auto tri = cdt.triangles;
        auto vert = cdt.vertices;

        // check if only 3 triangles have been found
        CHECK(tri.size() == 3);

        // 1st triangle
        CHECK(vert[tri[0].vertices[0]].x == 0.0);
        CHECK(vert[tri[0].vertices[0]].y == 2.0);
        CHECK(vert[tri[0].vertices[1]].x == 1.0);
        CHECK(vert[tri[0].vertices[1]].y == 1.0);
        CHECK(vert[tri[0].vertices[2]].x == 2.0);
        CHECK(vert[tri[0].vertices[2]].y == 2.0);

        // 2nd triangle
        CHECK(vert[tri[1].vertices[0]].x == 1.0);
        CHECK(vert[tri[1].vertices[0]].y == 1.0);
        CHECK(vert[tri[1].vertices[1]].x == 2.0);
        CHECK(vert[tri[1].vertices[1]].y == 0.0);
        CHECK(vert[tri[1].vertices[2]].x == 2.0);
        CHECK(vert[tri[1].vertices[2]].y == 2.0);

        // 3rd triangle
        CHECK(vert[tri[2].vertices[0]].x == 1.0);
        CHECK(vert[tri[2].vertices[0]].y == 1.0);
        CHECK(vert[tri[2].vertices[1]].x == 0.0);
        CHECK(vert[tri[2].vertices[1]].y == 0.0);
        CHECK(vert[tri[2].vertices[2]].x == 2.0);
        CHECK(vert[tri[2].vertices[2]].y == 0.0);

        // DEBUG print out all the points of the triangles (to check what might have went wrong)
        // for (std::size_t i = 0; i < tri.size(); i++) {
        //     printf("Triangle points: [[%f, %f], [%f, %f], [%f, %f]]\n",
        //         vert[tri[i].vertices[0]].x, // x0
        //         vert[tri[i].vertices[0]].y, // y0
        //         vert[tri[i].vertices[1]].x, // x1
        //         vert[tri[i].vertices[1]].y, // y1
        //         vert[tri[i].vertices[2]].x, // x2
        //         vert[tri[i].vertices[2]].y // y2
        //     );
        // }
    }

    SECTION("Triangle y ordering")
    {
        // make sure that the triangle_order function correctly orders the triangle points from lowest y to highest y value
        const std::vector<glm::vec2> triangle_points_012 = { { glm::vec2(30, 10), glm::vec2(10, 30), glm::vec2(50, 50) } };
        const std::vector<glm::vec2> triangle_points_021 = { glm::vec2(30, 10), glm::vec2(50, 50), glm::vec2(10, 30) };
        const std::vector<glm::vec2> triangle_points_102 = { glm::vec2(10, 30), glm::vec2(30, 10), glm::vec2(50, 50) };
        const std::vector<glm::vec2> triangle_points_201 = { glm::vec2(10, 30), glm::vec2(50, 50), glm::vec2(30, 10) };
        const std::vector<glm::vec2> triangle_points_120 = { glm::vec2(50, 50), glm::vec2(30, 10), glm::vec2(10, 30) };
        const std::vector<glm::vec2> triangle_points_210 = { glm::vec2(50, 50), glm::vec2(10, 30), glm::vec2(30, 10) };

        const std::vector<glm::vec2> correct = { { glm::vec2(30, 10), glm::vec2(10, 30), glm::vec2(50, 50) } };

        CHECK(correct == nucleus::utils::rasterizer::triangulize(triangle_points_012));
        CHECK(correct == nucleus::utils::rasterizer::triangulize(triangle_points_021));
        CHECK(correct == nucleus::utils::rasterizer::triangulize(triangle_points_102));
        CHECK(correct == nucleus::utils::rasterizer::triangulize(triangle_points_201));
        CHECK(correct == nucleus::utils::rasterizer::triangulize(triangle_points_120));
        CHECK(correct == nucleus::utils::rasterizer::triangulize(triangle_points_210));
    }

    SECTION("rasterize triangle")
    {
        // two triangles
        const std::vector<glm::vec2> triangles = { glm::vec2(30.5, 10.5), glm::vec2(10.5, 30.5), glm::vec2(50.5, 50.5), glm::vec2(5.5, 5.5), glm::vec2(15.5, 10.5), glm::vec2(5.5, 15.5) };

        nucleus::Raster<uint8_t> output({ 64, 64 }, 0u);
        const auto pixel_writer = [&output](glm::vec2 pos, int) { output.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles);

        nucleus::Raster<uint8_t> output2({ 64, 64 }, 0u);
        const auto pixel_writer2 = [&output2](glm::vec2 pos, int) { output2.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle_sdf(pixel_writer2, triangles, 0);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output.png"));
    }

    SECTION("rasterize triangle small")
    {
        const std::vector<glm::vec2> triangles_small = { glm::vec2(2, 2), glm::vec2(1, 3), glm::vec2(3.8, 3.8) };
        nucleus::Raster<uint8_t> output({ 6, 6 }, 0u);
        const auto pixel_writer = [&output](glm::vec2 pos, int) { output.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles_small);

        nucleus::Raster<uint8_t> output2({ 6, 6 }, 0u);
        const auto pixel_writer2 = [&output2](glm::vec2 pos, int) { output2.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle_sdf(pixel_writer2, triangles_small, 0);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_small.png"));
    }

    SECTION("rasterize triangle smallest")
    {
        // less than one pixel
        const std::vector<glm::vec2> triangles_smallest = { glm::vec2(30.4, 30.4), glm::vec2(30.8, 30.6), glm::vec2(30.8, 30.8) };
        nucleus::Raster<uint8_t> output({ 64, 64 }, 0u);
        const auto pixel_writer = [&output](glm::vec2 pos, int) { output.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles_smallest);

        nucleus::Raster<uint8_t> output2({ 64, 64 }, 0u);
        const auto pixel_writer2 = [&output2](glm::vec2 pos, int) { output2.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle_sdf(pixel_writer2, triangles_smallest, 0);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_smallest.png"));
    }

    SECTION("rasterize triangle enlarged endcaps only")
    {
        // less than one pixel
        const std::vector<glm::vec2> triangles = { glm::vec2(5.35, 5.35), glm::vec2(5.40, 5.40), glm::vec2(5.45, 5.35) };
        auto size = glm::vec2(10, 10);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 4.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::details::add_circle_end_cap(pixel_writer, triangles[0], 1, distance);
        nucleus::utils::rasterizer::details::add_circle_end_cap(pixel_writer, triangles[1], 1, distance);
        nucleus::utils::rasterizer::details::add_circle_end_cap(pixel_writer, triangles[2], 1, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle_sdf(pixel_writer2, triangles, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_enlarged_endcap.png"));
    }

    SECTION("rasterize triangle enlarged")
    {
        const std::vector<glm::vec2> triangles = { glm::vec2(30.5, 10.5),
            glm::vec2(10.5, 30.5),
            glm::vec2(50.5, 50.5),

            glm::vec2(5.5, 5.5),
            glm::vec2(15.5, 10.5),
            glm::vec2(5.5, 15.5) };
        // const std::vector<glm::vec2> triangles = { glm::vec2(5.35, 5.35), glm::vec2(5.40, 5.40), glm::vec2(5.45, 5.35) };
        auto size = glm::vec2(64, 64);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 5.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle_sdf(pixel_writer2, triangles, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_enlarged.png"));
    }

    SECTION("rasterize triangle horizontal")
    {
        const std::vector<glm::vec2> triangles = { glm::vec2(30.5, 10.5), glm::vec2(45.5, 45.5), glm::vec2(10.5, 45.5) };
        auto size = glm::vec2(64, 64);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 4.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle_sdf(pixel_writer2, triangles, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_enlarged_horizontal.png"));
    }

    // can't be verified exactly, since SDF is currently slightly inaccurate
    // SECTION("rasterize triangle narrow")
    // {
    //     const std::vector<glm::vec2> triangles = { glm::vec2(6.5, 16.5), glm::vec2(55.5, 18.5), glm::vec2(6.5, 20.5), glm::vec2(55.5, 46.5), glm::vec2(6.5, 48.5), glm::vec2(55.5, 50.5) };
    //     auto size = glm::vec2(64, 64);
    //     nucleus::Raster<uint8_t> output(size, 0u);
    //     float distance = 5.0;
    //     radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

    //     const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
    //         if (bounds.contains(pos))
    //             output.pixel(pos) = 255;
    //     };
    //     nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, distance);

    //     nucleus::Raster<uint8_t> output2(size, 0u);
    //     const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
    //         if (bounds.contains(pos))
    //             output2.pixel(pos) = 255;
    //     };
    //     nucleus::utils::rasterizer::rasterize_triangle_sdf(pixel_writer2, triangles, distance);

    //     CHECK(output.buffer() == output2.buffer());

    //     // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
    //     auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
    //     image.save(QString("rasterizer_output_enlarged_narrow.png"));
    // }

    SECTION("rasterize triangle start/end y")
    {
        const std::vector<glm::vec2> triangles = { // down to right
            glm::vec2(2.7, 1.5),
            glm::vec2(3.7, 2.5),
            glm::vec2(3.7, 2.7),
            // down to left
            glm::vec2(8.7, 1.5),
            glm::vec2(7.7, 2.5),
            glm::vec2(7.7, 2.7),

            // 1 lines
            // down to right
            glm::vec2(3.5, 7.3),
            glm::vec2(26.5, 7.5),
            glm::vec2(24.5, 7.6),
            // down to left
            glm::vec2(26.5, 12.3),
            glm::vec2(3.5, 12.5),
            glm::vec2(5.5, 12.6),

            // 2 lines
            // down to right
            glm::vec2(3.5, 16.7),
            glm::vec2(6.5, 17.5),
            glm::vec2(4.5, 17.6),
            // down to left
            glm::vec2(6.5, 21.7),
            glm::vec2(3.5, 22.5),
            glm::vec2(5.5, 22.6)
        };

        auto size = glm::vec2(30, 30);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 0.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle_sdf(pixel_writer2, triangles, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_triangle_start_end.png"));
    }

    SECTION("rasterize line start/end y")
    {
        const std::vector<glm::vec2> line_lower_min = { glm::vec2(2.7, 2.5), glm::vec2(3.7, 1.5) };
        const std::vector<glm::vec2> line_upper_min = { glm::vec2(7.3, 2.5), glm::vec2(8.5, 1.3) };
        const std::vector<glm::vec2> line_left_to_right_1pixel = { glm::vec2(2.3, 7.3), glm::vec2(28.5, 7.7) };
        const std::vector<glm::vec2> line_right_to_left_1pixel = { glm::vec2(2.3, 12.7), glm::vec2(28.5, 12.3) };
        const std::vector<glm::vec2> line_left_to_right_2pixel = { glm::vec2(2.3, 17.3), glm::vec2(6.5, 18.7) };
        const std::vector<glm::vec2> line_right_to_left_2pixel = { glm::vec2(2.3, 23.7), glm::vec2(6.5, 22.3) };

        auto size = glm::vec2(30, 30);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 0.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line_lower_min, distance);
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line_upper_min, distance);
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line_left_to_right_1pixel, distance);
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line_right_to_left_1pixel, distance);
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line_left_to_right_2pixel, distance);
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line_right_to_left_2pixel, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line_lower_min, distance);
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line_upper_min, distance);
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line_left_to_right_1pixel, distance);
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line_right_to_left_1pixel, distance);
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line_left_to_right_2pixel, distance);
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line_right_to_left_2pixel, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_line_start_end.png"));
    }

    SECTION("rasterize line straight")
    {
        const std::vector<glm::vec2> line = { glm::vec2(10.5, 10.5), glm::vec2(10.5, 50.5), glm::vec2(50.5, 50.5), glm::vec2(50.5, 10.5), glm::vec2(10.5, 10.5) };
        auto size = glm::vec2(64, 64);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 0.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_line_straight.png"));
    }

    SECTION("rasterize line diagonal")
    {
        const std::vector<glm::vec2> line = { glm::vec2(30.5, 10.5), glm::vec2(50.5, 30.5), glm::vec2(30.5, 50.5), glm::vec2(10.5, 30.5), glm::vec2(30.5, 10.5) };
        auto size = glm::vec2(64, 64);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 0.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_line_diagonal.png"));
    }

    SECTION("rasterize line straight enlarged")
    {
        const std::vector<glm::vec2> line = { glm::vec2(10.5, 10.5), glm::vec2(10.5, 50.5), glm::vec2(50.5, 50.5), glm::vec2(50.5, 10.5), glm::vec2(10.5, 10.5) };
        auto size = glm::vec2(64, 64);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 2.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_line_straight_enlarged.png"));
    }

    SECTION("rasterize line diagonal enlarged")
    {
        const std::vector<glm::vec2> line = { glm::vec2(30.5, 10.5), glm::vec2(50.5, 30.5), glm::vec2(30.5, 50.5), glm::vec2(10.5, 30.5), glm::vec2(30.5, 10.5) };
        // const std::vector<glm::vec2> line = { glm::vec2(30.5, 10.5), glm::vec2(50.5, 30.5) };
        auto size = glm::vec2(64, 64);
        nucleus::Raster<uint8_t> output(size, 0u);
        float distance = 2.0;
        radix::geometry::Aabb2<double> bounds = { { 0, 0 }, size };

        const auto pixel_writer = [&output, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::vec2 pos, int) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line_sdf(pixel_writer2, line, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_line_diagonal_enlarged.png"));
    }
}
