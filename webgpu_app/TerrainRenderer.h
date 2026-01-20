/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "GuiManager.h"
#include "InputMapper.h"
#include <SDL2/SDL.h>
#include <memory>

#include <nucleus/tile/GeometryScheduler.h>
#include <nucleus/tile/TextureScheduler.h>
#include <nucleus/timing/TimerManager.h>

#include <webgpu/webgpu.h>
#include <webgpu/timing/CpuTimer.h>
#include <webgpu/timing/GuiTimerManager.h>
#include <webgpu/timing/WebGpuTimer.h>

#include "webgpu_engine/Context.h"
#include "webgpu_engine/Window.h"

#include "RenderingContext.h"

namespace webgpu_app {

class TerrainRenderer : public QObject {
    Q_OBJECT

public:
    TerrainRenderer();
    ~TerrainRenderer() = default;

    struct GuiPipelineUBO {
        glm::vec2 resolution;
    };

    void init_window();
    void start();
    void poll_events();
    void render();
    void render_gui();
    void on_window_resize(int width, int height);
    void update_camera();

    [[nodiscard]] InputMapper* get_input_mapper() { return m_input_mapper.get(); }
    [[nodiscard]] GuiManager* get_gui_manager() { return m_gui_manager.get(); }
    [[nodiscard]] webgpu::timing::GuiTimerManager* get_timer_manager() { return m_timer_manager.get(); }
    [[nodiscard]] webgpu_engine::Window* get_webgpu_window() { return m_webgpu_window.get(); }
    [[nodiscard]] RenderingContext* get_rendering_context() { return m_context.get(); }
    [[nodiscard]] nucleus::camera::Controller* get_camera_controller() { return m_camera_controller.get(); }

signals:
    void update_camera_requested();

private slots:
    void set_window_size(glm::uvec2 size);
    void handle_shortcuts(QKeyCombination key);
    void schedule_update();

private:
    SDL_Window* m_sdl_window;
    std::unique_ptr<webgpu_engine::Window> m_webgpu_window;
    std::unique_ptr<nucleus::camera::Controller> m_camera_controller;
    std::unique_ptr<RenderingContext> m_context;

    std::unique_ptr<InputMapper> m_input_mapper;
    std::unique_ptr<GuiManager> m_gui_manager;
    std::unique_ptr<webgpu::timing::GuiTimerManager> m_timer_manager;

    WGPUInstanceDescriptor m_instance_desc;

    WGPUInstance m_instance = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;
    WGPUTextureFormat m_surface_texture_format = WGPUTextureFormat::WGPUTextureFormat_Undefined; // Will be replaced at swapchain creation
    WGPUTextureFormat m_depth_texture_format = WGPUTextureFormat::WGPUTextureFormat_Depth24Plus;

    glm::uvec2 m_viewport_size = glm::uvec2(1280u, 1024u);
    bool m_initialized = false;
    GuiPipelineUBO m_gui_ubo_data = { glm::vec2(1280.0, 1024.0) };

    std::unique_ptr<webgpu::Framebuffer> m_framebuffer;
    void create_framebuffer(uint32_t width, uint32_t height);
    void configure_surface(uint32_t width, uint32_t height);

    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_gui_pipeline;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_gui_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroup> m_gui_bind_group;
    std::unique_ptr<webgpu::raii::RawBuffer<GuiPipelineUBO>> m_gui_ubo;

    WGPUQuerySetDescriptor m_timestamp_query_desc;
    WGPUQuerySet m_timestamp_queries;
    WGPUPassTimestampWrites m_timestamp_writes;
    std::unique_ptr<webgpu::raii::RawBuffer<uint64_t>> m_timestamp_resolve;
    std::unique_ptr<webgpu::raii::RawBuffer<uint64_t>> m_timestamp_result;

    std::shared_ptr<webgpu::timing::WebGpuTimer> m_gputimer;
    std::shared_ptr<webgpu::timing::CpuTimer> m_cputimer;

    bool m_force_repaint = false;
    bool m_force_repaint_once = false;
    uint32_t m_repaint_count = 0;
    uint32_t m_frame_count = 0;
    WGPUPresentMode m_surface_presentmode = WGPUPresentMode_Fifo; // WGPUPresentMode_Immediate;

    // Flag to exit the rendering loop
    bool m_window_open = true;

    void webgpu_create_context();
    void webgpu_release_context();
};

} // namespace webgpu_app
