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

#include <memory>
#include <QObject>
#include <QOpenGLShaderProgram>

#include "alpine_gl_renderer/TileGLLocations.h"

//QT_BEGIN_NAMESPACE
//class QOpenGLShaderProgram;
//QT_END_NAMESPACE

class GLShaderManager : public QObject
{
  Q_OBJECT
public:
  explicit GLShaderManager();
  ~GLShaderManager() override;
  void bindTileShader();
  void bindDebugShader();
  QOpenGLShaderProgram* tileShader() const;
  void release();
  [[nodiscard]] TileGLAttributeLocations tileAttributeLocations() const;
  [[nodiscard]] TileGLUniformLocations tileUniformLocations() const;
signals:

private:
  std::unique_ptr<QOpenGLShaderProgram> m_tile_program = nullptr;
  std::unique_ptr<QOpenGLShaderProgram> m_debug_program = nullptr;
  TileGLUniformLocations m_tile_uniform_location = {};
  TileGLAttributeLocations m_tile_attribute_locations = {};
};

