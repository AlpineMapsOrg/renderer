 /*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Adam Celerek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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
#include <memory>

// consider removing. the only thing it does atm is a shader list + reloading. erm, so maybe rename into shader reloader..
namespace gl_engine {
class ShaderProgram;

class ShaderRegistry : public QObject {
    Q_OBJECT
public:
    ShaderRegistry();
    ~ShaderRegistry() override;
    [[nodiscard]] std::vector<std::weak_ptr<ShaderProgram>> all() const { return m_program_list; }
    void add_shader(std::weak_ptr<ShaderProgram> shader);
    void reload_shaders();
private:
    std::vector<std::weak_ptr<ShaderProgram>> m_program_list;
};
}
