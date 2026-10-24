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

#include "SandboxView.h"
#include "ui/ImGuiPanel.h"

#include <webgpu/compute/SimulationClock.h>

#include <QJsonArray>
#include <QJsonObject>
#include <chrono>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace webgpu_compute::nodes {
class MpmSolverNode;
class AvaFrameSandboxNode;
struct SandboxTerrain;
class GeoRegionNode;
class SelectTilesNode;
class Node;
}

namespace webgpu_app {

class NodeGraphPanel;
class AvaFrameDownload;

/// Sidebar controls for the MLS-MPM avalanche solver.
///
/// The simulation itself is advanced by webgpu_compute::SimulationDriver, every frame and
/// whether the GUI is shown or not. This panel only controls the solver's clock (play, pause,
/// step, real time or accuracy, speed, GPU budget) and runs the sandbox logic on top of it:
/// the at-rest stop, the exports and batch mode.
class AvalanchePanel : public ImGuiPanel {
    Q_OBJECT

public:
    /// A place the user can pick, expressed geographically so it does not depend on the
    /// tiles the graph happens to load.
    struct Scenario {
        std::string name;
        std::string note; // one-line description shown under the picker

        glm::dvec2 region_center; // latitude, longitude
        float region_extent; // terrain to load around the centre [m]
        uint32_t zoomlevel; // tile zoom; lower means larger area, coarser DEM

        glm::dvec2 domain_center; // latitude, longitude
        float domain_size; // [m]
        uint32_t grid_resolution;

        glm::dvec2 release_center; // latitude, longitude
        float release_radius; // [m]
        float slab_thickness; // [m]
    };

    /// A named material parameter set. Presets are a UI concept layered on top of the
    /// solver's ordinary settings: applying one *writes* those settings, nothing more. That
    /// keeps a single source of truth and lets a preset be edited afterwards.
    ///
    /// Values for the four flow regimes are Li et al. 2021, Table 1 (Cases I-IV) and their
    /// verification case (V); Stomakhin 2013 Table 1 for the film-snow default.
    struct MaterialPreset {
        std::string name;
        std::string note; // one line, shown under the picker

        uint32_t model; // webgpu_compute::nodes::MpmSolverNode::ConstitutiveModel

        // shared by every model
        float youngs_modulus;
        float poissons_ratio;
        float snow_density;
        float terrain_friction; // basal mu

        // Stomakhin 2013
        float hardening;
        float critical_compression;
        float critical_stretch;

        // Drucker-Prager
        float dp_friction_angle;

        // Cohesive Cam Clay
        float ccc_m;
        float ccc_beta;
        float ccc_xi;
        float ccc_p0_initial;
    };

    /// How much of each frame the simulation may take. The GPU has one queue, so every
    /// millisecond of simulation per frame is a millisecond the rendered frame waits.
    /// The automatic modes pick the substeps per frame from the solver's measured GPU time
    /// per substep; Manual uses the "Substeps per run / frame" settings as they are. In real
    /// time the budget is a cap: the simulation falls behind rather than exceed it.
    enum class Pacing : int { Manual = 0, Smooth, Balanced, Fast };

    /// Playback of one context (map or sandbox), remembered per user. The map defaults to real
    /// time at 60 fps, the sandbox to accuracy (as fast as the budget allows).
    struct Playback {
        webgpu_compute::SimulationClock::Mode mode;
        Pacing pacing;
    };

    /// Resolution presets: particle count and grid band height, the two knobs that decide
    /// the GPU cost (see 09-performance-analysis.md). Domain and cell size stay per scenario.
    struct DetailPreset {
        const char* name;
        uint32_t particles;
        uint32_t grid_layers;
    };

    explicit AvalanchePanel(NodeGraphPanel* graph_panel);

    // Loads the MLS-MPM graph and applies the remembered scenario when autostart is on.
    // Runs after NodeGraphPanel::ready() has loaded the default graph, so it replaces it.
    void ready() override;
    // Advances the simulation. Runs every frame, independent of any panel visibility.
    void draw() override;
    void draw_panel() override;

signals:
    /// "Show on the map" for a geolocated sandbox dataset: a camera looking at the avalanche
    /// steeply from the south, framing it. World coordinates; connected to the camera controller.
    void camera_requested(glm::dvec3 position, glm::dvec3 look_at);

private:
    /// Looks up the solver in the currently loaded graph. Returns nullptr if the active
    /// graph has no MPM node. Re-resolved every frame because loading a preset replaces
    /// the graph and every node in it.
    webgpu_compute::nodes::MpmSolverNode* find_solver() const;

    /// Finds the first node of the given type in the active graph, or nullptr.
    template <typename T> T* find_node() const;

    /// Writes the scenario into the region, tile and solver nodes and re-runs the graph.
    /// Returns false when the active graph cannot express it (e.g. no GeoRegionNode).
    bool apply_scenario(const Scenario& scenario);

    /// Writes a material preset into the solver settings, forces a reseed (the plastic state
    /// is model-specific) and pulls dt under the CFL bound of the new stiffness.
    void apply_material_preset(const MaterialPreset& preset);

    /// Startup preferences live outside the graph (QSettings, per user): whether to load
    /// the MLS-MPM graph at launch, which scenario and material to apply, and whether to
    /// start playing. The graph presets themselves stay untouched.
    void load_startup_settings();
    void save_startup_settings() const;
    void draw_startup_settings();

    /// The playback of the loaded context: the sandbox's when the graph has a sandbox node.
    Playback& playback();
    /// Writes mode, speed and GPU budget into the solver's clock. Every frame: cheap, and it
    /// keeps a freshly loaded solver (whose clock starts with defaults) in line with the panel.
    void sync_clock(webgpu_compute::nodes::MpmSolverNode& solver);
    bool playing() const;
    void set_playing(bool playing);
    /// One run now, through the clock (accuracy-sized); after a settings edit or Reset.
    void step(webgpu_compute::nodes::MpmSolverNode& solver);
    void draw_playback(webgpu_compute::nodes::MpmSolverNode& solver);
    void draw_performance(webgpu_compute::nodes::MpmSolverNode& solver);

    /// AvaFrame sandbox (see AvaFrameSandboxNode): datasets prepared by the download in the
    /// panel (AvaFrameData) or by mpm-mls-doc/avaframe/prepare_dataset.py, listed from
    /// <root>/index.json.
    struct SandboxDataset {
        std::string name;
        std::vector<std::string> scenarios;
        bool geolocated = false;
        bool has_reference = false;
    };
    void scan_sandbox_datasets();
    /// Loads the sandbox graph preset and the selected dataset.
    void load_sandbox();
    /// Points the sandbox node at the selected dataset and scenario and runs the graph.
    bool apply_sandbox_dataset();
    /// Once the sandbox node has loaded a dataset: domain size and grid from its suggestion,
    /// reseed. The suggestion needs the loaded data, so this runs after the graph, not before.
    void configure_sandbox_solver(const webgpu_compute::nodes::AvaFrameSandboxNode& sandbox, webgpu_compute::nodes::MpmSolverNode& solver);
    void draw_sandbox_section(webgpu_compute::nodes::AvaFrameSandboxNode& sandbox);
    /// "Download AvaFrame data": the button, a confirmation popup (external data, takes a while)
    /// and the progress of a running download.
    void draw_avaframe_download();
    /// A finished download: list the datasets again and load the selected one.
    void avaframe_download_finished(bool ok);
    /// Reads the solver's peak rasters back and writes them next to the dataset
    /// (write_sandbox_result). Asynchronous; the outcome lands in m_sandbox_status.
    void export_sandbox_result(const std::string& stopped_by);

    /// Batch mode (WEBIGEO_SANDBOX_BATCH=<jobs.json>, see mpm-mls-doc/avaframe/README.md): runs a
    /// list of sandbox runs one after the other, each to rest or to its time limit, exports each
    /// with its label, writes <jobs>.results.json and optionally quits. Never touches the stored
    /// user preferences.
    struct BatchRun {
        std::string label;
        std::string dataset;
        std::string scenario; // empty = the dataset's first
        std::string material_preset; // preset name; empty = keep the current material
        QJsonObject settings; // solver settings to override, keys as in MpmSolverNode::serialize_settings
        float max_time = 400.0f; // [s] simulated; com1DFA's default tEnd
        bool trajectories = false; // "model": "trajectories" - the weBIGeo trajectory model instead of the MPM
    };
    enum class BatchPhase { Idle, Loading, Running, Exporting, ShotSandbox, ShotMap, Done };
    bool load_batch(const QString& path);
    void start_batch_run();
    void batch_tick();
    void finish_batch();
    /// Frames a geolocated sandbox dataset on the map (emits camera_requested).
    void frame_on_map(const webgpu_compute::nodes::SandboxTerrain& terrain);
    /// Puts the camera close to the flow on the map (its centre of mass, steeply from the
    /// south, distance in metres), for the 3D particles. Does nothing before the first readback.
    void look_at_flow(const webgpu_compute::nodes::MpmSolverNode& solver, double distance_metres);
    /// Frames a map scenario's domain once the solver has scanned its terrain (the altitude to
    /// look at comes from that scan), so picking a location also takes the camera there.
    void frame_scenario_when_ready(webgpu_compute::nodes::MpmSolverNode& solver);
    /// Batch screenshots: logs "AvaFrame batch: screenshot <path>" for an external script to
    /// capture the window, then waits until the file exists (or a timeout).
    void request_screenshot(const std::string& path);
    bool screenshot_done();

private:
    NodeGraphPanel* m_graph_panel;

    bool m_autostart = false; // load the MLS-MPM graph and apply the scenario at launch
    bool m_autoplay = false; // ... and start playing once the terrain is in
    bool m_autoplay_pending = false;

    std::vector<Scenario> m_scenarios;
    int m_selected_scenario = 0;
    std::string m_scenario_error;
    const Scenario* m_frame_pending = nullptr; // scenario to frame on the map once its terrain is in

    std::vector<MaterialPreset> m_material_presets;
    int m_selected_material_preset = 0; // 0 = custom (no preset active)

    Playback m_map_playback { webgpu_compute::SimulationClock::Mode::RealTime, Pacing::Smooth };
    Playback m_sandbox_playback { webgpu_compute::SimulationClock::Mode::Accuracy, Pacing::Balanced };
    float m_playback_speed = 1.0f; // real time: simulated seconds per wall-clock second
    bool m_playback_interpolate = true; // real time: draw the particles between the last two simulated states

    bool m_startup_sandbox = false; // autostart loads the sandbox instead of the map
    std::string m_sandbox_root;
    std::vector<SandboxDataset> m_sandbox_datasets;
    std::string m_sandbox_scan_error;
    int m_sandbox_dataset = 0;
    int m_sandbox_scenario = 0;
    std::string m_sandbox_configured; // "dataset/scenario" the solver was last sized for
    bool m_sandbox_auto_export = false; // export the peak rasters when the flow comes to rest
    bool m_sandbox_stopped = false; // this run has come to rest (paused once, exported once), until the next reset
    float m_sandbox_last_time = 0.0f; // simulated time last frame; going back means a reset
    std::string m_sandbox_status;
    AvaFrameDownload* m_avaframe_download = nullptr; // child of this panel, created on first use
    std::string m_last_export; // directory of the last finished export, or "error: ..."

    QString m_batch_file;
    std::vector<BatchRun> m_batch;
    size_t m_batch_index = 0;
    BatchPhase m_batch_phase = BatchPhase::Idle;
    bool m_batch_quit = true;
    QJsonArray m_batch_results;
    std::chrono::steady_clock::time_point m_batch_run_started;
    bool m_batch_screenshots = false; // "screenshots": true - sandbox view and map per MPM run
    std::string m_screenshot_path; // pending screenshot
    std::chrono::steady_clock::time_point m_phase_started;
    SandboxView m_sandbox_view;
};

} // namespace webgpu_app
