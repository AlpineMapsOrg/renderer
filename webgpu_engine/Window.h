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
#include "UniformBufferObjects.h"
#include "nucleus/AbstractRenderWindow.h"
#include "nucleus/camera/AbstractDepthTester.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/utils/ColourTexture.h"
#include <webgpu/Buffer.h>
#include <webgpu/raii/BindGroup.h>
#include <webgpu/webgpu.h>

class QOpenGLFramebufferObject;

namespace webgpu_engine {

class Window : public nucleus::AbstractRenderWindow, public nucleus::camera::AbstractDepthTester {
    Q_OBJECT
public:
    Window();

    ~Window() override;

    void set_context(Context* context);
    void initialise_gpu() override;
    void resize_framebuffer(int w, int h) override;
    void ready();
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

    void set_max_zoom_level(uint32_t max_zoom_level);

public slots:
    void update_camera(const nucleus::camera::Definition& new_definition) override;
    void update_debug_scheduler_stats([[maybe_unused]] const QString& stats) override { }
    void pick_value([[maybe_unused]] const glm::dvec2& screen_space_coordinate) override { }

    void request_redraw();
    void reload_shaders();
    void on_shadow_texture_updated(const QByteArray& data);

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

    std::unique_ptr<webgpu::raii::TextureWithSampler> create_shadow_texture(uint32_t width, uint32_t height, uint32_t mip_levels);

private:
    Context* m_context = nullptr;

    std::unique_ptr<webgpu::Buffer<uboSharedConfig>> m_shared_config_ubo;
    std::unique_ptr<webgpu::Buffer<uboCameraConfig>> m_camera_config_ubo;

    std::unique_ptr<webgpu::raii::BindGroup> m_shared_config_bind_group;
    std::unique_ptr<webgpu::raii::BindGroup> m_camera_bind_group;
    std::array<std::unique_ptr<webgpu::raii::BindGroup>, 2> m_compose_bind_groups;
    std::unique_ptr<webgpu::raii::BindGroup> m_depth_texture_bind_group;

    nucleus::camera::Definition m_camera;
    uint32_t m_max_zoom_level = 18;

    webgpu::FramebufferFormat m_gbuffer_format;
    std::unique_ptr<webgpu::Framebuffer> m_gbuffer;

    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_compose_pipeline;

    // ToDo: Swapchain should get a raii class and the size could be saved in there
    glm::vec2 m_swapchain_size = glm::vec2(0.0f);
    WGPUPresentMode m_swapchain_presentmode = WGPUPresentMode::WGPUPresentMode_Fifo;

    bool m_needs_redraw = true;
    uint32_t m_paint_number = 0;

    std::unique_ptr<webgpu::raii::TextureWithSampler> m_shadow_texture;
};

} // namespace webgpu_engine
