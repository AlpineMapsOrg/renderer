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

#include "OverlayRenderNode.h"

#include "nucleus/srs.h"
#include <QDebug>
#include <webgpu/webgpu.h>
#include <webgpu_engine/Context.h>
#include <webgpu_engine/overlay/OverlayRenderer.h>
#include <webgpu_engine/overlay/TextureOverlay.h>

namespace webgpu_compute::nodes {

OverlayRenderNode::OverlayRenderNode(webgpu_engine::Context& context)
    : OverlayRenderNode(context, OverlaySettings {})
{
}

OverlayRenderNode::OverlayRenderNode(webgpu_engine::Context& context, const OverlaySettings& settings)
    : Node({ InputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>()),
               InputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>()) },
          {})
    , m_context(&context)
    , m_settings(settings)
{
}

OverlayRenderNode::~OverlayRenderNode()
{
    if (auto overlay = m_result_overlay.lock())
        overlay->link_texture(nullptr);
}

void OverlayRenderNode::run_impl()
{
    if (input_socket("texture").is_socket_connected() && input_socket("region aabb").is_socket_connected()) {
        const auto* texture = std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("texture").get_connected_data());
        const auto* aabb = std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("region aabb").get_connected_data());

        bool copy = m_settings.copy;
        if (copy && texture && !(texture->texture().descriptor().usage & WGPUTextureUsage_CopySrc)) {
            qWarning() << "OverlayRenderNode: source texture lacks CopySrc usage; falling back to linking instead of copying.";
            copy = false;
        }

        auto overlay = m_result_overlay.lock();
        if (!overlay) { // first run, or the user deleted it from the panel -> (re)create
            overlay = std::make_shared<webgpu_engine::TextureOverlay>();
            overlay->name = "Compute Result";
            m_context->overlay_renderer()->add_overlay(overlay);
            m_result_overlay = overlay;
        }

        // TODO: the stitch node ignores the last col/row; trim the aabb to match. This
        // correction should eventually move into the stitch node's "region aabb" output.
        radix::geometry::Aabb<2, double> trimmed = *aabb;
        trimmed.max -= glm::dvec2(nucleus::srs::tile_width(18) / 65, nucleus::srs::tile_height(18) / 65);
        overlay->settings.aabb = trimmed;
        if (texture) {
            if (copy) {
                overlay->load_texture(*texture);
            } else {
                overlay->link_texture(texture);
            }
        }
        overlay->update_gpu_settings();
        m_context->request_redraw();
    }
    complete_run();
}

} // namespace webgpu_compute::nodes
