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

#include "MpmSolverNode.h"
#include "Node.h"

#include <nucleus/Raster.h>
#include <webgpu/base/Context.h>
#include <webgpu/base/raii/TextureWithSampler.h>

#include <functional>
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

    /* com1DFA reference of this scenario, resampled onto the DEM grid (nearest cell): peak flow
     * thickness [m], velocity [m/s], pressure [kPa], and the footprint pft > 0. All empty when
     * the dataset has no benchmark for the scenario. */
    std::string reference_name;
    nucleus::Raster<uint8_t> reference_footprint;
    std::vector<float> reference_pft;
    std::vector<float> reference_pfv;
    std::vector<float> reference_ppr;

    /* Square box covering release and reference footprint (or the whole DEM without a
     * reference), with a margin. A suggestion; the solver's domain size setting decides. */
    glm::dvec2 suggested_domain_centre = glm::dvec2(0.0);
    double suggested_domain_size = 0.0;

    std::optional<glm::dvec2> centre_lat_lon; // only for geolocated (real) datasets

    /* Geolocated datasets: sandbox metres -> weBIGeo world coordinates (EPSG:3857), an affine
     * map least-squares fitted to the DEM's four corners from dataset.json (computed there with
     * pyproj from EPSG:31287). Over a few km both projections are conformal and nearly linear;
     * the fit residual is logged and stays well under a DEM cell. */
    std::optional<glm::dmat3> sandbox_to_world;
    double sandbox_to_world_residual = 0.0; // [world units], largest corner residual
};

/// Loads a prepared AvaFrame dataset (dataset.json, DEM, the release scenario, its com1DFA
/// reference, the placement on the map) from <dataset_path>: the CPU side of
/// AvaFrameSandboxNode, also used to bring the release areas onto the map. An empty
/// release_scenario means the first one. Returns nullptr and an error message on failure.
std::unique_ptr<SandboxTerrain> load_sandbox_terrain(const std::string& dataset_path, const std::string& release_scenario, std::string& error);

/// Peak rasters on the DEM grid (row 0 north), as written by write_sandbox_result.
struct SandboxPeaks {
    std::vector<float> pft; // [m], slope-normal
    std::vector<float> pfv; // [m/s]
    std::vector<float> ppr; // [kPa]
    float simulated_time = 0.0f;
};
/// Resamples the solver's column peaks onto the DEM grid: nearest column at each cell centre,
/// depth * cos(slope) for pft, rho pfv^2 for ppr, 0 outside the domain.
SandboxPeaks resample_sandbox_peaks(const SandboxTerrain& terrain, const MpmSolverNode::ColumnPeaks& peaks, float density);

enum class SandboxQuantity { Thickness, Velocity, Pressure };
/// Colour scale shared by the 2D view, its legend and the map overlay: viridis over
/// [0, 5 m], [0, 100 m/s], and log10 over [1, 1000 kPa]. 0 is transparent.
struct SandboxScale {
    const char* label;
    const char* unit;
    float min;
    float max;
    bool logarithmic;
};
SandboxScale sandbox_scale(SandboxQuantity quantity);
glm::u8vec4 sandbox_colour(SandboxQuantity quantity, float value);
/// A DEM-grid raster coloured with sandbox_colour. Cells whose flow thickness (thickness, same
/// source) is below SANDBOX_FOOTPRINT_THICKNESS are drawn faint: for the MPM that is the
/// sub-centimetre layer of single particles, which would otherwise dominate the picture.
nucleus::Raster<glm::u8vec4> colour_sandbox_raster(
    const SandboxTerrain& terrain, const std::vector<float>& values, const std::vector<float>& thickness, SandboxQuantity quantity);
/// Flow thickness that counts as the footprint in the displays and the export summary [m].
constexpr float SANDBOX_FOOTPRINT_THICKNESS = 0.1f;

/// Result of a sandbox run, written next to the dataset in AvaFrame's raster format so the
/// comparison tools treat it like a com1DFA result: <dataset>/mpm/<run>/{pft,pfv,ppr}.asc on the
/// DEM grid and in the dataset's CRS, plus params.json.
///   pft  peak flow thickness [m], slope-normal like com1DFA's: vertical depth * cos(slope)
///   pfv  peak of the mean particle speed in the grid column [m/s] (depth-averaged)
///   ppr  peak pressure rho * pfv^2 [kPa], com1DFA's definition
/// Values at DEM cell centres from the nearest grid column; 0 outside the domain.
struct SandboxRunSummary {
    std::string directory; // empty on failure
    std::string error;
    float max_thickness = 0.0f;
    float max_speed = 0.0f;
    double footprint_area = 0.0; // pft > 0 [m^2]
    double footprint_area_10cm = 0.0; // pft > 0.1 m [m^2]
};
SandboxRunSummary write_sandbox_result(const SandboxTerrain& terrain, const SandboxPeaks& peaks, const MpmSolverNode::ColumnPeaks& columns, float density,
    const QJsonObject& run_info);
/// Writes pft/pfv/ppr and params.json (plus dataset, release, reference) into a new
/// <dataset>/mpm/<time>_<release>[_<label>]/; fills summary.directory, or summary.error.
void write_run_files(const SandboxTerrain& terrain, const std::vector<float>& pft, const std::vector<float>& pfv, const std::vector<float>& ppr,
    QJsonObject params, SandboxRunSummary& summary);

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
    ~AvaFrameSandboxNode() override;

    /// Expires when the node is destroyed - for asynchronous callbacks that hold a pointer to it.
    std::weak_ptr<bool> alive_token() const { return m_alive; }

    void set_settings(const AvaFrameSandboxSettings& settings) { m_settings = settings; }
    const AvaFrameSandboxSettings& get_settings() const { return m_settings; }
    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

    /// The loaded dataset, or nullptr before the first successful run.
    const SandboxTerrain* terrain() const { return m_terrain.get(); }
    /// Hillshade of the DEM with the release area (cyan) and the com1DFA footprint outline
    /// (green), for the 2D sandbox view. Same grid as the DEM.
    const webgpu::raii::TextureWithSampler* preview_texture() const { return m_preview_texture.get(); }

    /// A DEM-sized RGBA layer for the 2D view (peak rasters), written by the caller.
    void set_layer_image(const nucleus::Raster<glm::u8vec4>& image);
    const webgpu::raii::TextureWithSampler* layer_texture() const { return m_layer_texture.get(); }

    /// Redraws the map overlay of a geolocated dataset: release (cyan), the MPM peak flow
    /// thickness where given (pft > 0.1 m, colour scale as in the 2D view) and the com1DFA
    /// footprint outline (green), resampled into a texture aligned with the world axes.
    /// Synthetic datasets keep a 1x1 transparent texture, so the overlay always has one.
    void update_map_overlay(const std::vector<float>* mpm_pft);

    /// Exports a run of the weBIGeo trajectory model (ComputeAvalancheTrajectoriesNode at
    /// resolution multiplier 1, so its layers are on the DEM grid) like a com4FlowPy run:
    /// footprint flag, sqrt(2 g zdelta), rho v^2. Asynchronous; the layer buffers must outlive it.
    void export_trajectories(webgpu::raii::RawBuffer<uint32_t>& zdelta, webgpu::raii::RawBuffer<uint32_t>& counts, QJsonObject run_info,
        std::function<void(const SandboxRunSummary&)> done);

public slots:
    void run_impl() override;

private:
    webgpu::Context* m_ctx;
    AvaFrameSandboxSettings m_settings;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

    std::unique_ptr<SandboxTerrain> m_terrain;
    radix::geometry::Aabb<2, double> m_region_aabb;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_height_texture;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_release_texture;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_preview_texture;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_layer_texture;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_release_mask_texture;

    /* Map overlay (OverlayRenderNode links the texture; it must never be null). */
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_map_texture;
    radix::geometry::Aabb<2, double> m_map_aabb;
    glm::dvec2 m_map_texel_size = glm::dvec2(1.0); // [world units]
};

} // namespace webgpu_compute::nodes
