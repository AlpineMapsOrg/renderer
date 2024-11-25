/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Lucas Dworschak
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

#include <array>
#include <iostream>
#include <vector>

#include <glm/glm.hpp>

namespace nucleus::utils::rasterizer {

// Possible future improvements:
// - DDA -> prevent fill_inner_triangle to be called twice for the same row (if we switch from one line to another)
//      - also check overall how often each pixel is written to

// TODOs:
// - enlarge triangles
// - line renderer

namespace details {

    inline float get_x_for_y_on_line(const glm::vec2 origin, const glm::vec2 line, float y)
    {
        // pos_on_line = origin + t * line
        float t = (y - origin.y) / line.y;
        return origin.x + t * line.x;
    }

    inline std::pair<glm::vec2, int> calculate_dda_steps(const glm::vec2 line)
    {
        int steps = ceil(fabs(line.x) > fabs(line.y) ? fabs(line.x) : fabs(line.y));
        // should only be triggered if two vertices of a triangle are exactly on the same position
        // -> the input data is wrong
        assert(steps > 0);
        // steps = std::max(steps, 1); // alternative force at least one dda step -> wrong output but it should at least "work"

        auto step_size = glm::vec2(line.x / float(steps), line.y / float(steps));

        // double the steps -> otherwise we might miss important pixels
        steps *= 2;
        step_size /= 2.0f;

        return std::make_pair(step_size, steps);
    }

    template <typename PixelWriterFunction>
    inline void fill_inner_triangle(
        const PixelWriterFunction& pixel_writer, const glm::vec2& current_position, const glm::vec2& top_point, const glm::vec2& top_bottom_line, unsigned int data_index, int fill_direction)
    {
        // calculate the x value of the top_bottom edge
        int x = get_x_for_y_on_line(top_point, top_bottom_line, current_position.y) + fill_direction;

        for (int j = current_position.x; j != x; j += fill_direction) {
            pixel_writer(glm::vec2(j, current_position.y), data_index);
        }
    }

    template <typename PixelWriterFunction>
    void dda_triangle_line(const PixelWriterFunction& pixel_writer,
        const glm::vec2& line_start_point,
        const glm::vec2& line,
        const glm::vec2& top_point,
        const glm::vec2& top_bottom_line,
        unsigned int data_index,
        int fill_direction)
    {
        glm::vec2 step_size;
        int steps;
        std::tie(step_size, steps) = calculate_dda_steps(line);

        int last_y = 0;

        glm::vec2 current_position = line_start_point;

        for (int j = 0; j < steps; j++) {
            pixel_writer(current_position, data_index);

            if (int(current_position.y + step_size.y) > current_position.y) { // next step would go to next y value

                fill_inner_triangle(pixel_writer, current_position, top_point, top_bottom_line, data_index, fill_direction);
                last_y = current_position.y;
            }
            current_position += step_size;
        }

        current_position -= step_size;

        // check if we have to apply the fill_inner_triangle alg for the current row
        if (last_y != floor(current_position.y))
            fill_inner_triangle(pixel_writer, current_position, top_point, top_bottom_line, data_index, fill_direction);
    }

    template <typename PixelWriterFunction> void dda_triangle(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangle, unsigned int triangle_index)
    {
        auto edge_top_bottom = triangle[2] - triangle[0];
        auto edge_top_middle = triangle[1] - triangle[0];
        auto edge_middle_bottom = triangle[2] - triangle[1];

        // TODO apply make triangle bigger lambda function if it was passed

        // auto normal_top_bottom = glm::normalize(glm::vec2(-edge_top_bottom.y, edge_top_bottom.x));
        // auto normal_top_middle = glm::normalize(glm::vec2(-edge_top_middle.y, edge_top_middle.x));
        // auto normal_middle_bottom = glm::normalize(glm::vec2(-edge_middle_bottom.y, edge_middle_bottom.x));

        // { // swap normal direction if they are incorrect (pointing to center)
        //     glm::vec2 centroid = (triangle[0] + triangle[1] + triangle[2]) / 3.0f;
        //     if (glm::dot(normal_top_bottom, centroid - triangle[0]) > 0) {
        //         normal_top_bottom *= -1;
        //     }
        //     if (glm::dot(normal_top_middle, centroid - triangle[0]) > 0) {
        //         normal_top_middle *= -1;
        //     }
        //     if (glm::dot(normal_middle_bottom, centroid - triangle[1]) > 0) {
        //         normal_middle_bottom *= -1;
        //     }
        // }

        // top bottom line
        // dda_line(pixel_writer, , triangle_index, 0, true);

        // do we have to fill the triangle from right to left(-1) or from left to right(1)
        int fill_direction = (triangle[1].x < triangle[2].x) ? 1 : -1;

        // // top middle line
        dda_triangle_line(pixel_writer, triangle[0], edge_top_middle, triangle[0], edge_top_bottom, triangle_index, fill_direction);
        // // middle bottom line
        dda_triangle_line(pixel_writer, triangle[1], edge_middle_bottom, triangle[0], edge_top_bottom, triangle_index, fill_direction);

        // TODO enable again
        // add_end_cap(triangle_collection.cell_to_data, triangle[0], triangle_index, thickness);
        // add_end_cap(triangle_collection.cell_to_data, triangle[1], triangle_index, thickness);
        // add_end_cap(triangle_collection.cell_to_data, triangle[2], triangle_index, thickness);
    }

    template <typename PixelWriterFunction>
    void triangle_sdf(const PixelWriterFunction& pixel_writer, const glm::vec2 current_position, const std::array<glm::vec2, 3> points, unsigned int data_index, float distance)
    {
        // https://iquilezles.org/articles/distfunctions2d/

        glm::vec2 e0 = points[1] - points[0], e1 = points[2] - points[1], e2 = points[0] - points[2];
        float s = glm::sign(e0.x * e2.y - e0.y * e2.x);

        glm::vec2 v0 = current_position - points[0], v1 = current_position - points[1], v2 = current_position - points[2];
        glm::vec2 pq0 = v0 - e0 * glm::clamp(glm::dot(v0, e0) / glm::dot(e0, e0), 0.0f, 1.0f);
        glm::vec2 pq1 = v1 - e1 * glm::clamp(glm::dot(v1, e1) / glm::dot(e1, e1), 0.0f, 1.0f);
        glm::vec2 pq2 = v2 - e2 * glm::clamp(glm::dot(v2, e2) / glm::dot(e2, e2), 0.0f, 1.0f);
        glm::vec2 d
            = min(min(glm::vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)), glm::vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))), glm::vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
        float dist_from_tri = -sqrt(d.x) * glm::sign(d.y);

        if (dist_from_tri < distance) {
            pixel_writer(current_position, data_index);
        }
    }

} // namespace details

// triangulizes polygons and orders the vertices by y position per triangle
// output: top, middle, bottom, top, middle,...
std::vector<glm::vec2> triangulize(std::vector<glm::vec2> polygons);

// ideal if you want to rasterize only a few triangles, where every triangle covers a large part of the raster size
// in this method every pixel traverses every triangle
template <typename PixelWriterFunction> void rasterize_triangle_sdf(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangles, float distance, const glm::ivec2 grid_size)
{
    // we sample from the center
    // and have a radius of a half pixel diagonal
    // this still leads to false positives if the edge of a triangle is slighly in another pixel without every comming in this pixel
    distance += sqrt(0.5);

    for (uint8_t i = 0; i < grid_size.x; i++) {
        for (uint8_t j = 0; j < grid_size.y; j++) {
            auto current_position = glm::vec2 { i + 0.5, j + 0.5 };

            for (size_t i = 0; i < triangles.size() / 3; ++i) {
                details::triangle_sdf(pixel_writer, current_position, { triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2] }, i, distance);
            }
        }
    }
}

// ideal for many triangles. especially if each triangle only covers small parts of the raster
// in this method every triangle is traversed only once, and it only accesses pixels it needs for itself.
template <typename PixelWriterFunction> void rasterize_triangle(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangles)
{
    for (size_t i = 0; i < triangles.size() / 3; ++i) {
        details::dda_triangle(pixel_writer, { triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2] }, i);
    }
}

// template <typename PixelWriterFunction>
// void rasterize_line(const PixelWriterFunction& pixel_writer, std::vector<glm::vec2> line_points);

} // namespace nucleus::utils::rasterizer
