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

#include <QObject>
#include <glm/glm.hpp>

#include "alpine_gl_renderer/GLVariableLocations.h"

class QOpenGLShaderProgram;

class GLDebugPainter : public QObject {
    Q_OBJECT
public:
    explicit GLDebugPainter(QObject* parent = nullptr);

    void setAttributeLocations(const DebugGLAttributeLocations& d);
    void setUniformLocations(const DebugGLUniformLocations& d);
    void activate(QOpenGLShaderProgram* shader_program, const glm::mat4& world_view_projection_matrix);
    void drawLineStrip(const std::vector<glm::vec3>& points) const;

signals:

private:
    DebugGLAttributeLocations m_attribute_locations;
    DebugGLUniformLocations m_uniform_locations;
};
