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

#include "Node.h"

#include <memory>
#include <nucleus/Raster.h>
#include <radix/geometry.h>
#include <string>
#include <webgpu/base/Context.h>
#include <webgpu/base/raii/TextureWithSampler.h>

namespace webgpu_compute::nodes {

struct SandboxTerrain;

/// A release given per cell, for the release grid mode of MpmSolverNode (the way the AvaFrame
/// sandbox releases): the vertical release depth [m] on a texture over the whole region, laid
/// out like the height texture (uv = region metres / region size, v pointing south), and what
/// the solver needs to sample it.
struct ReleaseArea {
    const webgpu::raii::TextureWithSampler* depth_texture = nullptr; // R32Float
    glm::dvec2 min = glm::dvec2(0.0); // bounds of the release cells [region metres]
    glm::dvec2 max = glm::dvec2(0.0);
    double volume = 0.0; // [m^3]
    double area = 0.0; // [m^2], horizontal
    float max_depth = 0.0f; // [m]
    std::string label; // e.g. "avaKot / relKot"
};

/// A sandbox terrain's release depth resampled onto a map region.
struct ResampledRelease {
    nucleus::Raster<float> depth; // [m] vertical; row 0 is the region's northern edge
    glm::dvec2 texel_size = glm::dvec2(1.0); // [region metres]
    glm::dvec2 min = glm::dvec2(0.0); // bounds of the texels with depth [region metres]
    glm::dvec2 max = glm::dvec2(0.0);
    double volume = 0.0; // [m^3]
    double area = 0.0; // [m^2]
    float max_depth = 0.0f; // [m]
};

/// Puts the release of an AvaFrame dataset onto a map region: every texel centre of a grid over
/// the region (region-relative metres, y north) goes to world coordinates and through the
/// inverse of the dataset's affine fit (terrain.sandbox_to_world) to the dataset's grid, where
/// it takes the nearest cell's vertical release depth. An approximation: the fit is good to
/// under a DEM cell, but the release keeps AvaFrame's depth (from AvaFrame's slope) and its
/// horizontal outline, on whatever terrain the map has there. Texels are about
/// texel_size_metres, at most 4096 per side. Empty (zero volume) without a placement.
ResampledRelease resample_release(const SandboxTerrain& terrain, const radix::geometry::Aabb<2, double>& region_world, double metres_per_world_unit,
    double texel_size_metres);

/// The release areas of a real AvaFrame dataset on the map (the five avalanches of the AvaFrame
/// comparison), for the map's real-time scenarios: loads the dataset (load_sandbox_terrain()),
/// resamples its release onto the region (resample_release()) and hands it to the solver as a
/// ReleaseArea. Without a dataset, or when it is not downloaded, the output is null and the
/// solver falls back to its release disc; the node never fails the graph for that. status()
/// says which.
class AvaFrameReleaseNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(AvaFrameReleaseNode)

    struct AvaFrameReleaseSettings {
        std::string dataset_path; // a folder in extern/avaframe_sandbox, e.g. .../avaKot; empty = none
        std::string release_scenario; // empty = the dataset's first
    };

    AvaFrameReleaseNode(webgpu::Context& ctx);
    ~AvaFrameReleaseNode() override;

    void set_settings(const AvaFrameReleaseSettings& settings) { m_settings = settings; }
    const AvaFrameReleaseSettings& get_settings() const { return m_settings; }
    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

    /// The release handed to the solver after the last run, or nullptr (no dataset, not loaded,
    /// or no release cell inside the region).
    const ReleaseArea* release_area() const { return m_area_valid ? &m_area : nullptr; }
    /// One line for the panel: what the last run did, or why there is no release.
    const std::string& status() const { return m_status; }

public slots:
    void run_impl() override;

private:
    webgpu::Context* m_ctx;
    AvaFrameReleaseSettings m_settings;
    std::unique_ptr<SandboxTerrain> m_terrain; // loaded dataset, kept while path and scenario stay
    std::string m_terrain_key;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_texture;
    ReleaseArea m_area;
    bool m_area_valid = false;
    std::string m_status;
};

} // namespace webgpu_compute::nodes
