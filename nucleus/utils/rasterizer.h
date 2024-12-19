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

#include <glm/glm.hpp>

namespace nucleus::utils::rasterizer {
/*
 * Possible future improvements:
 *      - check how often each pixel is written to -> maybe we can improve performance here
 */

/* PixelWriterFunctionConcept
 * The functions use a PixelWriterFunction as a parameter.
 * The pixelwriter function is a lambda with one glm::ivec2 position parameter and one optional unsigned int data argument
 *      glm::ivec2 position
 *          the position where a pixel should be rendered
 *      unsigned int data
 *          the triangle/line-segment index
 */
template <typename Func>
concept PixelWriterFunctionConcept = requires(Func f) {
    { f(glm::ivec2(11, 22)) };
} || requires(Func f) {
    { f(glm::ivec2(11, 22), 123456u) };
};

namespace details {

    /*
     * This function calls either the 1 parameter or the 2 parameter pixelwriter (determined at runtime)
     */
    template <PixelWriterFunctionConcept PixelWriterFunction> inline void invokePixelWriter(const PixelWriterFunction& pixel_writer, glm::ivec2 position, unsigned int data_index)
    {
        if constexpr (std::is_invocable_v<PixelWriterFunction, glm::ivec2, unsigned int>) {
            pixel_writer(position, data_index);
        } else if constexpr (std::is_invocable_v<PixelWriterFunction, glm::ivec2>) {
            pixel_writer(position);
        }
    }

    /*
     * Calculates the x value for a given y.
     * The resulting x value will be on the line.
     */
    inline float get_x_for_y_on_line(const glm::vec2& point_on_line, const glm::vec2& line, float y)
    {
        // pos_on_line = point_on_line + t * line
        float t = (y - point_on_line.y) / line.y;

        return point_on_line.x + t * line.x;
    }

    /*
     * calculate the x value of a line
     * uses fallback lines if the y value is outside of the line segment
     */
    inline int get_x_with_fallback(const glm::vec2& point,
        const glm::vec2& line,
        float y,
        bool is_enlarged,
        const glm::vec2& fallback_point_bottom,
        const glm::vec2& fallback_line_bottom,
        const glm::vec2& fallback_point_top,
        const glm::vec2& fallback_line_top)
    {
        // decide whether or not to fill only to the fallback line (if outside of other line segment)
        if (is_enlarged && y < point.y)
            return get_x_for_y_on_line(fallback_point_top, fallback_line_top, y);
        else if (is_enlarged && y > point.y + line.y)
            return get_x_for_y_on_line(fallback_point_bottom, fallback_line_bottom, y);
        else
            return get_x_for_y_on_line(point, line, y);
    }

    /*
     * fills a scanline in the intveral [x1,x2] (inclusive)
     */
    template <PixelWriterFunctionConcept PixelWriterFunction> inline void fill_between(const PixelWriterFunction& pixel_writer, int y, int x1, int x2, unsigned int data_index)
    {
        int fill_direction = (x1 < x2) ? 1 : -1;

        for (int j = x1; j != x2 + fill_direction; j += fill_direction) {
            invokePixelWriter(pixel_writer, glm::ivec2(j, y), data_index);
        }
    }

    /*
     * renders the given line
     * if other line is given fills everything in between the current and the other line
     * if the other line is not located on the current scan line -> we fill to the given fall back lines
     * the given line should always go from top to bottom (only exception horizontal lines)
     */
    template <PixelWriterFunctionConcept PixelWriterFunction>
    void render_line(const PixelWriterFunction& pixel_writer,
        unsigned int data_index,
        const glm::vec2& line_point,
        const glm::vec2& line,
        const int fill_direction = 0,
        const glm::vec2& other_point = { 0, 0 },
        const glm::vec2& other_line = { 0, 0 },
        const glm::vec2& fallback_point_bottom = { 0, 0 },
        const glm::vec2& fallback_line_bottom = { 0, 0 },
        const glm::vec2& fallback_point_top = { 0, 0 },
        const glm::vec2& fallback_line_top = { 0, 0 })
    {
        // if a line would go directly through integer points, we would falsely draw a whole pixel -> we want to prevent this
        constexpr float epsilon = 0.00001;

        // find out how many y steps lie between origin and line end
        const int y_steps = floor(line_point.y + line.y) - floor(line_point.y);

        // const bool is_enlarged_triangle = normal_line != glm::vec2 { 0, 0 };
        const bool fill = other_line != glm::vec2 { 0, 0 }; // no other line given? -> no fill needed
        const bool is_enlarged = fallback_line_bottom != glm::vec2 { 0, 0 }; // no fallback lines given -> not enlarged

        // create variables to remember where to check for x values for the fill_between fill
        // -> in the current scan line do we check the top of the pixel or the bottom of the pixel to fill a line to completion
        // this assumes that line is left of other line (if it is the other way around this will be fixed further down)
        bool fill_current_from_top_x = (line.x > 0) ? true : false;
        bool fill_other_from_top_x = (other_line.x > 0) ? false : true;

        if (fill && fill_direction < 0) {
            // we have to switch where to look for the fill line to stop
            fill_current_from_top_x = !fill_current_from_top_x;
            fill_other_from_top_x = !fill_other_from_top_x;
        }

        // special cases for line start/end + if line is smaller than 1px
        if (y_steps == 0) {
            // line is smaller than one scan line
            // only draw from line start to end
            if (!fill)
                fill_between(pixel_writer, line_point.y, line_point.x, line.x + line_point.x, data_index);
            else {
                // go from top or bottom of line (depending on where the other line is located)
                const auto x1 = (fill_current_from_top_x) ? line_point.x : line_point.x + line.x;

                const float test_y_other = (fill_other_from_top_x) ? line_point.y : line_point.y + line.y;
                const auto x2 = get_x_with_fallback(other_point, other_line, test_y_other, is_enlarged, fallback_point_bottom, fallback_line_bottom, fallback_point_top, fallback_line_top);

                fill_between(pixel_writer, line_point.y, x1, x2, data_index);
            }
        } else {
            // draw first and last stretches of the line
            if (!fill) {
                // start of the line (start to bottom of pixel)
                int x_start_bottom = get_x_for_y_on_line(line_point, line, ceil(line_point.y) - epsilon);
                fill_between(pixel_writer, line_point.y, line_point.x, x_start_bottom, data_index);

                // end of the line (top of pixel to end of line)
                int x_end_top = get_x_for_y_on_line(line_point, line, floor(line_point.y + line.y) + epsilon);
                fill_between(pixel_writer, line_point.y + line.y, line_point.x + line.x, x_end_top, data_index);
            } else {
                { // start of the line
                    // const float line_top_y = line_point.y;
                    const float bottom_y = ceil(line_point.y) - epsilon; // top of the y step

                    // either use line start or calculate the x at the bottom of the pixel
                    const auto x1 = (fill_current_from_top_x) ? line_point.x : get_x_for_y_on_line(line_point, line, bottom_y);

                    // decide whether or not to fill only to the fallback line (if outside of other line segment)
                    // although this is the end of the line -> we have to test both upper and lower of the other line for large enlarged triangles and small other lines
                    const float test_y_other = (fill_other_from_top_x) ? line_point.y : bottom_y;
                    const auto x2 = get_x_with_fallback(other_point, other_line, test_y_other, is_enlarged, fallback_point_bottom, fallback_line_bottom, fallback_point_top, fallback_line_top);

                    fill_between(pixel_writer, line_point.y, x1, x2, data_index);
                }

                { // end of the line
                    const float line_bottom_y = line_point.y + line.y;
                    const float top_y = floor(line_bottom_y) + epsilon; // top of the y step

                    // either calculate the y value at top of pixel or use the end point of line
                    const auto x1 = (fill_current_from_top_x) ? get_x_for_y_on_line(line_point, line, top_y) : line_point.x + line.x;

                    // decide whether or not to fill only to the fallback line (if outside of other line segment)
                    // although this is the end of the line -> we still have to test both upper and lower of the other line for large enlarged triangles and small other lines
                    const float test_y_other = (fill_other_from_top_x) ? top_y : line_bottom_y;
                    const auto x2 = get_x_with_fallback(other_point, other_line, test_y_other, is_enlarged, fallback_point_bottom, fallback_line_bottom, fallback_point_top, fallback_line_top);

                    fill_between(pixel_writer, line_bottom_y, x1, x2, data_index);
                }
            }
        }

        // draw all the steps in between
        for (int y = line_point.y + 1; y < line_point.y + y_steps - 1; y++) {
            // at a distinct y step draw the current line from floor to ceil

            if (!fill) {
                const auto x1 = get_x_for_y_on_line(line_point, line, y + epsilon);
                const auto x2 = get_x_for_y_on_line(line_point, line, y + 1 - epsilon);

                fill_between(pixel_writer, y, x1, x2, data_index);
            } else {
                // fill to other line
                const float test_y_current = (fill_current_from_top_x) ? y + epsilon : y + 1 - epsilon;
                const auto x1 = get_x_for_y_on_line(line_point, line, test_y_current);

                const float test_y_other = (fill_other_from_top_x) ? y + epsilon : y + 1 - epsilon;
                const auto x2 = get_x_with_fallback(other_point, other_line, test_y_other, is_enlarged, fallback_point_bottom, fallback_line_bottom, fallback_point_top, fallback_line_top);

                fill_between(pixel_writer, y, x1, x2, data_index);
            }
        }
    }

    /*
     * renders a circle at the given position
     */
    template <PixelWriterFunctionConcept PixelWriterFunction> void add_circle_end_cap(const PixelWriterFunction& pixel_writer, const glm::vec2& position, unsigned int data_index, float distance)
    {
        distance += sqrt(0.5);
        float distance_test = ceil(distance);

        for (int i = -distance_test; i < distance_test; i++) {
            for (int j = -distance_test; j < distance_test; j++) {
                const auto offset = glm::vec2(i + 0.0, j + 0.0);
                if (glm::length(offset) < distance)
                    invokePixelWriter(pixel_writer, position + offset, data_index);
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
    template <PixelWriterFunctionConcept PixelWriterFunction>
    void render_triangle(const PixelWriterFunction& pixel_writer, const std::array<glm::vec2, 3> triangle, unsigned int triangle_index, float distance)
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
        //     invokePixelWriter(pixel_writer, triangle[0], 50);
        //     invokePixelWriter(pixel_writer, enlarged_top_bottom_origin, 50);
        //     invokePixelWriter(pixel_writer, enlarged_top_middle_origin, 50);

        //     invokePixelWriter(pixel_writer, triangle[1], 50);
        //     invokePixelWriter(pixel_writer, enlarged_middle_bottom_origin, 50);
        //     invokePixelWriter(pixel_writer, enlarged_top_middle_end, 50);

        //     invokePixelWriter(pixel_writer, triangle[2], 50);
        //     invokePixelWriter(pixel_writer, enlarged_middle_bottom_end, 50);
        //     invokePixelWriter(pixel_writer, enlarged_top_bottom_end, 50);
        // }
    }

    template <PixelWriterFunctionConcept PixelWriterFunction>
    void render_line_preprocess(const PixelWriterFunction& pixel_writer, const std::array<glm::vec2, 2> line_points, unsigned int line_index, float distance)
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

        // only draw lines with 1 pixel width
        if (distance == 0.0) {
            render_line(pixel_writer, line_index, origin, edge);

            // debug output for points
            // invokePixelWriter(pixel_writer, line_points[0], 50);
            // invokePixelWriter(pixel_writer, line_points[1], 50);

            return; // finished
        }

        // we want to draw lines with a thickness

        // end caps
        add_circle_end_cap(pixel_writer, line_points[0], line_index, distance);
        add_circle_end_cap(pixel_writer, line_points[1], line_index, distance);

        // distance += sqrt(0.5);

        // create normal
        // make sure that the normal points downwards
        glm::vec2 normal;
        if (edge.x < 0)
            normal = glm::normalize(glm::vec2(edge.y, -edge.x)) * distance;
        else
            normal = glm::normalize(glm::vec2(-edge.y, edge.x)) * distance;

        auto enlarged_top_origin = origin + normal * -1.0f;
        auto enlarged_middle_origin = origin + normal;
        auto enlarged_middle_end = origin + edge + normal * -1.0f;
        // auto enlarged_bottom_origin = origin + edge + normal;

        // double the normal so that we construct a rectangle with the edge
        normal *= 2.0f;

        // determine if the doubled normal or the edge has a larger y difference
        // and switch if normal is larger in y direction
        if (edge.y < normal.y) {
            auto tmp = normal;
            normal = edge;
            edge = tmp;

            // we also need to change two enlarged points
            tmp = enlarged_middle_origin;
            enlarged_middle_origin = enlarged_middle_end;
            enlarged_middle_end = tmp;
        }

        // special case: one side is horizontal
        // -> only one pass from top to bottom necessary
        if (normal.y == 0) {
            render_line(pixel_writer, line_index, enlarged_top_origin, edge, (enlarged_top_origin.x > enlarged_middle_origin.x) ? -1 : 1, enlarged_middle_origin, edge);

            return;
        }

        // we have to fill once the edge side and once the normal side.
        // one side should not need fallbacks to render everything

        int fill_direction = (edge.x > 0) ? -1 : 1;

        // go from top of the line to bottom of the line
        // first fill everything between edge and 2xnormal, after this fill everything betweent edge and other edge
        render_line(pixel_writer, line_index, enlarged_top_origin, edge, fill_direction, enlarged_middle_origin, edge, enlarged_top_origin, normal, enlarged_top_origin, normal);

        // render the last bit of the line (from line end top to line end bottom) -> no need for a fallback line since both lines meet at the bottom
        render_line(pixel_writer, line_index, enlarged_middle_end, normal, fill_direction, enlarged_middle_origin, edge * 2.0f);

        // { // DEBUG visualize enlarged points
        //     // const auto enlarged_bottom_origin = origin + edge + normal * distance;
        //     invokePixelWriter(pixel_writer, enlarged_top_origin, 50);
        //     invokePixelWriter(pixel_writer, enlarged_middle_origin, 50);

        //     invokePixelWriter(pixel_writer, enlarged_middle_end, 50);
        //     invokePixelWriter(pixel_writer, enlarged_bottom_origin, 50);

        //     invokePixelWriter(pixel_writer, line_points[0], 50);
        //     invokePixelWriter(pixel_writer, line_points[1], 50);
        // }
    }

} // namespace details

/*
 * generates edges of a polygon
 * this assumes that polygons neighbouring in the vector should form an edge and the first and last edge are also connected
 */
std::vector<glm::ivec2> generate_neighbour_edges(std::vector<glm::vec2> polygon_points);

/*
 * triangulizes polygons and orders the vertices by y position per triangle
 * output: top, middle, bottom, top, middle,...
 */
std::vector<glm::vec2> triangulize(std::vector<glm::vec2> polygon_points, std::vector<glm::ivec2> edges, bool remove_duplicate_vertices = false);

/*
 * Rasterize a triangle
 * in this method every triangle is traversed only once, and it only accesses pixels it needs for itself.
 * this function determines the position where pixels should be set and calls the given pixel_writer lambda to actually draw the pixels
 *
 * NOTE: the triangle points have to be ordered correctly by y position
 * -> input triangles: ordered by y position within each triangle (top_ypos_triangle_1, middle_ypos_triangle_1, bottom_ypos_triangle_1, top_ypos_triangle_2, middle_ypos_triangle_2, ...)
 *
 * example usage:
 *      const std::vector<glm::vec2> triangle_points = { glm::vec2(30.5, 10.5), glm::vec2(10.5, 30.5), glm::vec2(50.5, 50.5) };
 *      nucleus::Raster<uint8_t> output({ 64, 64 }, 0u);
 *      const auto pixel_writer = [&output](glm::ivec2 pos) { output.pixel(pos) = 255; };
 *      nucleus::utils::rasterizer::rasterize_triangle(pixel_writer, triangle_points);
 */
template <PixelWriterFunctionConcept PixelWriterFunction> void rasterize_triangle(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& triangles, float distance = 0.0)
{
    for (size_t i = 0; i < triangles.size() / 3; ++i) {
        details::render_triangle(pixel_writer, { triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2] }, i, distance);
    }
}

/*
 * Rasterize a line
 *
 * this function determines the position where pixels should be set and calls the given pixel_writer lambda to actually draw the pixels
 *
 * input line_points: a list of points that should form a line (line_start, line_point1, line_point2, ..., line_end)
 * -> generates line segments between line_start-line_point1, line_point1 to line_point2, ...
 *
 * example usage:
 *      const std::vector<glm::vec2> line = { glm::vec2(30.5, 10.5), glm::vec2(50.5, 30.5), glm::vec2(30.5, 50.5), glm::vec2(10.5, 30.5), glm::vec2(30.5, 10.5) };
 *      nucleus::Raster<uint8_t> output({ 64, 64 }, 0u);
 *      const auto pixel_writer = [&output](glm::ivec2 pos) { output.pixel(pos) = 255; };
 *      nucleus::utils::rasterizer::rasterize_line(pixel_writer, line);
 */
template <PixelWriterFunctionConcept PixelWriterFunction> void rasterize_line(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& line_points, float distance = 0.0)
{
    for (size_t i = 0; i < line_points.size() - 1; ++i) {
        details::render_line_preprocess(pixel_writer, { line_points[i + 0], line_points[i + 1] }, i, distance);
    }
}

/*
 * Rasterize a polygon
 * convenience function that really just calls rasterize_triangle after triangulizing the polygon
 * this function determines the position where pixels should be set and calls the given pixel_writer lambda to actually draw the pixels
 *
 * example usage:
 *      const std::vector<glm::vec2> polygon_points = { glm::vec2(30.5, 10.5), glm::vec2(10.5, 30.5), glm::vec2(50.5, 50.5) };
 *      nucleus::Raster<uint8_t> output({ 64, 64 }, 0u);
 *      const auto pixel_writer = [&output](glm::ivec2 pos) { output.pixel(pos) = 255; };
 *      nucleus::utils::rasterizer::rasterize_polygon(pixel_writer, polygon_points);
 */
template <PixelWriterFunctionConcept PixelWriterFunction> void rasterize_polygon(const PixelWriterFunction& pixel_writer, const std::vector<glm::vec2>& polygon_points, float distance = 0.0)
{
    const auto edges = generate_neighbour_edges(polygon_points);
    const auto triangles = triangulize(polygon_points, edges);

    rasterize_triangle(pixel_writer, triangles, distance);
}

} // namespace nucleus::utils::rasterizer
