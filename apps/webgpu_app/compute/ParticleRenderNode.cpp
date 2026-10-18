/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

#include "ParticleRenderNode.h"

#include <algorithm>
#include <webgpu/compute/nodes/MpmSolverNode.h>
#include <webgpu/engine/Context.h>

namespace webgpu_compute::nodes {

ParticleRenderNode::ParticleRenderNode(webgpu_engine::Context& context)
    : Node({ InputSocket(*this, "particle view", data_type<const MpmParticleView*>()) }, {})
    , m_context(&context)
{
}

ParticleRenderNode::~ParticleRenderNode()
{
    if (auto* renderer = m_context->avalanche_renderer())
        renderer->clear_source(this);
}

webgpu_engine::AvalancheRenderer::Settings& ParticleRenderNode::renderer_settings()
{
    static webgpu_engine::AvalancheRenderer::Settings unused; // only without a renderer (tests)
    auto* renderer = m_context->avalanche_renderer();
    return renderer != nullptr ? renderer->settings : unused;
}

void ParticleRenderNode::run_impl()
{
    if (auto* renderer = m_context->avalanche_renderer()) {
        // Resolved every frame through the socket: the solver reallocates its buffer when the
        // particle count changes, and a removed solver disconnects the socket first.
        renderer->set_source(
            [this]() -> std::optional<webgpu_engine::AvalancheRenderer::Frame> {
                auto& socket = input_socket("particle view");
                if (!socket.is_socket_connected())
                    return std::nullopt;
                const auto* view = std::get<data_type<const MpmParticleView*>()>(socket.get_connected_data());
                if (view == nullptr || !view->placed || view->particles == nullptr)
                    return std::nullopt;
                return webgpu_engine::AvalancheRenderer::Frame {
                    view->particles, view->particle_count, view->world_from_metres, view->particle_spacing, view->domain_origin, view->dx, view->columns,
                    view->height_texture, view->region_size };
            },
            this);
        m_context->request_redraw();
    }
    complete_run();
}

void ParticleRenderNode::serialize_settings(QJsonObject& out) const
{
    const auto* renderer = m_context->avalanche_renderer();
    if (renderer == nullptr)
        return;
    out["enabled"] = renderer->settings.enabled;
    out["colour_mode"] = int(renderer->settings.colour_mode);
    out["radius_scale"] = double(renderer->settings.radius_scale);
    out["shape"] = int(renderer->settings.shape);
    out["strain_window"] = double(renderer->settings.strain_window);
    out["surface"] = renderer->settings.surface;
}

void ParticleRenderNode::deserialize_settings(const QJsonObject& in)
{
    auto& s = renderer_settings();
    if (in.contains("enabled"))
        s.enabled = in["enabled"].toBool(s.enabled);
    if (in.contains("colour_mode"))
        s.colour_mode = webgpu_engine::AvalancheRenderer::ColourMode(std::clamp(in["colour_mode"].toInt(), 0, 1));
    if (in.contains("radius_scale"))
        s.radius_scale = float(in["radius_scale"].toDouble(s.radius_scale));
    if (in.contains("shape"))
        s.shape = webgpu_engine::AvalancheRenderer::Shape(std::clamp(in["shape"].toInt(), 0, 1));
    if (in.contains("strain_window"))
        s.strain_window = float(in["strain_window"].toDouble(s.strain_window));
    if (in.contains("surface"))
        s.surface = in["surface"].toBool(s.surface);
}

} // namespace webgpu_compute::nodes
