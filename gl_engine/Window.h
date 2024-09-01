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

#include <QOpenGLPaintDevice>
#include <QOpenGLWindow>
#include <QPainter>
#include <QVector3D>
#include <QMap>
#include <glm/glm.hpp>
#include <memory>

#include "UniformBuffer.h"
#include "UniformBufferObjects.h"
#include "helpers.h"
#include "nucleus/AbstractRenderWindow.h"
#include "nucleus/camera/AbstractDepthTester.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/track/GPX.h"

#include "nucleus/timing/TimerManager.h"

class QOpenGLTexture;
class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;

namespace gl_engine {

class TileManager;
class MapLabelManager;
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

    [[nodiscard]] float depth(const glm::dvec2& normalised_device_coordinates) override;
    [[nodiscard]] glm::dvec3 position(const glm::dvec2& normalised_device_coordinates) override;
    void destroy() override;
    void set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr&) override;
    [[nodiscard]] nucleus::camera::AbstractDepthTester* depth_tester() override;
    [[nodiscard]] nucleus::utils::ColourTexture::Format ortho_tile_compression_algorithm() const override;
    void updateCameraEvent();
    void set_permissible_screen_space_error(float new_error) override;
    void set_quad_limit(unsigned new_limit) override;

public slots:
    void update_camera(const nucleus::camera::Definition& new_definition) override;
    void update_debug_scheduler_stats(const QString& stats) override;
    void update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads) override;
    void shared_config_changed(gl_engine::uboSharedConfig ubo);
    void reload_shader();
#ifdef ALP_ENABLE_LABELS
    void update_labels(const nucleus::vector_tile::PointOfInterestTileCollection& visible_features, const std::vector<tile::Id>& removed_tiles) override;
#endif
    void pick_value(const glm::dvec2& screen_space_coordinates) override;

signals:
    void report_measurements(QList<nucleus::timing::TimerReport> values);

private:
    std::unique_ptr<TileManager> m_tile_manager; // needs opengl context
    std::unique_ptr<MapLabelManager> m_map_label_manager;

    std::unique_ptr<Framebuffer> m_gbuffer;
    std::unique_ptr<Framebuffer> m_decoration_buffer;
    std::unique_ptr<Framebuffer> m_atmospherebuffer;
    std::unique_ptr<Framebuffer> m_pickerbuffer;

    std::unique_ptr<SSAO> m_ssao;
    std::unique_ptr<ShadowMapping> m_shadowmapping;

    std::shared_ptr<UniformBuffer<uboSharedConfig>> m_shared_config_ubo; // needs opengl context
    std::shared_ptr<UniformBuffer<uboCameraConfig>> m_camera_config_ubo;
    std::shared_ptr<UniformBuffer<uboShadowConfig>> m_shadow_config_ubo;

    helpers::ScreenQuadGeometry m_screen_quad_geometry;

    nucleus::camera::Definition m_camera;

    int m_frame = 0;
    bool m_initialised = false;
    QString m_debug_text;
    QString m_debug_scheduler_stats;

    std::unique_ptr<nucleus::timing::TimerManager> m_timer;

};

} // namespace
