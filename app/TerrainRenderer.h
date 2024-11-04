/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Adam Celarek
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
#include <QQuickFramebufferObject>

namespace gl_engine {
class Window;
}
namespace nucleus {
class Controller;
}
namespace nucleus::camera {
class Controller;
}

class TerrainRenderer : public QObject, public QQuickFramebufferObject::Renderer {
    Q_OBJECT
public:
    TerrainRenderer();
    ~TerrainRenderer() override;

    void synchronize(QQuickFramebufferObject *item) override;

    void render() override;

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;

    [[nodiscard]] gl_engine::Window* glWindow() const;

    [[nodiscard]] nucleus::camera::Controller* controller() const;

private:
    QQuickWindow* m_window = nullptr;
    std::unique_ptr<gl_engine::Window> m_glWindow;
    std::unique_ptr<nucleus::camera::Controller> m_camera_controller;
};
