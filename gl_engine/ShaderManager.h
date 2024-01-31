 /*****************************************************************************
 * Alpine Terrain Renderer
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

class ShaderManager : public QObject {
    Q_OBJECT
public:
    ShaderManager();
    ~ShaderManager() override;
    [[nodiscard]] ShaderProgram* tile_shader() const            { return m_tile_program.get(); }
    [[nodiscard]] ShaderProgram* screen_copy_program() const { return m_screen_copy.get(); }
    [[nodiscard]] ShaderProgram* atmosphere_bg_program() const  { return m_atmosphere_bg_program.get(); }
    [[nodiscard]] ShaderProgram* compose_program() const        { return m_compose_program.get(); }
    [[nodiscard]] ShaderProgram* ssao_program() const           { return m_ssao_program.get(); }
    [[nodiscard]] ShaderProgram* ssao_blur_program() const      { return m_ssao_blur_program.get(); }
    [[nodiscard]] ShaderProgram* shadowmap_program() const      { return m_shadowmap_program.get(); }
    [[nodiscard]] ShaderProgram* labels_program() const         { return m_labels_program.get(); }
    [[nodiscard]] ShaderProgram* track_program() const          { return m_track_program.get(); }
    [[nodiscard]] std::vector<ShaderProgram*> all() const       { return m_program_list; }
    std::shared_ptr<ShaderProgram> shared_ssao_program()        { return m_ssao_program; }
    std::shared_ptr<ShaderProgram> shared_ssao_blur_program()   { return m_ssao_blur_program; }
    std::shared_ptr<ShaderProgram> shared_shadowmap_program()   { return m_shadowmap_program; }
    void release();
public slots:
    void reload_shaders();
signals:

private:
    std::vector<ShaderProgram*> m_program_list;
    std::unique_ptr<ShaderProgram> m_tile_program;
    std::unique_ptr<ShaderProgram> m_screen_copy;
    std::unique_ptr<ShaderProgram> m_atmosphere_bg_program;
    std::unique_ptr<ShaderProgram> m_compose_program;
    std::shared_ptr<ShaderProgram> m_ssao_program;
    std::shared_ptr<ShaderProgram> m_ssao_blur_program;
    std::shared_ptr<ShaderProgram> m_shadowmap_program;
    std::shared_ptr<ShaderProgram> m_track_program;
    std::shared_ptr<ShaderProgram> m_labels_program;
};
}
