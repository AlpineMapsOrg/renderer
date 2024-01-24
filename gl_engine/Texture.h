/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
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

#include <qopengl.h>

namespace gl_engine {
class Texture {
public:
    enum class Target : GLenum { _2d = GL_TEXTURE_2D, _2dArray = GL_TEXTURE_2D_ARRAY };

public:
    Texture(Target target);
    ~Texture();

    void bind(unsigned texture_unit);

private:
    GLuint m_id = -1;
    Target m_target = Target::_2d;
};
} // namespace gl_engine
