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

#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace geometry {
template <typename T>
struct Plane;
}

namespace camera {

class Definition {
public:
    Definition();
    Definition(const glm::dvec3& position, const glm::dvec3& view_at_point);
    [[nodiscard]] glm::dmat4 cameraMatrix() const;
    [[nodiscard]] glm::dmat4 camera_space_to_world_matrix() const;
    [[nodiscard]] glm::dmat4 projectionMatrix() const;
    // transforms from webmercator to clip space. You should use this matrix only in double precision.
    [[nodiscard]] glm::dmat4 worldViewProjectionMatrix() const;
    // transforms form the local coordinate system (webmercator shifted by origin_offset) to clip space.
    [[nodiscard]] glm::mat4 localViewProjectionMatrix(const glm::dvec3& origin_offset) const;
    [[nodiscard]] glm::dvec3 position() const;
    [[nodiscard]] glm::dvec3 xAxis() const;
    [[nodiscard]] glm::dvec3 yAxis() const;
    [[nodiscard]] glm::dvec3 zAxis() const;
    [[nodiscard]] glm::dvec3 ray_direction(const glm::dvec2& normalised_device_coordinates) const;
    [[nodiscard]] std::vector<geometry::Plane<double>> clippingPlanes() const;
    [[nodiscard]] std::vector<geometry::Plane<double>> fourClippingPlanes() const;
    void setPerspectiveParams(float fov_degrees, const glm::uvec2& viewport_size, float near_plane);
    void setNearPlane(float near_plane);
    [[nodiscard]] float nearPlane() const;
    void pan(const glm::dvec2& v);
    void move(const glm::dvec3& v);
    void orbit(const glm::dvec3& centre, const glm::dvec2& degrees);
    // orbits around the intersection of negative z and 0 plane (temprorary only, until we can read the depth buffer)
    void orbit(const glm::vec2& degrees);
    void zoom(double v);

    [[nodiscard]] const glm::uvec2& viewportSize() const;

    void set_viewport_size(const glm::uvec2& new_viewport_size);

private:
    [[nodiscard]] glm::dvec3 operationCentre() const;

private:
    glm::dmat4 m_projection_matrix;
    glm::dmat4 m_camera_transformation;
    float m_fov = 75;
    float m_near_clipping = 1.0;
    float m_far_clipping = 100'000;
    glm::uvec2 m_viewport_size = { 1, 1 };

};
}
