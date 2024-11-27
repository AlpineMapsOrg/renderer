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
//      - current problems: some irregularities with the get_x_for_y_on_line result/ fill_inner_triangle function
//      - we start too late and stop to early with the top and bottom lines -> maybe we need to round first?
// - line renderer
// sdf box filter instead by circle to only consider values of the current cell

namespace details {

    inline float get_x_for_y_on_line(const glm::vec2 origin, const glm::vec2 line, float y)
    {
        // pos_on_line = origin + t * line
        float t = (y - origin.y) / line.y;

        return origin.x + t * line.x;
    }

    // half vector between n1 and n2 projected onto the half vector gives us the y value that can go through to the other side, without causing problems
    // the problem is that we might still remove things we want to render
    // solution: calculate the half vector and the projection on it for upper lower bound of our rendering pass
    // and only render from extended line to this half vector -> the last render pass renders from the other side also to the half vector
    // -> problem we split the line in two parts (up to middle - middle to bottom -> so we only have to render the largest line to middle with first half vector and from middle with second half vector
    // -> other problem what to do with middle part -> calculate new line between both enlarged vertices? and render this new line? -> fully through

    // scanne bis zum orig bottom vertice y value -> until then use the half vector for end position from then on use the normal vector as end position.

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

    // pixel_writer, topmost_renderable_point, top_bisector,
    // enlarged_top_bottom_origin, edge_top_bottom,
    // enlarged_top_middle_origin, edge_top_middle, triangle_index
    template <typename PixelWriterFunction>
    void dda_triangle_line(const PixelWriterFunction& pixel_writer,
        const glm::vec2& line_start_point,
        const glm::vec2& line,
        const glm::vec2& outer_point_a,
        const glm::vec2& outer_line_a,
        const glm::vec2& outer_point_b,
        const glm::vec2& outer_line_b,
        // const glm::vec2&, // outer_point_a,
        // const glm::vec2&, // outer_line_a,
        // const glm::vec2&, // outer_point_b,
        // const glm::vec2&, // outer_line_b,
        unsigned int data_index)
    {
        int fill_direction;
        bool only_fill_once = false; // only fill from line to outer line a
        if (outer_line_b == glm::vec2 { 0, 0 }) {
            if (outer_point_a.x == line_start_point.x) // determine fill direction at line end
                fill_direction = (outer_point_a.x + outer_line_a.x < line_start_point.x + line.x) ? -1 : 1;
            else // determine fill direction at line start
                fill_direction = (outer_point_a.x < line_start_point.x) ? -1 : 1;
            only_fill_once = true;
        } else
            fill_direction = (outer_point_a.x + outer_line_a.x < outer_point_b.x + outer_line_b.x) ? -1 : 1;

        glm::vec2 step_size;
        int steps;
        std::tie(step_size, steps) = calculate_dda_steps(line);

        int last_y = 0;

        glm::vec2 current_position = line_start_point;

        for (int j = 0; j < steps; j++) {
            pixel_writer(current_position, data_index);

            if (int(current_position.y + step_size.y) > current_position.y) { // next step would go to next y value
                fill_inner_triangle(pixel_writer, current_position, outer_point_a, outer_line_a, data_index, fill_direction);
                if (!only_fill_once)
                    fill_inner_triangle(pixel_writer, current_position, outer_point_b, outer_line_b, data_index, -fill_direction);
                last_y = current_position.y;
            }
            current_position += step_size;
        }

        current_position -= step_size;

        // check if we have to apply the fill_inner_triangle alg for the current row
        if (last_y != floor(current_position.y)) {
            fill_inner_triangle(pixel_writer, current_position, outer_point_a, outer_line_a, data_index, fill_direction);
            if (!only_fill_once)
                fill_inner_triangle(pixel_writer, current_position, outer_point_b, outer_line_b, data_index, -fill_direction);
        }
    }

    template <typename PixelWriterFunction> void add_end_cap(const PixelWriterFunction& pixel_writer, const glm::vec2 position, unsigned int data_index, float distance)
    {
        distance += sqrt(0.5);
        float distance_test = ceil(distance);

        for (int i = -distance_test; i < distance_test; i++) {
            for (int j = -distance_test; j < distance_test; j++) {
                const auto offset = glm::vec2(i + 0.0, j + 0.0);
                if (glm::length(offset) < distance)
                    pixel_writer(position + offset, data_index);
            }
        }
    }

    template <typename PixelWriterFunction> void dda_triangle(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangle, unsigned int triangle_index)
    {
        auto edge_top_bottom = triangle[2] - triangle[0];
        auto edge_top_middle = triangle[1] - triangle[0];
        auto edge_middle_bottom = triangle[2] - triangle[1];

        // top middle
        dda_triangle_line(pixel_writer, triangle[0], edge_top_middle, triangle[0], edge_top_bottom, { 0, 0 }, { 0, 0 }, triangle_index);
        // middle bottom
        dda_triangle_line(pixel_writer, triangle[1], edge_middle_bottom, triangle[0], edge_top_bottom, { 0, 0 }, { 0, 0 }, triangle_index);

        // lastly we have to add endcaps on each original vertice position with the passed through distance
        // add_end_cap(pixel_writer, triangle[0], triangle_index, distance);
        // add_end_cap(pixel_writer, triangle[1], triangle_index, distance);
        // add_end_cap(pixel_writer, triangle[2], triangle_index, distance);
    }

    template <typename PixelWriterFunction> void dda_triangle(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangle, unsigned int triangle_index, float distance)
    {
        // In order to process enlarged triangles, the triangle is separated into 3 parts: top, middle and bottom
        // top and bottom parts are rendered by calculating the bisector line of both normal vectors and rendering everything left and everyting right of this line by using a fill operation

        // for the middle part:
        // if we enlarge the triangle by moving the edge along an edge normal we generate a gap between the top-middle and middle-bottom edge (if those lines are only translated)
        // we have to somehow rasterize this gap between the bottom vertice of the translated top-middle edge and the top vertice of the middle-bottom edge.
        // in order to do this we draw a new edge between those vertices and rasterize everything between this new edge and the top-bottom edge.

        // in the worst case here we have one triangle line that is paralell to scan line
        // in this case it might be that we still have to consider that we are writing outside of the actual triangle !!!! -> find a solution!!!

        // distance += sqrt(0.5);

        auto edge_top_bottom = triangle[2] - triangle[0];
        auto edge_top_middle = triangle[1] - triangle[0];
        auto edge_middle_bottom = triangle[2] - triangle[1];

        auto normal_top_bottom = glm::normalize(glm::vec2(-edge_top_bottom.y, edge_top_bottom.x));
        auto normal_top_middle = glm::normalize(glm::vec2(-edge_top_middle.y, edge_top_middle.x));
        auto normal_middle_bottom = glm::normalize(glm::vec2(-edge_middle_bottom.y, edge_middle_bottom.x));

        { // swap normal direction if they are incorrect (pointing to center)
            glm::vec2 centroid = (triangle[0] + triangle[1] + triangle[2]) / 3.0f;
            if (glm::dot(normal_top_bottom, centroid - triangle[0]) > 0) {
                normal_top_bottom *= -1;
            }
            if (glm::dot(normal_top_middle, centroid - triangle[0]) > 0) {
                normal_top_middle *= -1;
            }
            if (glm::dot(normal_middle_bottom, centroid - triangle[1]) > 0) {
                normal_middle_bottom *= -1;
            }
        }

        auto half_vector_top = (normal_top_bottom + normal_top_middle) / glm::vec2(2.0);
        auto half_vector_bottom = (normal_top_bottom + normal_middle_bottom) / glm::vec2(2.0);

        auto enlarged_top_bottom_origin = triangle[0] + normal_top_bottom * distance;
        auto enlarged_top_bottom_end = triangle[2] + normal_top_bottom * distance;
        auto enlarged_top_middle_origin = triangle[0] + normal_top_middle * distance;

        auto enlarged_top_middle_end = triangle[1] + normal_top_middle * distance;
        auto enlarged_middle_bottom_origin = triangle[1] + normal_middle_bottom * distance;
        auto enlarged_middle_bottom_end = triangle[2] + normal_middle_bottom * distance;

        float top_y = std::min(std::min(triangle[0].y + half_vector_top.y, enlarged_top_bottom_origin.y), enlarged_top_middle_origin.y);
        float bottom_y = std::max(std::max(triangle[2].y + half_vector_bottom.y, enlarged_top_bottom_end.y), enlarged_middle_bottom_end.y);

        auto topmost_renderable_point = glm::vec2(get_x_for_y_on_line(triangle[0], half_vector_top, top_y), top_y);
        auto bottommost_renderable_point = glm::vec2(get_x_for_y_on_line(triangle[2], half_vector_bottom, bottom_y), bottom_y);

        auto top_bisector = glm::vec2(get_x_for_y_on_line(topmost_renderable_point, half_vector_top, enlarged_top_middle_end.y), enlarged_top_middle_end.y) - topmost_renderable_point;
        auto middle_to_bottom_origin = glm::vec2(get_x_for_y_on_line(bottommost_renderable_point, half_vector_bottom, enlarged_middle_bottom_origin.y), enlarged_middle_bottom_origin.y);

        auto middle_bisector = bottommost_renderable_point - middle_to_bottom_origin;

        // top to middle(1)
        dda_triangle_line(pixel_writer, topmost_renderable_point, top_bisector, enlarged_top_bottom_origin, edge_top_bottom, enlarged_top_middle_origin, edge_top_middle, triangle_index);

        // middle(1) to middle(2)
        dda_triangle_line(
            pixel_writer, enlarged_top_middle_end, enlarged_middle_bottom_origin - enlarged_top_middle_end, enlarged_top_bottom_origin, edge_top_bottom, { 0, 0 }, { 0, 0 }, triangle_index);

        // middle(2) to bottom
        dda_triangle_line(pixel_writer, middle_to_bottom_origin, middle_bisector, enlarged_top_bottom_origin, edge_top_bottom, enlarged_middle_bottom_origin, edge_middle_bottom, triangle_index);

        // lastly we have to add endcaps on each original vertice position with the passed through distance
        // add_end_cap(pixel_writer, triangle[0], triangle_index, distance);
        // add_end_cap(pixel_writer, triangle[1], triangle_index, distance);
        // add_end_cap(pixel_writer, triangle[2], triangle_index, distance);
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
template <typename PixelWriterFunction> void rasterize_triangle(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangles, float distance = 0.0)
{
    for (size_t i = 0; i < triangles.size() / 3; ++i) {
        if (distance == 0.0)
            details::dda_triangle(pixel_writer, { triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2] }, i);
        else
            details::dda_triangle(pixel_writer, { triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2] }, i, distance);
    }
}

// template <typename PixelWriterFunction>
// void rasterize_line(const PixelWriterFunction& pixel_writer, std::vector<glm::vec2> line_points);

} // namespace nucleus::utils::rasterizer
