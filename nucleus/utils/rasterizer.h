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
#include <vector>

#include <iostream>

#include <glm/glm.hpp>

namespace nucleus::utils::rasterizer {

/*
 * Possible future improvements:
 *      - check how often each pixel is written to -> maybe we can improve performance here
 *      - sdf box filter instead by circle to only consider values of the current cell
 *      - calculate_dda_steps currently doubles the steps to prevent missing certain pixels (this might be improved)
 */

// TODOs:
// - line renderer

namespace details {

    /*
     * Calculates the x value for a given y.
     * The resulting x value will be on the line.
     */
    inline float get_x_for_y_on_line(const glm::vec2 point_on_line, const glm::vec2 line, float y)
    {
        // pos_on_line = point_on_line + t * line
        float t = (y - point_on_line.y) / line.y;

        return point_on_line.x + t * line.x;
    }

    /*
     * Calculates the steps and step_size of the given line.
     * the "bigger dimension" (-> line is bigger in x or y direction) has a step size of 0.5
     * the other dimension has a step size <= 0.5
     */
    // inline std::pair<glm::vec2, int> calculate_dda_steps(const glm::vec2 line)
    // {
    //     float steps = fabs(line.x) > fabs(line.y) ? fabs(line.x) : fabs(line.y);

    //     // should only be triggered if two vertices of a triangle are exactly on the same position -> the input data is wrong
    //     assert(steps > 0);
    //     // steps = std::max(steps, 1); // alternative force at least one dda step -> wrong output but it should at least "work"

    //     auto step_size = glm::vec2(line.x / float(steps), line.y / float(steps));

    //     // double the steps -> otherwise we might miss important pixels ( see "rasterize triangle small" unittest for use case)
    //     // it might be possible to capture the special cases during the line traversal and render an extra pixel
    //     // -> possible performance improvement
    //     steps *= 2;
    //     step_size /= 2.0f;

    //     // TODO here: possible solution: dont render anything if the current point is exactly on integer values (e.g. {2, 2])

    //     std::cout << step_size.x << " " << step_size.y << " " << ceil(steps) << std::endl;
    //     return std::make_pair(step_size, ceil(steps));
    // }

    template <typename PixelWriterFunction> inline void fill_between(const PixelWriterFunction& pixel_writer, int y, int x1, int x2, unsigned int data_index)
    {
        // calculate the x value of the other line

        int fill_direction = (x1 < x2) ? 1 : -1;

        for (int j = x1; j != x2 + fill_direction; j += fill_direction) {
            pixel_writer(glm::vec2(j, y), data_index);
        }
    }

    /*
     * fills a scan line from current position to x position of given other line
     */
    template <typename PixelWriterFunction>
    inline void fill_to_line(const PixelWriterFunction& pixel_writer, const glm::vec2& current_position, const glm::vec2& other_point, const glm::vec2& other_line, unsigned int data_index)
    {
        // calculate the x value of the other line
        int x = get_x_for_y_on_line(other_point, other_line, current_position.y);

        fill_between(pixel_writer, current_position.y, current_position.x, x, data_index);
    }

    /*
     * applies dda algorithm to the given line and fills the area in between this line and the given other line
     * if the other line is not located on the current scan line -> we fill to the given fall back lines
     * the given line should always go from top to bottom (only exception horizontal lines)
     */
    template <typename PixelWriterFunction>
    void render_line(const PixelWriterFunction& pixel_writer,
        unsigned int data_index,
        const glm::vec2& line_point,
        const glm::vec2& line,
        int fill_direction = 0,
        const glm::vec2& other_point = { 0, 0 },
        const glm::vec2& other_line = { 0, 0 },
        const glm::vec2& fallback_point_upper = { 0, 0 },
        const glm::vec2& fallback_line_upper = { 0, 0 },
        const glm::vec2& fallback_point_lower = { 0, 0 },
        const glm::vec2& fallback_line_lower = { 0, 0 })
    {
        int current_y = floor(line_point.y);
        // find out how many y steps lie between origin and line end
        int y_steps = floor(line_point.y + line.y) - current_y;

        // const bool is_enlarged_triangle = normal_line != glm::vec2 { 0, 0 };
        const bool fill = other_line != glm::vec2 { 0, 0 }; // no other line given? -> no fill needed
        const bool is_enlarged = fallback_line_upper != glm::vec2 { 0, 0 }; // no fallback lines given -> not enlarged

        // create variables to remember where to check for x values for the fill_between fill
        // -> in the current scan line do we check the top of the pixel or the bottom of the pixel to fill a line to completion
        // this assumes that line is left of other line (if it is the other way around this will be fixed further down)
        bool fill_current_from_top_x = (line.x > 0) ? true : false;
        // bool fill_current_from_top_x = (line.x > 0) ? false : true;
        bool fill_other_from_top_x = (other_line.x > 0) ? false : true;

        // int fill_direction = 0;
        if (fill) {
            // look at x position of other line
            // note if the other line goes through the current line there might be a problem
            // but this shouldn't happen -> so we should be fine by only looking at the upper x part and compare
            // int x_other_line = get_x_for_y_on_line(other_point, other_line, line_point.y + line.y);
            // if (line_point.x > x_other_line) {
            if (fill_direction < 0) {
                // fill_direction = -1;
                // we have to switch where to look for the fill line to stop
                fill_current_from_top_x = !fill_current_from_top_x;
                fill_other_from_top_x = !fill_other_from_top_x;

            } else {
                // fill_direction = 1;
            }
        }

        // glm::vec2 current_position = line_point;

        // if a line would go directly through integer points, we would falsely draw a whole pixel -> we want to prevent this
        constexpr float epsilon = 0.0001;

        if (y_steps == 0) {
            // line is smaller than one scan line
            // only draw from line start to end
            if (!fill)
                fill_between(pixel_writer, line_point.y, line_point.x, line.x + line_point.x, data_index);
            else {
                int x_current_line;
                int x_other_line;

                if (fill_current_from_top_x)
                    x_current_line = line_point.x;
                else
                    x_current_line = line_point.x + line.x;

                const float test_other_y = (fill_other_from_top_x) ? line_point.y : line_point.y + line.y;

                if (is_enlarged && test_other_y < other_point.y)
                    x_other_line = get_x_for_y_on_line(fallback_point_lower, fallback_line_lower, test_other_y);
                else if (is_enlarged && test_other_y > other_point.y + other_line.y)
                    x_other_line = get_x_for_y_on_line(fallback_point_upper, fallback_line_upper, test_other_y);
                else
                    x_other_line = get_x_for_y_on_line(other_point, other_line, test_other_y);

                fill_between(pixel_writer, line_point.y, x_current_line, x_other_line, data_index);
            }

        } else {
            // draw first and last stretches of the line
            if (!fill) {
                // start of the line
                int x_start_bottom = get_x_for_y_on_line(line_point, line, ceil(line_point.y) - epsilon);
                fill_between(pixel_writer, line_point.y, line_point.x, x_start_bottom, data_index);

                // end of the line
                int x_end_top = get_x_for_y_on_line(line_point, line, floor(line_point.y + line.y) + epsilon);
                fill_between(pixel_writer, line_point.y + line.y, line_point.x + line.x, x_end_top, data_index);
            } else {

                int x_current_line;
                int x_other_line;

                { // start of the line
                    // const float line_top_y = line_point.y;
                    const float bottom_y = ceil(line_point.y) - epsilon; // top of the y step

                    if (fill_current_from_top_x)
                        x_current_line = line_point.x;
                    else
                        x_current_line = get_x_for_y_on_line(line_point, line, bottom_y);

                    const float test_other_y = (fill_other_from_top_x) ? line_point.y : bottom_y;

                    // decide whether or not to fill only to the fallback line (if outside of other line segment)
                    // although this is the end of the line -> we have to test both upper and lower of the other line for large enlarged triangles and small other lines
                    if (is_enlarged && test_other_y < other_point.y)
                        x_other_line = get_x_for_y_on_line(fallback_point_lower, fallback_line_lower, test_other_y);
                    else if (is_enlarged && test_other_y > other_point.y + other_line.y)
                        x_other_line = get_x_for_y_on_line(fallback_point_upper, fallback_line_upper, test_other_y);
                    else
                        x_other_line = get_x_for_y_on_line(other_point, other_line, test_other_y);

                    fill_between(pixel_writer, line_point.y, x_current_line, x_other_line, data_index);
                }

                { // end of the line
                    const float line_bottom_y = line_point.y + line.y;
                    const float top_y = floor(line_bottom_y) + epsilon; // top of the y step

                    if (fill_current_from_top_x)
                        x_current_line = get_x_for_y_on_line(line_point, line, top_y);
                    else
                        x_current_line = line_point.x + line.x; // no calculation necessary we just take the line end

                    const float test_other_y = (fill_other_from_top_x) ? top_y : line_bottom_y;

                    // decide whether or not to fill only to the fallback line (if outside of other line segment)
                    // although this is the end of the line -> we have to test both upper and lower of the other line for large enlarged triangles and small other lines
                    if (is_enlarged && test_other_y < other_point.y)
                        x_other_line = get_x_for_y_on_line(fallback_point_lower, fallback_line_lower, test_other_y);
                    else if (is_enlarged && test_other_y > other_point.y + other_line.y)
                        x_other_line = get_x_for_y_on_line(fallback_point_upper, fallback_line_upper, test_other_y);
                    else
                        x_other_line = get_x_for_y_on_line(other_point, other_line, test_other_y);

                    // if (fill_other_from_top_x)
                    //     x_other_line = get_x_for_y_on_line(other_point, other_line, top_y);
                    // else
                    //     x_other_line = get_x_for_y_on_line(other_point, other_line, line_bottom_y);

                    fill_between(pixel_writer, line_bottom_y, x_current_line, x_other_line, data_index);
                }
            }
        }

        // TODO draw all steps in between

        for (int y = line_point.y + 1; y < line_point.y + y_steps - 1; y++) {
            // draw all the steps in between

            // at a distinct y step draw the current line from floor to ceil
            // if(fill_current_from_top_x)

            if (!fill) {
                int x1 = get_x_for_y_on_line(line_point, line, y + epsilon);
                int x2 = get_x_for_y_on_line(line_point, line, y + 1 - epsilon);

                fill_between(pixel_writer, y, x1, x2, data_index);
            } else {
                // fill to other line
                int x1, x2;

                if (fill_current_from_top_x)
                    x1 = get_x_for_y_on_line(line_point, line, y + epsilon);
                else
                    x1 = get_x_for_y_on_line(line_point, line, y + 1 - epsilon);

                float test_other_y = (fill_other_from_top_x) ? y + epsilon : y + 1 - epsilon;

                // decide whether or not to fill only to the fallback line (if outside of other line segment)
                if (is_enlarged && test_other_y < other_point.y)
                    x2 = get_x_for_y_on_line(fallback_point_lower, fallback_line_lower, test_other_y);
                else if (is_enlarged && test_other_y > other_point.y + other_line.y)
                    x2 = get_x_for_y_on_line(fallback_point_upper, fallback_line_upper, test_other_y);
                else
                    x2 = get_x_for_y_on_line(other_point, other_line, test_other_y);

                fill_between(pixel_writer, y, x1, x2, data_index);
            }
        }
    }

    /*
     * renders a circle at the given position
     */
    template <typename PixelWriterFunction> void add_circle_end_cap(const PixelWriterFunction& pixel_writer, const glm::vec2 position, unsigned int data_index, float distance)
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

    /*
     * In order to process enlarged triangles, the triangle is separated into 3 parts: top, middle and bottom
     * we first generate the normals for each edge and offset the triangle origins by the normal multiplied by the supplied distance to get 6 "enlarged" points that define the bigger triangle
     *
     * top / bottom part:
     * the top and bottom part of the triangle are rendered by going through each y step of the line with the shorter y value and filling the inner triangle until it reaches the other side of the
     * triangle for the very top and very bottom of the triangle special considerations have to be done, otherwise we would render too much if the line on the other side of the triangle is not hit
     * during a fill operation (= we want to fill above or below the line segment), we use the normal of the current line as the bound for the fill operation
     *
     * for the middle part:
     * if we enlarge the triangle by moving the edge along an edge normal we generate a gap between the top-middle and middle-bottom edge (since those lines are only translated)
     * we have to somehow rasterize this gap between the bottom vertice of the translated top-middle edge and the top vertice of the middle-bottom edge.
     * in order to do this we draw a new edge between those vertices and rasterize everything between this new edge and the top-bottom edge.
     *
     * lastly we have to add endcaps on each original vertice position with the given distance
     */
    template <typename PixelWriterFunction> void render_triangle(const PixelWriterFunction& pixel_writer, const std::array<glm::vec2, 3> triangle, unsigned int triangle_index, float distance)
    {

        assert(triangle[0].y <= triangle[1].y);
        assert(triangle[1].y <= triangle[2].y);

        auto edge_top_bottom = triangle[2] - triangle[0];
        auto edge_top_middle = triangle[1] - triangle[0];
        auto edge_middle_bottom = triangle[2] - triangle[1];

        int fill_direction = (triangle[1].x < triangle[2].x) ? 1 : -1;

        if (distance == 0.0) {
            // top middle
            render_line(pixel_writer, triangle_index, triangle[0], edge_top_middle, fill_direction, triangle[0], edge_top_bottom);
            // middle bottom
            render_line(pixel_writer, triangle_index, triangle[1], edge_middle_bottom, fill_direction, triangle[0], edge_top_bottom);

            return; // finished
        }

        // we need to render an enlarged triangle

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

        auto enlarged_top_bottom_origin = triangle[0] + normal_top_bottom * distance;
        // auto enlarged_top_bottom_end = triangle[2] + normal_top_bottom * distance; // not needed

        auto enlarged_top_middle_origin = triangle[0] + normal_top_middle * distance;
        auto enlarged_top_middle_end = triangle[1] + normal_top_middle * distance;

        auto enlarged_middle_bottom_origin = triangle[1] + normal_middle_bottom * distance;
        auto enlarged_middle_bottom_end = triangle[2] + normal_middle_bottom * distance;

        // top middle
        render_line(pixel_writer,
            triangle_index,
            enlarged_top_middle_origin,
            edge_top_middle,
            fill_direction,
            enlarged_top_bottom_origin,
            edge_top_bottom,
            enlarged_middle_bottom_end,
            normal_middle_bottom,
            enlarged_top_middle_origin,
            normal_top_middle);

        // // middle_top_part to middle_bottom_part
        auto half_middle_line = (enlarged_middle_bottom_origin - enlarged_top_middle_end) / glm::vec2(2.0);

        render_line(pixel_writer,
            triangle_index,
            enlarged_top_middle_end,
            half_middle_line,
            fill_direction,
            enlarged_top_bottom_origin,
            edge_top_bottom,
            enlarged_middle_bottom_end,
            normal_middle_bottom,
            enlarged_top_middle_origin,
            normal_top_middle);
        render_line(pixel_writer,
            triangle_index,
            enlarged_top_middle_end + half_middle_line,
            half_middle_line,
            fill_direction,
            enlarged_top_bottom_origin,
            edge_top_bottom,
            enlarged_middle_bottom_end,
            normal_middle_bottom,
            enlarged_top_middle_origin,
            normal_top_middle);

        // middle bottom
        render_line(pixel_writer,
            triangle_index,
            enlarged_middle_bottom_origin,
            edge_middle_bottom,
            fill_direction,
            enlarged_top_bottom_origin,
            edge_top_bottom,
            enlarged_middle_bottom_end,
            normal_middle_bottom,
            enlarged_top_middle_origin,
            normal_top_middle);

        // endcaps
        add_circle_end_cap(pixel_writer, triangle[0], triangle_index, distance);
        add_circle_end_cap(pixel_writer, triangle[1], triangle_index, distance);
        add_circle_end_cap(pixel_writer, triangle[2], triangle_index, distance);

        // { // DEBUG visualize enlarged points
        //     auto enlarged_top_bottom_end = triangle[2] + normal_top_bottom * distance;
        //     pixel_writer(triangle[0], 50);
        //     pixel_writer(enlarged_top_bottom_origin, 50);
        //     pixel_writer(enlarged_top_middle_origin, 50);

        //     pixel_writer(triangle[1], 50);
        //     pixel_writer(enlarged_middle_bottom_origin, 50);
        //     pixel_writer(enlarged_top_middle_end, 50);

        //     pixel_writer(triangle[2], 50);
        //     pixel_writer(enlarged_middle_bottom_end, 50);
        //     pixel_writer(enlarged_top_bottom_end, 50);
        // }
    }

    template <typename PixelWriterFunction> void render_line_preprocess(const PixelWriterFunction& pixel_writer, const std::array<glm::vec2, 2> line_points, unsigned int line_index, float distance)
    {
        // edge from top to bottom
        glm::vec2 origin;
        glm::vec2 edge;
        if (line_points[0].y > line_points[1].y) {
            edge = line_points[0] - line_points[1];
            origin = line_points[1];
        } else {
            edge = line_points[1] - line_points[0];
            origin = line_points[0];
        }

        if (distance == 0.0) {
            render_line(pixel_writer, line_index, origin, edge);

            pixel_writer(line_points[0], 50);
            pixel_writer(line_points[1], 50);

            return; // finished
        }

        // we need to render an enlarged triangle

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

        // auto enlarged_top_bottom_origin = triangle[0] + normal_top_bottom * distance;
        // // auto enlarged_top_bottom_end = triangle[2] + normal_top_bottom * distance; // not needed

        // auto enlarged_top_middle_origin = triangle[0] + normal_top_middle * distance;
        // auto enlarged_top_middle_end = triangle[1] + normal_top_middle * distance;

        // auto enlarged_middle_bottom_origin = triangle[1] + normal_middle_bottom * distance;
        // auto enlarged_middle_bottom_end = triangle[2] + normal_middle_bottom * distance;

        // // top middle
        // render_line(pixel_writer,
        //     line_index,
        //     enlarged_top_middle_origin,
        //     edge_top_middle,
        //     enlarged_top_bottom_origin,
        //     edge_top_bottom,
        //     normal_top_middle,
        //     enlarged_middle_bottom_end,
        //     normal_middle_bottom,
        //     enlarged_top_middle_origin,
        //     normal_top_middle);

        // // // middle_top_part to middle_bottom_part
        // auto half_middle_line = (enlarged_middle_bottom_origin - enlarged_top_middle_end) / glm::vec2(2.0);

        // render_line(pixel_writer,
        //     line_index,
        //     enlarged_top_middle_end,
        //     half_middle_line,
        //     enlarged_top_bottom_origin,
        //     edge_top_bottom,
        //     normal_top_middle,
        //     enlarged_middle_bottom_end,
        //     normal_middle_bottom,
        //     enlarged_top_middle_origin,
        //     normal_top_middle);
        // render_line(pixel_writer,
        //     line_index,
        //     enlarged_top_middle_end + half_middle_line,
        //     half_middle_line,
        //     enlarged_top_bottom_origin,
        //     edge_top_bottom,
        //     normal_middle_bottom,
        //     enlarged_middle_bottom_end,
        //     normal_middle_bottom,
        //     enlarged_top_middle_origin,
        //     normal_top_middle);

        // // middle bottom
        // render_line(pixel_writer,
        //     line_index,
        //     enlarged_middle_bottom_origin,
        //     edge_middle_bottom,
        //     enlarged_top_bottom_origin,
        //     edge_top_bottom,
        //     normal_middle_bottom,
        //     enlarged_middle_bottom_end,
        //     normal_middle_bottom,
        //     enlarged_top_middle_origin,
        //     normal_top_middle);

        // // endcaps
        // add_circle_end_cap(pixel_writer, triangle[0], line_index, distance);
        // add_circle_end_cap(pixel_writer, triangle[1], line_index, distance);
        // add_circle_end_cap(pixel_writer, triangle[2], line_index, distance);

        // { // DEBUG visualize enlarged points
        //     auto enlarged_top_bottom_end = triangle[2] + normal_top_bottom * distance;
        //     pixel_writer(triangle[0], 50);
        //     pixel_writer(enlarged_top_bottom_origin, 50);
        //     pixel_writer(enlarged_top_middle_origin, 50);

        //     pixel_writer(triangle[1], 50);
        //     pixel_writer(enlarged_middle_bottom_origin, 50);
        //     pixel_writer(enlarged_top_middle_end, 50);

        //     pixel_writer(triangle[2], 50);
        //     pixel_writer(enlarged_middle_bottom_end, 50);
        //     pixel_writer(enlarged_top_bottom_end, 50);
        // }
    }

    /*
     * calculates how far away the given position is from a triangle
     * uses distance to shift the current triangle distance
     * if it is within the triangle, the pixel writer function is called
     *
     * uses triangle distance calculation from: https://iquilezles.org/articles/distfunctions2d/
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
        //

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
            pixel_writer(current_position, data_index);
        }
    }

    /*
     * calculates how far away the given position is from a line segment
     * uses distance to shift the current triangle distance
     * if it is within the triangle, the pixel writer function is called
     *
     * uses line segment distance calculation from: https://iquilezles.org/articles/distfunctions2d/
     */
    template <typename PixelWriterFunction>
    void line_sdf(const PixelWriterFunction& pixel_writer, const glm::vec2 current_position, const glm::vec2 origin, glm::vec2 edge, unsigned int data_index, float distance)
    {
        glm::vec2 v0 = current_position - origin;

        float dist_from_line = length(v0 - edge * glm::clamp(glm::dot(v0, edge) / glm::dot(edge, edge), 0.0f, 1.0f));

        if (dist_from_line < distance) {
            pixel_writer(current_position, data_index);
        }
    }

} // namespace details

/*
 * triangulizes polygons and orders the vertices by y position per triangle
 * output: top, middle, bottom, top, middle,...
 */
std::vector<glm::vec2> triangulize(std::vector<glm::vec2> polygons);

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

                details::triangle_sdf(pixel_writer, current_position, points, edges, s, i, distance);
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

                details::line_sdf(pixel_writer, current_position, points[0], edge, i, distance);
            }
        }
    }
}

/*
 * ideal for many triangles. especially if each triangle only covers small parts of the raster
 * in this method every triangle is traversed only once, and it only accesses pixels it needs for itself.
 */
template <typename PixelWriterFunction> void rasterize_triangle(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangles, float distance = 0.0)
{
    for (size_t i = 0; i < triangles.size() / 3; ++i) {
        details::render_triangle(pixel_writer, { triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2] }, i, distance);
    }
}

template <typename PixelWriterFunction> void rasterize_line(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& line_points, float distance = 0.0)
{
    for (size_t i = 0; i < line_points.size() - 1; ++i) {
        details::render_line_preprocess(pixel_writer, { line_points[i + 0], line_points[i + 1] }, i, distance);
    }
}

} // namespace nucleus::utils::rasterizer
