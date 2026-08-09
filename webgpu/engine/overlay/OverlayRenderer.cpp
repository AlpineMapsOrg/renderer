/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
 * Copyright (C) 2025 Patrick Komon
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

#include "OverlayRenderer.h"

#include "webgpu/engine/Context.h"
#include <algorithm>
#include <memory>
#include <string>

namespace webgpu_engine {

OverlayRenderer::OverlayRenderer()
    : QObject { nullptr }
{
}

void OverlayRenderer::add_overlay(std::shared_ptr<Overlay> overlay)
{
    // Auto-assign the highest positive z_index
    int max_z = 0;
    for (const auto& o : m_overlays)
        max_z = std::max(max_z, o->z_index);
    overlay->z_index = max_z + 1;

    if (m_ctx) {
        overlay->init(*m_ctx);
        if (m_is_ready)
            overlay->ready(m_ctx->webgpu_ctx());
    }
    m_overlays.push_back(std::move(overlay));
    rebucket();
}

void OverlayRenderer::remove_overlay(size_t index)
{
    if (index < m_overlays.size()) {
        m_overlays.erase(m_overlays.begin() + static_cast<ptrdiff_t>(index));
        rebucket();
    }
}

const std::vector<std::shared_ptr<Overlay>>& OverlayRenderer::overlays() const { return m_overlays; }

void OverlayRenderer::sort_overlays()
{
    std::sort(m_overlays.begin(), m_overlays.end(), [](const auto& a, const auto& b) { return a->z_index < b->z_index; });
    rebucket();
}

void OverlayRenderer::rebucket()
{
    m_pre_overlays.clear();
    m_post_overlays.clear();
    for (const auto& overlay : m_overlays)
        (overlay->z_index < 0 ? m_pre_overlays : m_post_overlays).push_back(overlay.get());
}

void OverlayRenderer::init(Context& ctx)
{
    m_ctx = &ctx;

    for (auto& overlay : m_overlays)
        overlay->init(ctx);
}

void OverlayRenderer::ready(webgpu::Context& ctx)
{
    m_is_ready = true;
    for (auto& overlay : m_overlays)
        overlay->ready(ctx);
}

void OverlayRenderer::resize(int /*w*/, int /*h*/) { }

OverlayRenderer::GraphResult OverlayRenderer::draw(webgpu::rg::RenderGraph* render_graph,
    webgpu::rg::TextureHandle position,
    webgpu::rg::TextureHandle normal,
    webgpu::rg::TextureHandle overlay,
    const WGPUBindGroup& shared_config_bg,
    const WGPUBindGroup& camera_bg)
{
    auto run_bucket = [&](const std::vector<Overlay*>& bucket, std::string_view clear_id, std::string_view target_prefix) -> webgpu::rg::TextureHandle {
        const webgpu::rg::TextureDesc desc = webgpu::rg::texture_relative(WGPUTextureFormat_RGBA8Unorm, position, 1.0f);

        webgpu::rg::TextureHandle source = render_graph->create_initialized_texture(clear_id, desc, { 0.0, 0.0, 0.0, 0.0 });

        int stage = 0;
        for (Overlay* o : bucket) {
            const std::string name = std::format("{}{}", target_prefix, stage++);
            const webgpu::rg::TextureHandle target = render_graph->create_transient_texture(name, desc);

            o->draw(render_graph, position, normal, overlay, shared_config_bg, camera_bg, source, target);
            source = target;
        }
        return source;
    };

    webgpu::rg::TextureHandle pre = run_bucket(m_pre_overlays, "overlay.pre.clear", "overlay.pre.");
    webgpu::rg::TextureHandle post = run_bucket(m_post_overlays, "overlay.post.clear", "overlay.post.");
    return { pre, post };
}

} // namespace webgpu_engine
