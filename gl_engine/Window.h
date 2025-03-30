/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Jakob Maier
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

#include "UniformBuffer.h"
#include "UniformBufferObjects.h"
#include "helpers.h"
#include <QMap>
#include <QOpenGLWindow>
#include <QPainter>
#include <QVariantMap>
#include <QVector3D>
#include <glm/glm.hpp>
#include <memory>
#include <nucleus/AbstractRenderWindow.h>
#include <nucleus/camera/AbstractDepthTester.h>
#include <nucleus/camera/Definition.h>
#include <nucleus/timing/TimerManager.h>
#include <nucleus/track/GPX.h>
#include <string>

class QOpenGLTexture;
class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;

namespace gl_engine {

class MapLabels;
class ShaderProgram;
class Framebuffer;
class SSAO;
class ShadowMapping;
class Context;

class Window : public nucleus::AbstractRenderWindow, public nucleus::camera::AbstractDepthTester {
    Q_OBJECT
public:
    Window(std::shared_ptr<Context> context);
    ~Window() override;

    void initialise_gpu() override;
    void resize_framebuffer(int w, int h) override;
    void paint(QOpenGLFramebufferObject* framebuffer = nullptr) override;

    [[nodiscard]] float depth(const glm::dvec2& normalised_device_coordinates) override;
    [[nodiscard]] glm::dvec3 position(const glm::dvec2& normalised_device_coordinates) override;
    void destroy() override;
    [[nodiscard]] nucleus::camera::AbstractDepthTester* depth_tester() override;
    [[nodiscard]] nucleus::utils::ColourTexture::Format ortho_tile_compression_algorithm() const override;

public slots:
    void update_camera(const nucleus::camera::Definition& new_definition) override;
    void update_debug_scheduler_stats(const QString& stats) override;
    void shared_config_changed(gl_engine::uboSharedConfig ubo);
    void reload_shader();
    void pick_value(const glm::dvec2& screen_space_coordinates) override;

signals:
    void timer_measurements_ready(QList<nucleus::timing::TimerReport> values);
    void tile_stats_ready(QVariantMap stats);

private:
    std::shared_ptr<Context> m_context;
    std::unique_ptr<MapLabels> m_map_label_manager;

    std::unique_ptr<Framebuffer> m_gbuffer;
    std::unique_ptr<Framebuffer> m_decoration_buffer;
    std::unique_ptr<Framebuffer> m_atmospherebuffer;
    std::unique_ptr<Framebuffer> m_pickerbuffer;

    std::shared_ptr<ShaderProgram> m_atmosphere_shader;
    std::shared_ptr<ShaderProgram> m_compose_shader;
    std::shared_ptr<ShaderProgram> m_screen_copy_shader;
    std::unique_ptr<SSAO> m_ssao;
    std::unique_ptr<ShadowMapping> m_shadowmapping;

    std::shared_ptr<UniformBuffer<uboSharedConfig>> m_shared_config_ubo; // needs opengl context
    std::shared_ptr<UniformBuffer<uboCameraConfig>> m_camera_config_ubo;
    std::shared_ptr<UniformBuffer<uboShadowConfig>> m_shadow_config_ubo;

    helpers::ScreenQuadGeometry m_screen_quad_geometry;

    nucleus::camera::Definition m_camera;

    int m_frame = 0;
    bool m_initialised = false;
    float m_permissible_screen_space_error = 2.f;
    QString m_debug_text;
    QString m_debug_scheduler_stats;

    std::unique_ptr<nucleus::timing::TimerManager> m_timer;

};

} // namespace
