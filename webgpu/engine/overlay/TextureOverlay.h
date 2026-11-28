/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "Overlay.h"
#include <QString>
#include <functional>
#include <memory>
#include <radix/geometry.h>
#include <radix/raster.h>
#include <webgpu/base/Buffer.h>
#include <webgpu/base/Framebuffer.h>
#include <webgpu/base/raii/Pipeline.h>
#include <webgpu/base/raii/TextureWithSampler.h>

namespace webgpu_engine {

class TextureOverlay : public Overlay {
public:
    enum class FilterMode { Nearest, Linear };
    enum class Mode { AlphaBlend, EncodedFloat };
    struct Settings {
        radix::geometry::Aabb<2, double> aabb = { { 0.0, 0.0 }, { 1.0, 1.0 } }; // world-space extent
        float opacity = 1.0f;
        Mode mode = Mode::AlphaBlend;
        glm::vec2 float_decode_range = glm::vec2(0.0f, 20.0f); // [lower, upper] for EncodedFloat mode
        FilterMode filter_mode = FilterMode::Linear; // filter_mode/use_mipmaps take effect on next load_image
        bool use_mipmaps = true;
    };
    Settings settings;

    TextureOverlay();

    // Load an RGBA8 image from disk into the overlays own texture.
    void load_image(const QString& path);

    // Copy an external GPU texture into the overlays own texture.
    void load_texture(const webgpu::raii::TextureWithSampler& source);

    // Link an external GPU texture directly (non-owning). The caller must keep it alive while linked.
    // pass nullptr to unlink.
    void link_texture(const webgpu::raii::TextureWithSampler* texture);
    // Link whatever texture `source` returns, asked every frame (non-owning). For a texture its owner
    // may replace at any time, e.g. a compute node's output: the overlay never holds on to a freed
    // one. nullptr from the source = nothing to draw this frame. An empty function unlinks.
    using TextureSource = std::function<const webgpu::raii::TextureWithSampler*()>;
    void link_texture_source(TextureSource source);
    [[nodiscard]] bool is_linked() const { return bool(m_linked_source); }

    bool can_draw() const override;

    void init(Context& ctx) override;
    void ready(webgpu::Context& ctx) override;
    void update_gpu_settings();
    void draw(const WGPUCommandEncoder& command_encoder,
        const webgpu::raii::TextureView& position_view,
        const webgpu::raii::TextureView& normal_view,
        const webgpu::raii::TextureView& overlay_view,
        const WGPUBindGroup& shared_config_bg,
        const WGPUBindGroup& camera_bg,
        const webgpu::raii::TextureWithSampler& current_input,
        webgpu::raii::TextureWithSampler& target_output,
        glm::uvec2 output_size) override;

private:
    struct GpuSettings {
        glm::vec2 aabb_min = glm::vec2(0.0f);
        glm::vec2 aabb_size = glm::vec2(1.0f);
        float opacity = 1.0f;
        uint32_t mode = 0u; // (0=AlphaBlend, 1=EncodedFloat)
        glm::vec2 float_decode_range = glm::vec2(0.0f, 20.0f); // user visualization range
        glm::vec2 encoded_float_range; // encoding format range (defined in geopng module)
    };

    void create_texture(webgpu::Context& ctx, uint32_t width, uint32_t height);

    webgpu::Context* m_ctx = nullptr;
    bool m_is_ready = false;

    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_pipeline;
    std::unique_ptr<webgpu::Buffer<GpuSettings>> m_settings_uniform;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_overlay_texture; // owned source (load_image/load_texture)
    TextureSource m_linked_source; // borrowed source (link_texture, link_texture_source)
    /// The texture to draw: the linked one, else the owned one; nullptr if there is none.
    const webgpu::raii::TextureWithSampler* current_texture() const;
};

} // namespace webgpu_engine
