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

#pragma once

#include <webgpu/compute/nodes/Node.h>
#include <webgpu/engine/avalanche/AvalancheRenderer.h>

namespace webgpu_engine {
class Context;
}

namespace webgpu_compute::nodes {

/// Hands the MPM solver's particles to the engine's AvalancheRenderer, which draws them in 3D
/// on the map (sphere impostors in the G-buffer). Like OverlayRenderNode, an app-side terminal
/// node: it knows both the compute graph and the engine. The renderer reads the solver's
/// "particle view" through this node every frame, so buffer reallocations and a solver that is
/// disconnected or removed are picked up at once.
class ParticleRenderNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(ParticleRenderNode)

    explicit ParticleRenderNode(webgpu_engine::Context& context);
    ~ParticleRenderNode() override;

    /// The renderer's settings (colour, size); shared by every node, since there is one renderer.
    webgpu_engine::AvalancheRenderer::Settings& renderer_settings();

    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;

private:
    webgpu_engine::Context* m_context;
};

} // namespace webgpu_compute::nodes
