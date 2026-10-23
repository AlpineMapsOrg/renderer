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

#include <memory>
#include <webgpu/compute/nodes/Node.h>
#include <webgpu/engine/avalanche/AvalancheRenderer.h>

namespace webgpu_engine {
class Context;
class ScreenSpaceSnowOverlay;
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
    /// Shows or hides the snow cover around the avalanche (Settings::snow_cover, while the 3D
    /// view is on): a screen-space snow overlay this node adds on first use, before the shading
    /// so it is lit like the terrain. The avalanche marks its pixels, so the overlay skips them.
    /// The snow line follows the simulated terrain [min_altitude, max_altitude] (metres; pass
    /// equal values while unknown): full cover in the upper part, thinning to sparse patches
    /// towards the valley. Call every frame; cheap when nothing changes.
    void sync_snow_cover(float min_altitude, float max_altitude);

    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;

private:
    webgpu_engine::Context* m_context;
    std::weak_ptr<webgpu_engine::ScreenSpaceSnowOverlay> m_snow_overlay; // owned by the OverlayRenderer
    glm::vec2 m_snow_terrain = glm::vec2(0.0f); // the terrain range the snow line was fitted to
};

} // namespace webgpu_compute::nodes
