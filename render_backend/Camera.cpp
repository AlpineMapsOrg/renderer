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

Camera::Camera(const glm::vec3& position, const glm::vec3& view_at_point)// : m_position(position)
{
  m_matrix = glm::inverse(glm::lookAt(position, view_at_point, {0, 0, 1}));
//  const auto view_direction = glm::normalize(position - view_at_point);
//  m_rotation_around_x = std::acos(glm::dot(-view_direction, glm::vec3(0, 0, 1)));
//  const auto x_axis = glm::cross(view_direction, glm::vec3(0, 0, 1));
//  m_rotation_around_z = std::acos(glm::dot(x_axis, glm::vec3(0, 0, 1)));
}

glm::mat4 Camera::cameraMatrix() const
{
  return glm::inverse(m_matrix);
  //  return glm::rotate(m_rotation_around_x, glm::vec3(1, 0, 0)) * glm::rotate(m_rotation_around_z, glm::vec3(0, 0, 1)) * glm::translate(m_position);
}

glm::vec3 Camera::position() const
{
  return glm::vec3(m_matrix * glm::vec4(0, 0, 0, 1));
}

glm::vec3 Camera::xAxis() const
{
  return glm::vec3(m_matrix[0]);
}

void Camera::pan(const glm::vec2& v)
{
  const auto x_dir = xAxis();
  const auto y_dir = glm::cross(x_dir, glm::vec3(0, 0, 1));
  m_matrix = glm::translate(-1.f * (v.x * x_dir + v.y * y_dir)) * m_matrix;
}

void Camera::move(const glm::vec3& v)
{
  m_matrix = glm::translate(v) * m_matrix;
}

void Camera::orbit(const glm::vec3& centre, const glm::vec2& degrees)
{
//  pan(-centre);
//  const auto rotation_x_axis = glm::rotate(glm::radians(degrees.y), glm::vec3(1, 0, 0));
////  const auto rotation_x_axis2 = glm::rotate(-degrees.y, glm::vec3(1, 0, 0));
//  // z axis of world is y axis of camera
//  const auto rotation_z_axis = glm::rotate(glm::radians(-degrees.x), glm::vec3(0, 1, 0));
//  const auto rotation = rotation_z_axis * rotation_x_axis;
//  const auto camera_position = glm::vec3(m_matrix[3]);

//  m_matrix = glm::translate(camera_position) * rotation * glm::translate(-camera_position) * m_matrix;

//  pan(centre);
  move(centre);
//  const auto old_position = position();
//  move(old_position);
  const auto rotation_x_axis = glm::rotate(glm::radians(degrees.y), xAxis());
  //  const auto rotation_x_axis2 = glm::rotate(-degrees.y, glm::vec3(1, 0, 0));
  // z axis of world is y axis of camera
  const auto rotation_z_axis = glm::rotate(glm::radians(degrees.x), glm::vec3(0, 0, 1));
  const auto rotation = rotation_z_axis * rotation_x_axis;
//  const auto camera_position = glm::vec3(m_matrix[3]);

  m_matrix = rotation * m_matrix;

//  move(-old_position);
  move(-centre);
}
