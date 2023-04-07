/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#include <QOpenGLPaintDevice>
#include <QOpenGLWindow>
#include <QPainter>
#include <QVector3D>
#include <chrono>
#include <glm/glm.hpp>
#include <memory>

#include "helpers.h"
#include "nucleus/AbstractRenderWindow.h"
#include "nucleus/camera/AbstractDepthTester.h"
#include "nucleus/camera/Definition.h"

class QOpenGLTexture;
class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
class TileManager;

namespace gl_engine {
class DebugPainter;
class ShaderManager;
class Framebuffer;
class Atmosphere;

class Window : public nucleus::AbstractRenderWindow, public nucleus::camera::AbstractDepthTester {
    Q_OBJECT
public:
    Window();
    ~Window() override;

    void initialise_gpu() override;
    void resize_framebuffer(int w, int h) override;
    void paint(QOpenGLFramebufferObject* framebuffer = nullptr) override;
    void paintOverGL(QPainter* painter);

    [[nodiscard]] float depth(const glm::dvec2& normalised_device_coordinates) override;
    [[nodiscard]] glm::dvec3 position(const glm::dvec2& normalised_device_coordinates) override;
    void deinit_gpu() override;
    void set_aabb_decorator(const nucleus::tile_scheduler::AabbDecoratorPtr&) override;
    void add_tile(const std::shared_ptr<nucleus::Tile>&) override;
    void remove_tile(const tile::Id&) override;
    [[nodiscard]] nucleus::camera::AbstractDepthTester* depth_tester() override;
    void keyPressEvent(QKeyEvent*);
    void keyReleaseEvent(QKeyEvent*);
    void updateCameraEvent();
    void set_permissible_screen_space_error(float new_error) override;

public slots:
    void update_camera(const nucleus::camera::Definition& new_definition) override;
    void update_debug_scheduler_stats(const QString& stats) override;

private:
    using ClockResolution = std::chrono::microseconds;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock, ClockResolution>;

    std::unique_ptr<TileManager> m_tile_manager; // needs opengl context
    std::unique_ptr<DebugPainter> m_debug_painter; // needs opengl context
    std::unique_ptr<Atmosphere> m_atmosphere; // needs opengl context
    std::unique_ptr<ShaderManager> m_shader_manager;
    std::unique_ptr<Framebuffer> m_framebuffer;
    std::unique_ptr<Framebuffer> m_depth_buffer;
    gl_engine::helpers::ScreenQuadGeometry m_screen_quad_geometry;

    nucleus::camera::Definition m_camera;

    int m_frame = 0;
    bool m_initialised = false;
    TimePoint m_frame_start;
    TimePoint m_frame_end;
    QString m_debug_text;
    QString m_debug_scheduler_stats;
};
}
