/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2026 Wendelin Muth
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

#include "Context.h"
#include "PipelineManager.h"
#include "PipelineSettings.h"
#include "ShaderModuleManager.h"
#include "TrackRenderer.h"
#include "UniformBufferObjects.h"
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include "compute/imgui/NodeGraphRenderer.h"
#endif
#include "compute/NodeGraph.h"
#include "nucleus/AbstractRenderWindow.h"
#include "nucleus/camera/AbstractDepthTester.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/track/GPX.h"
#include "nucleus/utils/ColourTexture.h"
#include <webgpu/raii/BindGroup.h>
#include <webgpu/webgpu.h>

class QOpenGLFramebufferObject;

namespace webgpu_engine {

#define DEFAULT_GPX_TRACK_PATH ":/gpx/breite_ries.gpx"

struct GuiErrorState {
    bool should_open_modal = false;
    std::string text = "";
};

class Window : public nucleus::AbstractRenderWindow, public nucleus::camera::AbstractDepthTester {
    Q_OBJECT
public:
    enum class ComputePipelineType {
        NORMALS = 0,
        SNOW = 1,
        AVALANCHE_TRAJECTORIES = 2,
        RELEASE_POINTS = 3,
        ITERATIVE_SIMULATION = 4,
    };

public:
    Window();

    ~Window() override;

    void set_wgpu_context(WGPUInstance instance, WGPUDevice device, WGPUAdapter adapter, WGPUSurface surface, WGPUQueue queue, Context* context);
    void initialise_gpu() override;
    void resize_framebuffer(int w, int h) override;
    void paint(webgpu::Framebuffer* framebuffer, WGPUCommandEncoder command_encoder);
    // void paint(WGPUTextureView target_color_texture, WGPUTextureView target_depth_texture, WGPUCommandEncoder encoder);
    void paint([[maybe_unused]] QOpenGLFramebufferObject* framebuffer = nullptr) override { throw std::runtime_error("Not implemented"); }

    [[nodiscard]] float depth(const glm::dvec2& normalised_device_coordinates) override;
    [[nodiscard]] glm::dvec3 position(const glm::dvec2& normalised_device_coordinates) override;
    void destroy() override;
    [[nodiscard]] nucleus::camera::AbstractDepthTester* depth_tester() override;
    nucleus::utils::ColourTexture::Format ortho_tile_compression_algorithm() const override;
    bool needs_redraw() { return m_needs_redraw; }

    void update_required_gpu_limits(WGPULimits& limits, const WGPULimits& supported_limits);
    void paint_gui();
    void paint_compute_pipeline_gui();


    void set_max_zoom_level(uint32_t max_zoom_level);

public slots:
    void update_camera(const nucleus::camera::Definition& new_definition) override;
    void update_debug_scheduler_stats(const QString& stats) override;
    void pick_value(const glm::dvec2& screen_space_coordinate) override;

    void request_redraw();
    void load_track_and_focus(const std::string& path);
    void focus_region_3d(const radix::geometry::Aabb3d& aabb);
    void focus_region_2d(const radix::geometry::Aabb<2, double>& aabb);
    void reload_shaders();
    void on_pipeline_run_completed();
    void on_shadow_texture_updated(const QByteArray& data);

private slots:
    void file_upload_handler(const std::string& filename, const std::string& tag);

signals:
    void set_camera_definition_requested(nucleus::camera::Definition definition);

private:
    std::unique_ptr<webgpu::raii::RawBuffer<glm::vec4>> m_position_readback_buffer;
    glm::vec4 m_last_position_readback;

    void create_buffers();
    void create_bind_groups();
    void recreate_compose_bind_group();

    // A helper function for the depth and position method.
    // ATTENTION: This function is synchronous and will hold rendering. Use with caution!
    // Note: Depth aswell as the position is saved in the gbuffer. In contrast to the gl version
    // we can directly readback the content of the position buffer and don't need the readback depth
    // buffer anymore. May actually increase performance as we don't need to fill the seperate buffer.
    glm::vec4 synchronous_position_readback(const glm::dvec2& normalised_device_coordinates);

    void select_last_loaded_track_region();
    void refresh_compute_pipeline_settings(const radix::geometry::Aabb3d& world_aabb, const nucleus::track::Point& focused_track_point_coords);
    void create_and_set_compute_pipeline(ComputePipelineType pipeline_type, bool should_recreate_compose_bind_group = true);
    void update_compute_pipeline_settings();
    void update_settings_and_rerun_pipeline(const std::string& entry_node = "");

    std::unique_ptr<webgpu::raii::TextureWithSampler> create_overlay_texture(unsigned int width, unsigned int height, bool linear_interpolation = true);
    void update_image_overlay_texture(const std::string& image_file_path);
    bool update_image_overlay_aabb(const radix::geometry::Aabb<2, double>& aabb);
    void update_image_overlay_aabb_and_focus(const std::string& aabb_file_path);

    void clear_compute_overlay();
    void update_compute_overlay_texture(const webgpu::raii::TextureWithSampler& texture_with_sampler);
    void update_compute_overlay_aabb(const radix::geometry::Aabb<2, double>& aabb);


    void after_first_frame();

    void display_message(const std::string& message);

    std::unique_ptr<webgpu::raii::TextureWithSampler> create_shadow_texture(uint32_t width, uint32_t height, uint32_t mip_levels);

private:
    WGPUInstance m_instance = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUQueue m_queue = nullptr;
    Context* m_context = nullptr;

    std::unique_ptr<Buffer<uboSharedConfig>> m_shared_config_ubo;
    std::unique_ptr<Buffer<uboCameraConfig>> m_camera_config_ubo;

    std::unique_ptr<webgpu::raii::BindGroup> m_shared_config_bind_group;
    std::unique_ptr<webgpu::raii::BindGroup> m_camera_bind_group;
    std::array<std::unique_ptr<webgpu::raii::BindGroup>, 2> m_compose_bind_groups;
    std::unique_ptr<webgpu::raii::BindGroup> m_depth_texture_bind_group;

    nucleus::camera::Definition m_camera;
    uint32_t m_max_zoom_level = 18;

    webgpu::FramebufferFormat m_gbuffer_format;
    std::unique_ptr<webgpu::Framebuffer> m_gbuffer;

    std::unique_ptr<webgpu::Framebuffer> m_atmosphere_framebuffer;

    // ToDo: Swapchain should get a raii class and the size could be saved in there
    glm::vec2 m_swapchain_size = glm::vec2(0.0f);
    WGPUPresentMode m_swapchain_presentmode = WGPUPresentMode::WGPUPresentMode_Fifo;

    bool m_needs_redraw = true;
    bool m_first_paint = true;
    bool m_is_first_pipeline_run = true;
    uint32_t m_paint_number = 0;
    std::string m_last_dialog_directory = ".";

    std::unique_ptr<TrackRenderer> m_track_renderer;

    std::unique_ptr<compute::nodes::NodeGraph> m_compute_graph;
    ComputePipelineType m_active_compute_pipeline_type;
    ComputePipelineSettings m_compute_pipeline_settings;
    bool m_is_region_selected = false;
    GuiErrorState m_gui_error_state;

    std::unique_ptr<webgpu::raii::TextureWithSampler> m_image_overlay_texture;
    std::unique_ptr<Buffer<ImageOverlaySettings>> m_image_overlay_settings_uniform_buffer;
    std::string m_image_overlay_texture_path;
    bool m_image_overlay_linear_interpolation = true;

    std::unique_ptr<webgpu::raii::TextureWithSampler> m_compute_overlay_dummy_texture;
    std::unique_ptr<Buffer<ImageOverlaySettings>> m_compute_overlay_settings_uniform_buffer;

    const webgpu::raii::TextureView* m_compute_overlay_texture_view = nullptr; // will be set to correct texture view after pipeline run completion
    const webgpu::raii::Sampler* m_compute_overlay_sampler = nullptr; // will be set to correct sampler after pipeline run completion

    std::unique_ptr<webgpu::raii::TextureWithSampler> m_shadow_texture;

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    std::unique_ptr<compute::NodeGraphRenderer> m_node_graph_renderer;
    bool m_should_render_node_graph = false;
#endif


};

} // namespace webgpu_engine
