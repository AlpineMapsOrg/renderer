/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include "Definition.h"

#include <cmath>

#include <QDebug>
#include <glm/gtx/transform.hpp>

#include "sherpa/geometry.h"

// camera space in opengl / webgl:
// origin:
// - x and y: the ray that goes through the centre of the monitor
// - z: the eye position
// axis directions:
// - x points to the right
// - y points upwards
// - negative z points into the scene.
//
// https://webglfundamentals.org/webgl/lessons/webgl-3d-camera.html

camera::Definition::Definition()
    : Definition({ 1, 1, 1 }, { 0, 0, 0 })
{
}

camera::Definition::Definition(const glm::dvec3& position, const glm::dvec3& view_at_point) // : m_position(position)
{
    m_camera_transformation = glm::inverse(glm::lookAt(position, view_at_point, { 0, 0, 1 }));
    if (std::isnan(m_camera_transformation[0][0])) {
        m_camera_transformation = glm::inverse(glm::lookAt(position, view_at_point, { 0, 1, 0 }));
    }

    set_perspective_params(m_fov, m_viewport_size, m_near_clipping);
}

glm::dmat4 camera::Definition::camera_matrix() const
{
    return glm::inverse(m_camera_transformation);
}

glm::dmat4 camera::Definition::camera_space_to_world_matrix() const
{
    return m_camera_transformation;
}

glm::dmat4 camera::Definition::projection_matrix() const
{
    return m_projection_matrix;
}

glm::dmat4 camera::Definition::world_view_projection_matrix() const
{
    return m_projection_matrix * camera_matrix();
}

glm::mat4 camera::Definition::local_view_projection_matrix(const glm::dvec3& origin_offset) const
{
    return glm::mat4(m_projection_matrix * camera_matrix() * glm::translate(origin_offset));
}

glm::dvec3 camera::Definition::position() const
{
    return glm::dvec3(m_camera_transformation[3]);
}

glm::dvec3 camera::Definition::x_axis() const
{
    return glm::dvec3(m_camera_transformation[0]);
}

glm::dvec3 camera::Definition::y_axis() const
{
    return glm::dvec3(m_camera_transformation[1]);
}

glm::dvec3 camera::Definition::z_axis() const
{
    return glm::dvec3(m_camera_transformation[2]);
}

glm::dvec3 camera::Definition::ray_direction(const glm::dvec2& normalised_device_coordinates) const
{
    const auto inverse_projection_matrix = glm::inverse(projection_matrix());
    const auto inverse_view_matrix = m_camera_transformation;
    const auto unprojected = inverse_projection_matrix * glm::dvec4(normalised_device_coordinates.x, normalised_device_coordinates.y, 1, 1);
    const auto normalised_unprojected = unprojected / unprojected.w;
    return glm::normalize(glm::dvec3(inverse_view_matrix * normalised_unprojected) - position());
}

std::vector<geometry::Plane<double>> camera::Definition::clipping_planes() const
{
    std::vector<geometry::Plane<double>> clipping_panes;
    // front and back
    const auto p0 = position() + -z_axis() * double(m_near_clipping);
    clipping_panes.push_back({ .normal = -z_axis(), .distance = -dot(-z_axis(), p0) });
    const auto p1 = position() + -z_axis() * double(m_far_clipping);
    clipping_panes.push_back({ .normal = z_axis(), .distance = -dot(z_axis(), p1) });

    const auto four = four_clipping_planes();
    std::copy(four.begin(), four.end(), std::back_inserter(clipping_panes));
    return clipping_panes;
}

std::vector<geometry::Plane<double>> camera::Definition::four_clipping_planes() const
{
    const auto clippingPane = [this](const glm::dvec2& a, const glm::dvec2& b) {
        const auto v_a = ray_direction(a);
        const auto v_b = ray_direction(b);
        const auto normal = glm::normalize(cross(v_a, v_b));
        const auto distance = -dot(normal, position());
        return geometry::Plane<double> { normal, distance };
    };
    std::vector<geometry::Plane<double>> clipping_panes;

    // top and down
    clipping_panes.push_back(clippingPane({ -1, 1 }, { 1, 1 }));
    clipping_panes.push_back(clippingPane({ 1, -1 }, { -1, -1 }));

    // left and right
    clipping_panes.push_back(clippingPane({ -1, -1 }, { -1, 1 }));
    clipping_panes.push_back(clippingPane({ 1, 1 }, { 1, -1 }));
    return clipping_panes;
}

void camera::Definition::set_perspective_params(float fov_degrees, const glm::uvec2& viewport_size, float near_plane)
{
    m_near_clipping = near_plane;
    m_far_clipping = near_plane * 100'000;
    m_far_clipping = std::min(m_far_clipping, 1'000'000.f);     // will be obscured by atmosphere anyways + depth based atmosphere will have numerical issues (show background atmosphere)
    m_viewport_size = viewport_size;
    m_fov = fov_degrees;
    m_projection_matrix = glm::perspective(
        glm::radians(double(fov_degrees)),
        double(viewport_size.x) / double(viewport_size.y),
        double(m_near_clipping),
        double(m_far_clipping));
}

void camera::Definition::set_near_plane(float near_plane)
{
    set_perspective_params(m_fov, m_viewport_size, near_plane);
}

float camera::Definition::near_plane() const
{
    return m_near_clipping;
}

void camera::Definition::pan(const glm::dvec2& v)
{
    const auto x_dir = x_axis();
    const auto y_dir = glm::cross(x_dir, glm::dvec3(0, 0, 1));
    m_camera_transformation = glm::translate(-1.0 * (v.x * x_dir + v.y * y_dir)) * m_camera_transformation;
}

void camera::Definition::move(const glm::dvec3& v)
{
    m_camera_transformation = glm::translate(v) * m_camera_transformation;
}

void camera::Definition::orbit(const glm::dvec3& centre, const glm::dvec2& degrees)
{
    move(-centre);
    const auto rotation_x_axis = glm::rotate(glm::radians(degrees.y), x_axis());
    const auto rotation_z_axis = glm::rotate(glm::radians(degrees.x), glm::dvec3(0, 0, 1));
    const auto rotation = rotation_z_axis * rotation_x_axis;
    m_camera_transformation = rotation * m_camera_transformation;
    move(centre);
}

void camera::Definition::orbit(const glm::vec2& degrees)
{
    orbit(operation_centre(), degrees);
}

void camera::Definition::zoom(double v)
{
    move(z_axis() * v);
}

const glm::uvec2& camera::Definition::viewport_size() const
{
    return m_viewport_size;
}

glm::dvec3 camera::Definition::operation_centre() const
{
    // a ray going through the middle pixel, intersecting with the z == 0 pane
    const auto origin = position();
    const auto direction = -z_axis();
    const auto t = -origin.z / direction.z;
    return origin + t * direction;
}


namespace camera {
void Definition::set_viewport_size(const glm::uvec2& new_viewport_size)
{
    set_perspective_params(m_fov, new_viewport_size, m_near_clipping);
}

}
