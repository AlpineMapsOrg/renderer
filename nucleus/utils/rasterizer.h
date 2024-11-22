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

#include <vector>

#include <glm/glm.hpp>

namespace nucleus::utils::rasterizer {

namespace details {

    inline float get_x_for_y_on_line(const glm::vec2 origin, const glm::vec2 line, float y)
    {
        // pos_on_line = origin + t * line
        float t = (y - origin.y) / line.y;
        return origin.x + t * line.x;
    }

    inline std::pair<glm::vec2, int> calculate_dda_steps(const glm::vec2 line)
    {
        int steps = fabs(line.x) > fabs(line.y) ? fabs(line.x) : fabs(line.y);

        auto step_size = glm::vec2(line.x / float(steps), line.y / float(steps));

        return std::make_pair(step_size, steps);
    }

    template <typename PixelWriterFunction>
    void dda_triangle_line(
        const PixelWriterFunction& pixel_writer, const glm::vec2 origin0, const glm::vec2 line0, const glm::vec2 origin1, const glm::vec2 line1, unsigned int data_index, int fill_direction)
    {

        glm::vec2 step_size;
        int steps;
        std::tie(step_size, steps) = calculate_dda_steps(line0);

        // make sure that at least one step is applied
        steps = std::max(steps, 1);

        glm::vec2 current_start_position = origin0;

        glm::vec2 current_position = current_start_position;

        for (int j = 0; j < steps; j++) {
            pixel_writer(current_position, data_index);

            // add a write step to x/y -> this prevents holes from thickness steps to appear by making the lines thicker
            if (step_size.x < 1)
                pixel_writer(current_position + glm::vec2(1, 0) * glm::sign(step_size.x), data_index);
            if (step_size.y < 1)
                pixel_writer(current_position + glm::vec2(0, 1) * glm::sign(step_size.y), data_index);

            if (int(current_position.y + step_size.y) > current_position.y) { // next step would go to next y value

                int x = get_x_for_y_on_line(origin1, line1, current_position.y);

                for (int j = current_position.x; j != x; j += fill_direction) {
                    pixel_writer(glm::vec2(j, current_position.y), data_index);
                }
            }
            current_position += step_size;
        }
    }

    template <typename PixelWriterFunction> void dda_triangle(const PixelWriterFunction& pixel_writer, std::vector<glm::vec2> triangle, unsigned int triangle_index)
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

        // DEBUG draw lines with certain index
        // dda_line(triangle[0], edge_top_bottom, normal_top_bottom * 1.0f, 100, 0, true);
        // dda_line(triangle[0], edge_top_middle, normal_top_middle * 1.0f, 100, fill_direction, true);
        // dda_line(triangle[1], edge_middle_bottom, normal_middle_bottom * 1.0f, 100, fill_direction, true);

        // TODO enable again
        // add_end_cap(triangle_collection.cell_to_data, triangle[0], triangle_index, thickness);
        // add_end_cap(triangle_collection.cell_to_data, triangle[1], triangle_index, thickness);
        // add_end_cap(triangle_collection.cell_to_data, triangle[2], triangle_index, thickness);
    }

} // namespace details

// triangulizes polygons and orders the vertices by y position per triangle
// output: top, middle, bottom, top, middle,...
std::vector<glm::vec2> triangulize(std::vector<glm::vec2> polygons);

template <typename PixelWriterFunction> void rasterize_triangle(const PixelWriterFunction& pixel_writer, std::vector<glm::vec2> triangles)
{
    for (size_t i = 0; i < triangles.size() / 3; ++i) {
        details::dda_triangle(pixel_writer, { triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2] }, i);
    }
}

// template <typename PixelWriterFunction>
// void rasterize_line(const PixelWriterFunction& pixel_writer, std::vector<glm::vec2> line_points);

} // namespace nucleus::utils::rasterizer
