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
//  float m_rotation_around_z = 0;
//  float m_rotation_around_x = 0;
//  glm::vec3 m_position;
  glm::mat4 m_matrix;
public:
  Camera(const glm::vec3& position, const glm::vec3& view_at_point);
  [[nodiscard]] glm::mat4 cameraMatrix() const;
  [[nodiscard]] glm::vec3 position() const;
  [[nodiscard]] glm::vec3 xAxis() const;
  void pan(const glm::vec2& v);
  void move(const glm::vec3& v);
  void orbit(const glm::vec3& centre, const glm::vec2& degrees);
};

