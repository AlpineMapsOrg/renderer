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

#include <glm/glm.hpp>

class Camera
{
  glm::mat4 m_projection_matrix;
  glm::mat4 m_camera_transformation;
public:
  Camera(const glm::vec3& position, const glm::vec3& view_at_point);
  [[nodiscard]] glm::mat4 cameraMatrix() const;
  [[nodiscard]] glm::mat4 viewProjectionMatrix() const;
  [[nodiscard]] glm::vec3 position() const;
  [[nodiscard]] glm::vec3 xAxis() const;
  [[nodiscard]] glm::vec3 negativeZAxis() const;
  void setPerspectiveParams(float fov_degrees, int viewport_width, int viewport_height);
  void pan(const glm::vec2& v);
  void move(const glm::vec3& v);
  void orbit(const glm::vec3& centre, const glm::vec2& degrees);
  // orbits around the intersection of negative z and 0 plane (temprorary only, until we can read the depth buffer)
  void orbit(const glm::vec2& degrees);
  void zoom(float v);

private:
  glm::vec3 operationCentre() const;
};

