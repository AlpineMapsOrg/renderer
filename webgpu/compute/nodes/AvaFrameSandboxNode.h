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

#include <nucleus/Raster.h>
#include <webgpu/base/Context.h>
#include <webgpu/base/raii/TextureWithSampler.h>

#include <memory>
#include <optional>
#include <string>

namespace webgpu_compute::nodes {

/// A test case the solver runs on directly, instead of on the map: AvaFrame's DEM and release
/// area, prepared by mpm-mls-doc/avaframe/prepare_dataset.py. Everything is in metres on the
/// DEM grid, relative to its lower-left corner ("sandbox coordinates"). Row 0 of every raster
/// is the northern edge, as in the ESRI ASCII files and the height texture.
struct SandboxTerrain {
    std::string name; // dataset, e.g. avaKot
    std::string scenario; // release scenario, e.g. relKot
    std::string dataset_path; // folder the dataset was loaded from

    glm::uvec2 grid_size = glm::uvec2(0); // DEM cells (columns, rows)
    double cellsize = 0.0; // [m]
    glm::dvec2 size_metres = glm::dvec2(0.0); // DEM extent
    glm::dvec2 source_origin = glm::dvec2(0.0); // xllcorner, yllcorner in the dataset's own CRS

    nucleus::Raster<float> heights; // [m], NODATA filled from the nearest valid cell
    nucleus::Raster<uint8_t> nodata; // 1 where the DEM had no data
    nucleus::Raster<float> release_depth; // vertical release depth [m]: thickness / cos(slope)

    /* AvaFrame's thickness is perpendicular to the slope. Volume = sum of thickness times
     * surface area = sum of vertical depth times horizontal cell area. */
    double release_volume = 0.0; // [m^3]
    double release_area = 0.0; // horizontal [m^2]
    float release_max_depth = 0.0f; // [m], vertical
    glm::dvec2 release_min = glm::dvec2(0.0); // bounds of the release cells, sandbox metres
    glm::dvec2 release_max = glm::dvec2(0.0);

    /* com1DFA reference footprint of this scenario (peak flow thickness > 0), resampled onto
     * the DEM grid. Empty when the dataset has no benchmark for the scenario. */
    std::string reference_name;
    nucleus::Raster<uint8_t> reference_footprint;

    /* Square box covering release and reference footprint (or the whole DEM without a
     * reference), with a margin. A suggestion; the solver's domain size setting decides. */
    glm::dvec2 suggested_domain_centre = glm::dvec2(0.0);
    double suggested_domain_size = 0.0;

    std::optional<glm::dvec2> centre_lat_lon; // only for geolocated (real) datasets
};

/// Loads one prepared AvaFrame dataset and hands it to the MPM solver as a sandbox: the DEM
/// as height texture, the release as a depth texture and the region in metres. See
/// SandboxTerrain and mpm-mls-doc/avaframe/README.md.
class AvaFrameSandboxNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(AvaFrameSandboxNode)

    struct AvaFrameSandboxSettings {
        std::string dataset_path; // a folder in extern/avaframe_sandbox, e.g. .../avaKot
        std::string release_scenario; // empty = the dataset's first scenario
    };

    AvaFrameSandboxNode(webgpu::Context& ctx);
    AvaFrameSandboxNode(webgpu::Context& ctx, const AvaFrameSandboxSettings& settings);

    void set_settings(const AvaFrameSandboxSettings& settings) { m_settings = settings; }
    const AvaFrameSandboxSettings& get_settings() const { return m_settings; }
    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

    /// The loaded dataset, or nullptr before the first successful run.
    const SandboxTerrain* terrain() const { return m_terrain.get(); }
    /// Hillshade of the DEM with the release area (cyan) and the com1DFA footprint outline
    /// (green), for the 2D sandbox view. Same grid as the DEM.
    const webgpu::raii::TextureWithSampler* preview_texture() const { return m_preview_texture.get(); }

public slots:
    void run_impl() override;

private:
    webgpu::Context* m_ctx;
    AvaFrameSandboxSettings m_settings;

    std::unique_ptr<SandboxTerrain> m_terrain;
    radix::geometry::Aabb<2, double> m_region_aabb;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_height_texture;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_release_texture;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_preview_texture;
};

} // namespace webgpu_compute::nodes
