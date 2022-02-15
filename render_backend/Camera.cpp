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

#include <glm/gtx/transform.hpp>
#include <QDebug>

#include "render_backend/utils/geometry.h"

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

Camera::Camera(const glm::dvec3& position, const glm::dvec3& view_at_point)// : m_position(position)
{
  m_camera_transformation = glm::inverse(glm::lookAt(position, view_at_point, {0, 0, 1}));
  if (std::isnan(m_camera_transformation[0][0])) {
    m_camera_transformation = glm::inverse(glm::lookAt(position, view_at_point, {0, 1, 0}));
  }

  setPerspectiveParams(45, {1, 1});
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

glm::dvec3 Camera::unproject(const glm::dvec2& screen_space_position) const
{
  //    At first I was a bit surprised, that we don't need to subtract the camera position, so I'll explain why that is (hopefully correctly):
  //    Homogenous coordinates use a non-zero number for the w coordinate, if it is a point, and zero if it is a vector (a direction). The cannonical
  // form uses w == 1, and you can convert into cannonical form by dividing the whole vec4 by w. if w == 0, then translation has no effect (makes
  // sense for vectors / directions.
  //    The perspective projection needs a division by the z-component (in camera space, i.e. the point projected on the (negative) z-axis). The
  // perspective transformation matrix achieves that by mapping z onto w. when putting the homogenious coordinate back into cannonical form, we divide
  // by w => after perspective projection transform we divide by z.
//        const auto wvpm = worldViewProjectionMatrix();  // camera is positioned at 2, 1, 0, and looks in direction of x+, and with z+ up
//        const auto p0 = wvpm * glm::dvec4(4, 2, 2, 0);  // gives (-2, 2, 4, 4) => (-0.5, 0.5, 1, 1 ) in cannonical form
//        const auto p1 = wvpm * glm::dvec4(6, 3, 2, 1);  // gives (-2, 2, 3.9, 4) =>  (-0.5, 0.5, 0.975, 1) in cannonical form
  // p0 is a transformed vector, and p1 is a transformed point. p0 and p1 have the same screen space coordinates (x and y), and w returns the projected
  // distance on the z-axis. I don't understand, what the significance of the z axis is, but for vectors it seems to be 1 always, while for points it
  // is something that can be used in the depth buffer for sorting (so it's a stricticly increasing function of the distance)
  //    Anyways, we can use the vector form, and backproject into worldspace. in that case we don't need to subtract the camera position.
  // Alternatively, we could use e.g.: unprojection_matrix * glm::dvec4(screen_space_position.x, screen_space_position.y, 0.9, 1), divide by 2
  // to put it in cannonical form, subtract the camera position and nomralise.
  //
  // based on https://gabrielgambetta.com/computer-graphics-from-scratch/09-perspective-projection.html and
  // https://gabrielgambetta.com/computer-graphics-from-scratch/10-describing-and-rendering-a-scene.html
  const auto unprojection_matrix = glm::inverse(worldViewProjectionMatrix());
  return glm::normalize((unprojection_matrix * glm::dvec4(screen_space_position.x, screen_space_position.y, 1, 1)).xyz());
}

std::vector<geometry::Plane<double> > Camera::clippingPlanes() const
{
  const auto clippingPane = [this](const glm::dvec2& a, const glm::dvec2& b) {
      const auto v_a = unproject(a);
      const auto v_b = unproject(b);
      const auto normal = glm::normalize(cross(v_a, v_b));
      const auto distance = - dot(normal, position());
      return geometry::Plane<double>{normal, distance};
    };
  std::vector<geometry::Plane<double> > clipping_panes;
  // front and back
  const auto p0 = position() + -zAxis() * m_near_clipping;
  clipping_panes.push_back({.normal = -zAxis(), .distance = - dot(-zAxis(), p0)});
  const auto p1 = position() + -zAxis() * m_far_clipping;
  clipping_panes.push_back({.normal = zAxis(), .distance = - dot(zAxis(), p1)});

  // top and down
  clipping_panes.push_back(clippingPane({-1, 1}, {1, 1}));
  clipping_panes.push_back(clippingPane({1, -1}, {-1, -1}));

  // left and right
  clipping_panes.push_back(clippingPane({-1, -1}, {-1, 1}));
  clipping_panes.push_back(clippingPane({1, 1}, {1, -1}));
  return clipping_panes;
}

void Camera::setPerspectiveParams(float fov_degrees, const glm::uvec2& viewport_size)
{
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
