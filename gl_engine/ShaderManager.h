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

#include <QObject>
#include <memory>

// consider removing. the only thing it does atm is a shader list + reloading. erm, so maybe rename into shader reloader..
namespace gl_engine {
class ShaderProgram;

class ShaderManager : public QObject {
    Q_OBJECT
public:
    ShaderManager();
    ~ShaderManager() override;
    [[nodiscard]] ShaderProgram* tileShader() const;
    [[nodiscard]] ShaderProgram* debugShader() const;
    [[nodiscard]] ShaderProgram* screen_quad_program() const;
    [[nodiscard]] ShaderProgram* atmosphere_bg_program() const;
    void release();
public slots:
    void reload_shaders();
signals:

private:
    std::vector<ShaderProgram*> m_program_list;
    std::unique_ptr<ShaderProgram> m_tile_program;
    std::unique_ptr<ShaderProgram> m_debug_program;
    std::unique_ptr<ShaderProgram> m_screen_quad_program;
    std::unique_ptr<ShaderProgram> m_atmosphere_bg_program;
};
}
