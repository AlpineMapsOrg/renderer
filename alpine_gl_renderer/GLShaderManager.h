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

// QT_BEGIN_NAMESPACE
// class QOpenGLShaderProgram;
// QT_END_NAMESPACE
class ShaderProgram;

class GLShaderManager : public QObject {
    Q_OBJECT
public:
    explicit GLShaderManager();
    ~GLShaderManager() override;
    void bindTileShader();
    void bindDebugShader();
    [[nodiscard]] ShaderProgram* tileShader() const;
    [[nodiscard]] ShaderProgram* debugShader() const;
    [[nodiscard]] ShaderProgram* screen_quad_program() const;
    void release();
signals:

private:
    std::unique_ptr<ShaderProgram> m_tile_program;
    std::unique_ptr<ShaderProgram> m_debug_program;
    std::unique_ptr<ShaderProgram> m_screen_quad_program ;
};
