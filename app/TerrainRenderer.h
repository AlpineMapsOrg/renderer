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

#include <QTimer>
#include <glm/glm.hpp>
#include <memory>
#include <optional>

#include <QObject>
#include <QQuickFramebufferObject>

namespace gl_engine {
class Window;
}
namespace nucleus {
class Controller;
}
namespace nucleus::tile::cache_queries {
class DemMeshSampler;
}

namespace nucleus::camera {
class Controller;
}
namespace nucleus::avalanche {
class ReportLoadService;
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

    [[nodiscard]] std::shared_ptr<nucleus::avalanche::ReportLoadService> eaws_report_load_service();

private:
    // the last camera view (position + orientation, i.e. the model matrix) is
    // persisted and restored on startup, so the app reopens where it was left
    [[nodiscard]] std::optional<glm::dmat4> load_last_camera() const;
    void save_last_camera() const;

    QQuickWindow* m_window = nullptr;
    std::unique_ptr<gl_engine::Window> m_glWindow;
    std::unique_ptr<nucleus::camera::Controller> m_camera_controller;
    // decode-once dem sampler for the camera's navigation floor (must outlive
    // the controller; used only on the render thread)
    std::unique_ptr<nucleus::tile::cache_queries::DemMeshSampler> m_terrain_floor_sampler;
    std::shared_ptr<nucleus::avalanche::ReportLoadService> m_eaws_report_load_service;
    // debounces the camera persistence while the camera moves
    QTimer m_camera_autosave;
};
