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
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <CDT.h>

#include "nucleus/Raster.h"
// #include "nucleus/tile/conversion.h"
#include "nucleus/utils/rasterizer.h"

#include <radix/geometry.h>

/*
 * calculates how far away the given position is from a triangle
 * uses distance to shift the current triangle distance
 * if it is within the triangle, the pixel writer function is called
 *  * uses triangle distance calculation from: https://iquilezles.org/articles/distfunctions2d/
 */
template <typename PixelWriterFunction>
void triangle_sdf(const PixelWriterFunction& pixel_writer,
    const glm::vec2 current_position,
    const std::array<glm::vec2, 3> points,
    const std::array<glm::vec2, 3> edges,
    float s,
    unsigned int data_index,
    float distance)
{
    glm::vec2 v0 = current_position - points[0], v1 = current_position - points[1], v2 = current_position - points[2];

    // pq# = distance from edge #
    glm::vec2 pq0 = v0 - edges[0] * glm::clamp(glm::dot(v0, edges[0]) / glm::dot(edges[0], edges[0]), 0.0f, 1.0f);
    glm::vec2 pq1 = v1 - edges[1] * glm::clamp(glm::dot(v1, edges[1]) / glm::dot(edges[1], edges[1]), 0.0f, 1.0f);
    glm::vec2 pq2 = v2 - edges[2] * glm::clamp(glm::dot(v2, edges[2]) / glm::dot(edges[2], edges[2]), 0.0f, 1.0f);
    // d.x == squared distance to triangle edge/vertice
    // d.y == are we inside or outside triangle
    glm::vec2 d = min(min(glm::vec2(dot(pq0, pq0), s * (v0.x * edges[0].y - v0.y * edges[0].x)), glm::vec2(dot(pq1, pq1), s * (v1.x * edges[1].y - v1.y * edges[1].x))),
        glm::vec2(dot(pq2, pq2), s * (v2.x * edges[2].y - v2.y * edges[2].x)));
    float dist_from_tri = -sqrt(d.x) * glm::sign(d.y);

    if (dist_from_tri < distance) {
        nucleus::utils::rasterizer::details::invokePixelWriter(pixel_writer, current_position, data_index);
    }
}

/*
 * calculates how far away the given position is from a line segment
 * uses distance to shift the current triangle distance
 * if it is within the triangle, the pixel writer function is called
 *  * uses line segment distance calculation from: https://iquilezles.org/articles/distfunctions2d/
 */
template <nucleus::utils::rasterizer::PixelWriterFunctionConcept PixelWriterFunction>
void line_sdf(const PixelWriterFunction& pixel_writer, const glm::vec2 current_position, const glm::vec2 origin, glm::vec2 edge, unsigned int data_index, float distance)
{
    glm::vec2 v0 = current_position - origin;

    float dist_from_line = length(v0 - edge * glm::clamp(glm::dot(v0, edge) / glm::dot(edge, edge), 0.0f, 1.0f));

    if (dist_from_line < distance) {
        nucleus::utils::rasterizer::details::invokePixelWriter(pixel_writer, current_position, data_index);
    }
}

/*
 * ideal if you want to rasterize only a few triangles, where every triangle covers a large part of the raster size
 * in this method we traverse through every triangle and generate the bounding box and traverse the bounding box
 */
template <typename PixelWriterFunction> void rasterize_triangle_sdf(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangles, float distance)
{
    // we sample from the center
    // and have a radius of a half pixel diagonal
    // NOTICE: this still leads to false positives if the edge of a triangle is slighly in another pixel without every comming in this pixel!!
    distance += sqrt(0.5);

    for (size_t i = 0; i < triangles.size() / 3; ++i) {
        const std::array<glm::vec2, 3> points = { triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2] };
        const std::array<glm::vec2, 3> edges = { points[1] - points[0], points[2] - points[1], points[0] - points[2] };

        auto min_bound = min(min(points[0], points[1]), points[2]) - glm::vec2(distance);
        auto max_bound = max(max(points[0], points[1]), points[2]) + glm::vec2(distance);

        float s = glm::sign(edges[0].x * edges[2].y - edges[0].y * edges[2].x);

        for (size_t x = min_bound.x; x < max_bound.x; x++) {
            for (size_t y = min_bound.y; y < max_bound.y; y++) {
                auto current_position = glm::vec2 { x + 0.5, y + 0.5 };

                triangle_sdf(pixel_writer, current_position, points, edges, s, i, distance);
            }
        }
    }
}

template <typename PixelWriterFunction> void rasterize_line_sdf(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& line_points, float distance)
{
    // we sample from the center
    // and have a radius of a half pixel diagonal
    // NOTICE: this still leads to false positives if the edge of a triangle is slighly in another pixel without every comming in this pixel!!
    distance += sqrt(0.5);

    for (size_t i = 0; i < line_points.size() - 1; ++i) {
        const std::array<glm::vec2, 2> points = { line_points[i + 0], line_points[i + 1] };
        auto edge = points[1] - points[0];

        auto min_bound = min(points[0], points[1]) - glm::vec2(distance);
        auto max_bound = max(points[0], points[1]) + glm::vec2(distance);

        for (size_t x = min_bound.x; x < max_bound.x; x++) {
            for (size_t y = min_bound.y; y < max_bound.y; y++) {
                auto current_position = glm::vec2 { x + 0.5, y + 0.5 };

                line_sdf(pixel_writer, current_position, points[0], edge, i, distance);
            }
        }
    }
}

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

    SECTION("rasterize Polygon")
    {

        const std::vector<glm::vec2> polygon_points = {
            glm::vec2(10.5, 10.5),
            glm::vec2(30.5, 10.5),
            glm::vec2(50.5, 50.5),
            glm::vec2(10.5, 30.5),
        };

        nucleus::Raster<uint8_t> output({ 64, 64 }, 0u);
        const auto pixel_writer = [&output](glm::ivec2 pos) { output.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_polygon(pixel_writer, polygon_points);

        const auto triangles = nucleus::utils::rasterizer::triangulize(polygon_points);
        nucleus::Raster<uint8_t> output2({ 64, 64 }, 0u);
        auto pixel_writer2 = [&output2](glm::ivec2 pos) { output2.pixel(pos) = 255; };
        rasterize_triangle_sdf(pixel_writer2, triangles, 0);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_polygon.png"));
    }

    SECTION("rasterize triangle")
    {
        // two triangles
        const std::vector<glm::vec2> triangles = { glm::vec2(30.5, 10.5), glm::vec2(10.5, 30.5), glm::vec2(50.5, 50.5), glm::vec2(5.5, 5.5), glm::vec2(15.5, 10.5), glm::vec2(5.5, 15.5) };

        nucleus::Raster<uint8_t> output({ 64, 64 }, 0u);
        const auto pixel_writer = [&output](glm::ivec2 pos) { output.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles);

        nucleus::Raster<uint8_t> output2({ 64, 64 }, 0u);
        auto pixel_writer2 = [&output2](glm::ivec2 pos) { output2.pixel(pos) = 255; };
        rasterize_triangle_sdf(pixel_writer2, triangles, 0);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output.png"));
    }

    SECTION("rasterize triangle small")
    {
        const std::vector<glm::vec2> triangles_small = { glm::vec2(2, 2), glm::vec2(1, 3), glm::vec2(3.8, 3.8) };
        nucleus::Raster<uint8_t> output({ 6, 6 }, 0u);
        const auto pixel_writer = [&output](glm::ivec2 pos) { output.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles_small);

        nucleus::Raster<uint8_t> output2({ 6, 6 }, 0u);
        const auto pixel_writer2 = [&output2](glm::ivec2 pos) { output2.pixel(pos) = 255; };
        rasterize_triangle_sdf(pixel_writer2, triangles_small, 0);

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
        const auto pixel_writer = [&output](glm::ivec2 pos) { output.pixel(pos) = 255; };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles_smallest);

        nucleus::Raster<uint8_t> output2({ 64, 64 }, 0u);
        const auto pixel_writer2 = [&output2](glm::ivec2 pos) { output2.pixel(pos) = 255; };
        rasterize_triangle_sdf(pixel_writer2, triangles_smallest, 0);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::details::add_circle_end_cap(pixel_writer, triangles[0], 1, distance);
        nucleus::utils::rasterizer::details::add_circle_end_cap(pixel_writer, triangles[1], 1, distance);
        nucleus::utils::rasterizer::details::add_circle_end_cap(pixel_writer, triangles[2], 1, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_triangle_sdf(pixel_writer2, triangles, distance);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_triangle_sdf(pixel_writer2, triangles, distance);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_triangle_sdf(pixel_writer2, triangles, distance);

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

    //     const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
    //         if (bounds.contains(pos))
    //             output.pixel(pos) = 255;
    //     };
    //     nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, distance);

    //     nucleus::Raster<uint8_t> output2(size, 0u);
    //     const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
    //         if (bounds.contains(pos))
    //             output2.pixel(pos) = 255;
    //     };
    //     rasterize_triangle_sdf(pixel_writer2, triangles, distance);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_triangle_sdf(pixel_writer2, triangles, distance);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
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
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_line_sdf(pixel_writer2, line_lower_min, distance);
        rasterize_line_sdf(pixel_writer2, line_upper_min, distance);
        rasterize_line_sdf(pixel_writer2, line_left_to_right_1pixel, distance);
        rasterize_line_sdf(pixel_writer2, line_right_to_left_1pixel, distance);
        rasterize_line_sdf(pixel_writer2, line_left_to_right_2pixel, distance);
        rasterize_line_sdf(pixel_writer2, line_right_to_left_2pixel, distance);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_line_sdf(pixel_writer2, line, distance);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_line_sdf(pixel_writer2, line, distance);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_line_sdf(pixel_writer2, line, distance);

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

        const auto pixel_writer = [&output, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output.pixel(pos) = 255;
        };
        nucleus::utils::rasterizer::rasterize_line(pixel_writer, line, distance);

        nucleus::Raster<uint8_t> output2(size, 0u);
        const auto pixel_writer2 = [&output2, bounds](glm::ivec2 pos) {
            if (bounds.contains(pos))
                output2.pixel(pos) = 255;
        };
        rasterize_line_sdf(pixel_writer2, line, distance);

        CHECK(output.buffer() == output2.buffer());

        // DEBUG: save image (image saved to build/Desktop-Profile/unittests/nucleus)
        // auto image = nucleus::tile::conversion::u8raster_2_to_qimage(output, output2);
        // image.save(QString("rasterizer_output_line_diagonal_enlarged.png"));
    }
}
TEST_CASE("nucleus/utils/rasterizer benchmarks")
{
    BENCHMARK("triangulize polygons")
    {
        const std::vector<glm::vec2> polygon_points = { glm::vec2(10.5, 10.5), glm::vec2(30.5, 10.5), glm::vec2(50.5, 50.5), glm::vec2(10.5, 30.5) };
        nucleus::utils::rasterizer::triangulize(polygon_points);
    };

    BENCHMARK("Rasterize triangle")
    {
        const std::vector<glm::vec2> triangles = { glm::vec2(30.5, 10.5), glm::vec2(10.5, 30.5), glm::vec2(50.5, 50.5), glm::vec2(5.5, 5.5), glm::vec2(15.5, 10.5), glm::vec2(5.5, 15.5) };

        // we dont particular care how long it takes to write to raster -> so do nothing
        const auto pixel_writer = [](glm::ivec2) { /*do nothing*/ };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles);
    };

    BENCHMARK("Rasterize triangle distance")
    {
        const std::vector<glm::vec2> triangles = { glm::vec2(30.5, 10.5), glm::vec2(10.5, 30.5), glm::vec2(50.5, 50.5), glm::vec2(5.5, 5.5), glm::vec2(15.5, 10.5), glm::vec2(5.5, 15.5) };

        // we dont particular care how long it takes to write to raster -> so do nothing
        const auto pixel_writer = [](glm::ivec2) { /*do nothing*/ };
        nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangles, 5.0);
    };

    BENCHMARK("Rasterize triangle SDF")
    {
        const std::vector<glm::vec2> triangles = { glm::vec2(30.5, 10.5), glm::vec2(10.5, 30.5), glm::vec2(50.5, 50.5), glm::vec2(5.5, 5.5), glm::vec2(15.5, 10.5), glm::vec2(5.5, 15.5) };

        // we dont particular care how long it takes to write to raster -> so do nothing
        const auto pixel_writer = [](glm::ivec2) { /*do nothing*/ };
        rasterize_triangle_sdf(pixel_writer, triangles, 0);
    };

    BENCHMARK("Rasterize triangle SDF distance")
    {
        const std::vector<glm::vec2> triangles = { glm::vec2(30.5, 10.5), glm::vec2(10.5, 30.5), glm::vec2(50.5, 50.5), glm::vec2(5.5, 5.5), glm::vec2(15.5, 10.5), glm::vec2(5.5, 15.5) };

        // we dont particular care how long it takes to write to raster -> so do nothing
        const auto pixel_writer = [](glm::ivec2) { /*do nothing*/ };
        rasterize_triangle_sdf(pixel_writer, triangles, 5.0);
    };
}
