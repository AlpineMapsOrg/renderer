 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

 namespace nucleus::camera {

 struct Frustum {
     std::array<geometry::Plane<double>, 6> clipping_planes; // the order of clipping panes is front, back, top, down, left, and right
     std::array<glm::dvec3, 8> corners; // the order of corners is ccw, starting from top left, front plane -> back plane
};

class Definition {
public:
    Definition();
    Definition(const glm::dvec3& position, const glm::dvec3& view_at_point);
    [[nodiscard]] glm::dmat4 camera_matrix() const;
    [[nodiscard]] glm::dmat4 camera_space_to_world_matrix() const;
    void set_camera_space_to_world_matrix(const glm::dmat4& new_camera_transformation);
    [[nodiscard]] glm::dmat4 projection_matrix() const;
    // transforms from webmercator to clip space. You should use this matrix only in double precision.
    [[nodiscard]] glm::dmat4 world_view_projection_matrix() const;
    // transforms form the local coordinate system (webmercator shifted by origin_offset) to clip space.
    [[nodiscard]] glm::mat4 local_view_projection_matrix(const glm::dvec3& origin_offset) const;
    [[nodiscard]] glm::mat4 local_view_matrix() const;
    [[nodiscard]] glm::dvec3 position() const;
    [[nodiscard]] glm::dvec3 x_axis() const;
    [[nodiscard]] glm::dvec3 y_axis() const;
    [[nodiscard]] glm::dvec3 z_axis() const;
    [[nodiscard]] glm::dvec3 ray_direction(const glm::dvec2& normalised_device_coordinates) const;
    [[nodiscard]] Frustum frustum() const;
    [[nodiscard]] std::array<geometry::Plane<double>, 6> clipping_planes() const;
    [[nodiscard]] std::vector<geometry::Plane<double>> four_clipping_planes() const;
    [[nodiscard]] float distance_scale_factor() const;
    void set_perspective_params(float fov_degrees, const glm::uvec2& viewport_size, float near_plane);
    void set_near_plane(float near_plane);
    [[nodiscard]] float near_plane() const;
    void pan(const glm::dvec2& v);
    void move(const glm::dvec3& v);
    void orbit(const glm::dvec3& centre, const glm::dvec2& degrees);
    void orbit_clamped(const glm::dvec3& centre, const glm::dvec2& degrees);
    void zoom(double v);
    void look_at(const glm::dvec3& camera_position, const glm::dvec3& view_at_point);

    [[nodiscard]] const glm::uvec2& viewport_size() const;
    // screen space is assumed in the qt way, i.e., origin is top left (https://doc.qt.io/qt-6/coordsys.html)
    [[nodiscard]] glm::dvec2 to_ndc(const glm::dvec2& screen_space_coordinates) const;
    [[nodiscard]] float to_screen_space(float world_space_size, float world_space_distance) const;

    // Calculates an arbritrary lookat position at a given WS distance infront of the camera
    glm::dvec3 calculate_lookat_position(double distance) const;

    void set_viewport_size(const glm::uvec2& new_viewport_size);

    bool operator==(const Definition& other) const;

    float field_of_view() const;
    void set_field_of_view(float new_field_of_view_degrees);

private:
    [[nodiscard]] glm::dvec3 operation_centre() const;

private:
    glm::dmat4 m_projection_matrix;
    glm::dmat4 m_camera_transformation;
    float m_field_of_view = 0; // degrees
    float m_distance_scaling_factor = 0;
    float m_near_clipping = 1.0;
    float m_far_clipping = 1'000'000;
    glm::uvec2 m_viewport_size = { 480, 270 };
};

}
