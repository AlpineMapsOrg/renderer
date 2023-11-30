 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#include <QOpenGLPaintDevice>
#include <QOpenGLWindow>
#include <QPainter>
#include <QVector3D>
#include <QMap>
#include <chrono>
#include <glm/glm.hpp>
#include <memory>

#include "helpers.h"
#include "nucleus/AbstractRenderWindow.h"
#include "nucleus/camera/AbstractDepthTester.h"
#include "nucleus/camera/Definition.h"
#include "UniformBufferObjects.h"
#include "UniformBuffer.h"

#include "nucleus/timing/TimerManager.h"

class QOpenGLTexture;
class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
class TileManager;

namespace gl_engine {

class DebugPainter;
class ShaderManager;
class Framebuffer;
class SSAO;
class ShadowMapping;

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
    void set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr&) override;
    void remove_tile(const tile::Id&) override;
    [[nodiscard]] nucleus::camera::AbstractDepthTester* depth_tester() override;
    void keyPressEvent(QKeyEvent*);
    void keyReleaseEvent(QKeyEvent*);
    void updateCameraEvent();
    void set_permissible_screen_space_error(float new_error) override;

public slots:
    void update_camera(const nucleus::camera::Definition& new_definition) override;
    void update_debug_scheduler_stats(const QString& stats) override;
    void update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads) override;
    void key_press(const QKeyCombination& e); // Slot to connect key-events to
    void shared_config_changed(gl_engine::uboSharedConfig ubo);
    void render_looped_changed(bool render_looped_flag);
    void reload_shader();

signals:
    void report_measurements(QList<nucleus::timing::TimerReport> values);

private:
    std::unique_ptr<TileManager> m_tile_manager; // needs opengl context
    std::unique_ptr<DebugPainter> m_debug_painter; // needs opengl context
    std::unique_ptr<ShaderManager> m_shader_manager;

    std::unique_ptr<Framebuffer> m_gbuffer;
    std::unique_ptr<Framebuffer> m_atmospherebuffer;

    std::unique_ptr<SSAO> m_ssao;
    std::unique_ptr<ShadowMapping> m_shadowmapping;

    std::shared_ptr<UniformBuffer<uboSharedConfig>> m_shared_config_ubo; // needs opengl context
    std::shared_ptr<UniformBuffer<uboCameraConfig>> m_camera_config_ubo;
    std::shared_ptr<UniformBuffer<uboShadowConfig>> m_shadow_config_ubo;

    helpers::ScreenQuadGeometry m_screen_quad_geometry;

    nucleus::camera::Definition m_camera;

    int m_frame = 0;
    bool m_initialised = false;
    bool m_render_looped = false;
    bool m_wireframe_enabled = false;
    QString m_debug_text;
    QString m_debug_scheduler_stats;

    std::unique_ptr<nucleus::timing::TimerManager> m_timer;

};

} // namespace
