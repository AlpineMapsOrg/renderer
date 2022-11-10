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

#include "Camera.h"

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

Camera::Camera(const glm::dvec3& position, const glm::dvec3& view_at_point) // : m_position(position)
{
    m_camera_transformation = glm::inverse(glm::lookAt(position, view_at_point, { 0, 0, 1 }));
    if (std::isnan(m_camera_transformation[0][0])) {
        m_camera_transformation = glm::inverse(glm::lookAt(position, view_at_point, { 0, 1, 0 }));
    }

    setPerspectiveParams(45, { 1, 1 }, m_near_clipping);
}

glm::dmat4 Camera::cameraMatrix() const
{
    return glm::inverse(m_camera_transformation);
}

glm::dmat4 Camera::projectionMatrix() const
{
    return m_projection_matrix;
}

glm::dmat4 Camera::worldViewProjectionMatrix() const
{
    return m_projection_matrix * cameraMatrix();
}

glm::mat4 Camera::localViewProjectionMatrix(const glm::dvec3& origin_offset) const
{
    return glm::mat4(m_projection_matrix * cameraMatrix() * glm::translate(origin_offset));
}

glm::dvec3 Camera::position() const
{
    return glm::dvec3(m_camera_transformation[3]);
}

glm::dvec3 Camera::xAxis() const
{
    return glm::dvec3(m_camera_transformation[0]);
}

glm::dvec3 Camera::yAxis() const
{
    return glm::dvec3(m_camera_transformation[1]);
}

glm::dvec3 Camera::zAxis() const
{
    return glm::dvec3(m_camera_transformation[2]);
}

glm::dvec3 Camera::ray_direction(const glm::dvec2& normalised_device_coordinates) const
{
    const auto inverse_projection_matrix = glm::inverse(projectionMatrix());
    const auto inverse_view_matrix = m_camera_transformation;
    const auto unprojected = inverse_projection_matrix * glm::dvec4(normalised_device_coordinates.x, normalised_device_coordinates.y, 1, 1);
    const auto normalised_unprojected = unprojected / unprojected.w;
    return glm::normalize(glm::dvec3(inverse_view_matrix * normalised_unprojected) - position());
}

std::vector<geometry::Plane<double>> Camera::clippingPlanes() const
{
    const auto clippingPane = [this](const glm::dvec2& a, const glm::dvec2& b) {
        const auto v_a = ray_direction(a);
        const auto v_b = ray_direction(b);
        const auto normal = glm::normalize(cross(v_a, v_b));
        const auto distance = -dot(normal, position());
        return geometry::Plane<double> { normal, distance };
    };
    std::vector<geometry::Plane<double>> clipping_panes;
    // front and back
    const auto p0 = position() + -zAxis() * m_near_clipping;
    clipping_panes.push_back({ .normal = -zAxis(), .distance = -dot(-zAxis(), p0) });
    const auto p1 = position() + -zAxis() * m_far_clipping;
    clipping_panes.push_back({ .normal = zAxis(), .distance = -dot(zAxis(), p1) });

    // top and down
    clipping_panes.push_back(clippingPane({ -1, 1 }, { 1, 1 }));
    clipping_panes.push_back(clippingPane({ 1, -1 }, { -1, -1 }));

    // left and right
    clipping_panes.push_back(clippingPane({ -1, -1 }, { -1, 1 }));
    clipping_panes.push_back(clippingPane({ 1, 1 }, { 1, -1 }));
    return clipping_panes;
}

void Camera::setPerspectiveParams(float fov_degrees, const glm::uvec2& viewport_size, double near_plane)
{
    m_near_clipping = near_plane;
    m_far_clipping = near_plane * 1000;
    m_viewport_size = viewport_size;
    // half a metre to 10 000 km
    // should be precise enough (https://outerra.blogspot.com/2012/11/maximizing-depth-buffer-range-and.html)
    m_projection_matrix = glm::perspective(glm::radians(double(fov_degrees)), double(viewport_size.x) / double(viewport_size.y), m_near_clipping, m_far_clipping);
}

void Camera::pan(const glm::dvec2& v)
{
    const auto x_dir = xAxis();
    const auto y_dir = glm::cross(x_dir, glm::dvec3(0, 0, 1));
    m_camera_transformation = glm::translate(-1.0 * (v.x * x_dir + v.y * y_dir)) * m_camera_transformation;
}

void Camera::move(const glm::dvec3& v)
{
    m_camera_transformation = glm::translate(v) * m_camera_transformation;
}

void Camera::orbit(const glm::dvec3& centre, const glm::dvec2& degrees)
{
    move(-centre);
    const auto rotation_x_axis = glm::rotate(glm::radians(degrees.y), xAxis());
    const auto rotation_z_axis = glm::rotate(glm::radians(degrees.x), glm::dvec3(0, 0, 1));
    const auto rotation = rotation_z_axis * rotation_x_axis;
    m_camera_transformation = rotation * m_camera_transformation;
    move(centre);
}

void Camera::orbit(const glm::vec2& degrees)
{
    orbit(operationCentre(), degrees);
}

void Camera::zoom(double v)
{
    move(zAxis() * v);
}

const glm::uvec2& Camera::viewportSize() const
{
    return m_viewport_size;
}

glm::dvec3 Camera::operationCentre() const
{
    // a ray going through the middle pixel, intersecting with the z == 0 pane
    const auto origin = position();
    const auto direction = -zAxis();
    const auto t = -origin.z / direction.z;
    return origin + t * direction;
}
