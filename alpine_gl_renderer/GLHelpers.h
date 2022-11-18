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

#include <QMatrix4x4>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace gl_helpers {
inline QMatrix4x4 toQtType(const glm::mat4& mat)
{
    return QMatrix4x4(glm::value_ptr(mat)).transposed();
}
inline QVector4D toQtType(const glm::vec4& v)
{
    return QVector4D(v.x, v.y, v.z, v.w);
}
inline QVector3D toQtType(const glm::vec3& v)
{
    return QVector3D(v.x, v.y, v.z);
}
inline QVector2D toQtType(const glm::vec2& v)
{
    return QVector2D(v.x, v.y);
}
template <typename T>
T toQtType(const T& v)
{
    return v;
}


template <typename T>
inline int bufferLengthInBytes(const std::vector<T>& vec)
{
    return int(vec.size() * sizeof(T));
}
}
