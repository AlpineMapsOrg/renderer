/*****************************************************************************
 * AlpineMaps.org
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

#include <QObject>

namespace gl_engine {
class ShaderRegistry;
class ShaderProgram;

class TextureLayer : public QObject {
    Q_OBJECT
public:
    explicit TextureLayer(QObject* parent = nullptr);
    void init(ShaderRegistry* shader_registry); // needs OpenGL context

private:
    std::shared_ptr<ShaderProgram> m_shader;
};
} // namespace gl_engine
