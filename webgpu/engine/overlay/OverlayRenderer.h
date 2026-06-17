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
#include <QObject>
#include <array>
#include <memory>
#include <vector>
#include <webgpu/base/Context.h>
#include <webgpu/base/raii/TextureView.h>
#include <webgpu/base/raii/TextureWithSampler.h>
#include <webgpu/webgpu.h>

namespace webgpu_engine {

class Context;

class OverlayRenderer : public QObject {
    Q_OBJECT
public:
    explicit OverlayRenderer();

    void add_overlay(std::shared_ptr<Overlay> overlay);
    void remove_overlay(size_t index);
    // Re-sort m_overlays by z_index after the GUI has assigned new z_indices.
    // IMPORTANT: Call after any change to the overlay set or its z_indices.
    void sort_overlays();

    [[nodiscard]] const std::vector<std::shared_ptr<Overlay>>& overlays() const;

    void init(Context& ctx);
    // Called once after all GPU resources are created (and the initial setup is done).
    void ready(webgpu::Context& ctx);
    void resize(int w, int h);

    void draw(const WGPUCommandEncoder& command_encoder,
        const webgpu::raii::TextureView& position_view,
        const webgpu::raii::TextureView& normal_view,
        const webgpu::raii::TextureView& overlay_view,
        const WGPUBindGroup& shared_config_bg,
        const WGPUBindGroup& camera_bg);

    [[nodiscard]] const webgpu::raii::TextureView* result_pre_view() const;
    [[nodiscard]] const webgpu::raii::TextureView* result_post_view() const;

private:
    using TexturePair = std::array<std::unique_ptr<webgpu::raii::TextureWithSampler>, 2>;

    std::unique_ptr<webgpu::raii::TextureWithSampler> create_output_texture(int w, int h, const char* label) const;
    // Refill m_pre_overlays / m_post_overlays from m_overlays (z_index < 0 -> pre otherwise post),
    // preserving order.
    void rebucket();

    void draw_bucket(const WGPUCommandEncoder& command_encoder,
        const std::vector<Overlay*>& bucket,
        TexturePair& tex,
        const webgpu::raii::TextureView& position_view,
        const webgpu::raii::TextureView& normal_view,
        const webgpu::raii::TextureView& overlay_view,
        const WGPUBindGroup& shared_config_bg,
        const WGPUBindGroup& camera_bg,
        glm::uvec2 output_size);

    Context* m_ctx = nullptr;
    bool m_is_ready = false;
    std::vector<std::shared_ptr<Overlay>> m_overlays; // owning; sorted by z_index ascending
    // Per-bucket, non-owning views into m_overlays (non-canonical, rebuilt by rebucket()), iterated directly by draw().
    std::vector<Overlay*> m_pre_overlays; // z_index < 0
    std::vector<Overlay*> m_post_overlays; // z_index >= 0
    // Ping-pong textures per bucket. Index 0 is the persistent result
    TexturePair m_pre;
    TexturePair m_post;
};

} // namespace webgpu_engine
