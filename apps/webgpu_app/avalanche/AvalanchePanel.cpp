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

#include "AvalanchePanel.h"

#include "AvaFrameData.h"
#include "compute/NodeGraphPanel.h"
#include "compute/OverlayRenderNode.h"
#include "compute/ParticleRenderNode.h"
#include <IconsFontAwesome5.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <QSettings>
#endif
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <nucleus/srs.h>
#include <webgpu/compute/NodeGraph.h>
#include <QJsonObject>
#include <SDL2/SDL.h>
#include <cstdlib>
#include <webgpu/compute/nodes/AvaFrameReleaseNode.h>
#include <webgpu/compute/nodes/AvaFrameSandboxNode.h>
#include <webgpu/compute/nodes/ComputeAvalancheTrajectoriesNode.h>
#include <webgpu/compute/nodes/GeoRegionNode.h>
#include <webgpu/compute/nodes/MpmSolverNode.h>
#include <webgpu/compute/nodes/SelectTilesNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;
using webgpu_compute::SimulationClock;

AvalanchePanel::AvalanchePanel(NodeGraphPanel* graph_panel)
    : m_graph_panel(graph_panel)
    , m_scenarios({
          Scenario { "Breite Ries (Schneeberg)", "Gully on Austria's easternmost 2000er, 1931 m to 1364 m.",
              glm::dvec2(47.77663, 15.81600), 8000.0f, 15, glm::dvec2(47.77663, 15.81600), 4000.0f, 320, glm::dvec2(47.77480, 15.81050), 100.0f, 1.5f },
          Scenario { "Grossglockner (Pasterze side)", "Austria's highest summit, 3798 m, above the Pasterze glacier.",
              glm::dvec2(47.07900, 12.70200), 8000.0f, 15, glm::dvec2(47.07900, 12.70200), 4000.0f, 320, glm::dvec2(47.07500, 12.69600), 150.0f, 2.0f },
          Scenario { "Dachstein (Hallstatt Glacier)", "North side of the Dachstein plateau above the glacier.",
              glm::dvec2(47.47800, 13.60600), 8000.0f, 15, glm::dvec2(47.47800, 13.60600), 4000.0f, 320, glm::dvec2(47.47300, 13.60400), 130.0f, 2.0f },
          // AvaFrame's five real test avalanches (the ones the comparison uses), on the map. Domain:
          // about 1.4x the sandbox's (release and com1DFA footprint with a margin), because on the
          // map's terrain the flow spreads further across the valley floors. Release: AvaFrame's
          // release areas resampled onto the map (AvaFrameReleaseNode) when the AvaFrame data is
          // downloaded. Otherwise a disc of the same area (its largest part for Madlein) on its
          // centroid, masked by the map's own 30-45 degree release slopes, with AvaFrame's
          // slope-normal thickness as vertical depth.
          Scenario { "Kontertal (AvaFrame avaKot)", "AvaFrame test avalanche, Lechtal.", glm::dvec2(47.25740, 10.39954), 4600.0f, 15,
              glm::dvec2(47.25740, 10.39954), 2600.0f, 325, glm::dvec2(47.25227, 10.39604), 70.0f, 1.4f, "avaKot", "relKot" },
          Scenario { "Gallreide (AvaFrame avaGar)", "AvaFrame test avalanche, event of February 2009.", glm::dvec2(47.04659, 11.33737), 4900.0f, 15,
              glm::dvec2(47.04659, 11.33737), 2900.0f, 363, glm::dvec2(47.05046, 11.32931), 114.0f, 1.5f, "avaGar", "relGar" },
          Scenario { "Hintertux (AvaFrame avaHit)", "AvaFrame test avalanche, Tuxertal.", glm::dvec2(47.11677, 11.67028), 5200.0f, 15,
              glm::dvec2(47.11677, 11.67028), 3200.0f, 400, glm::dvec2(47.12098, 11.66391), 261.0f, 1.24f, "avaHit", "relHit" },
          Scenario { "Wolfsgruben (AvaFrame avaWog)", "AvaFrame test avalanche, event of 1988.", glm::dvec2(47.12760, 10.28294), 5400.0f, 15,
              glm::dvec2(47.12760, 10.28294), 3400.0f, 425, glm::dvec2(47.12161, 10.28847), 207.0f, 1.21f, "avaWog", "relWog" },
          Scenario { "Madlein (AvaFrame avaMal)", "AvaFrame test avalanche above Ischgl, event of 1984.", glm::dvec2(47.02478, 10.28108), 6500.0f, 15,
              glm::dvec2(47.02478, 10.28108), 4500.0f, 512, glm::dvec2(47.03342, 10.27979), 298.0f, 1.24f, "avaMal", "relMal1to3" },
      })
    , m_material_presets({
          // name, note, model, E, nu, rho, mu, | Stomakhin xi, theta_c, theta_s | DP phi | CCC M, beta, xi, p0
          // Entry 0 is the "custom" placeholder and is never applied.
          MaterialPreset { "(custom)", "Parameters edited by hand.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
          MaterialPreset { "Stomakhin 2013 (film snow)", "Disney's parameters, tuned for metre-scale visuals, not measured snow.",
              nodes::MpmSolverNode::STOMAKHIN_2013, 1.4e5f, 0.2f, 400.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 0.7f, 0.2f, 0.002f, 3000.0f },
          // The Li 2021 entries carry the paper's Table 1 parameters, and their notes describe what
          // Li observed - with an associative return at 0.5 m cells. At this solver's ~12 m cells
          // the slab is under one cell thick, so the regimes are a starting point, not a result.
          MaterialPreset { "Cold dense (Li 2021 I parameters)", "Li observed: fluid-like, fully sheared, low flow height, surges at the front.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 0.5f, 0.0f, 1.0f, 3000.0f },
          MaterialPreset { "Warm shear (Li 2021 II parameters)", "Li observed: granulation, fluctuating surface, piles above initial height.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 1.5f, 0.3f, 1.0f, 30000.0f },
          MaterialPreset { "Sliding slab (Li 2021 III parameters)", "Li observed: brittle, breaks into blocks shortly after release.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 1.5f, 0.5f, 1.0f, 42000.0f },
          MaterialPreset { "Warm plug (Li 2021 IV parameters)", "Li observed: ductile block sheared only at the base. Lowest M, highest beta - not a typo.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 0.5f, 1.0f, 0.1f, 12000.0f },
          MaterialPreset { "Vallee de la Sionne 2003 (Li 2021 V parameters)", "Li back-calculated these from the real avalanche of 7 Feb 2003. New snow.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 200.0f, 0.49f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 0.7f, 0.2f, 0.002f, 3000.0f },
          MaterialPreset { "Cold dense, cohesionless (Drucker-Prager)", "Cohesionless alternative for the cold-dense regime: friction cone. phi from M = 0.5.",
              nodes::MpmSolverNode::DRUCKER_PRAGER, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 13.3f, 0.5f, 0.0f, 1.0f, 3000.0f },
      })
{
}

namespace {
constexpr const char* MPM_GRAPH_PRESET = ":/graphs/mpm_avalanche_simulation.json";
constexpr const char* SANDBOX_GRAPH_PRESET = ":/graphs/mpm_avaframe_sandbox.json";
constexpr const char* TRAJECTORIES_SANDBOX_GRAPH_PRESET = ":/graphs/trajectories_avaframe_sandbox.json";

// Where prepare_dataset.py writes the sandbox datasets, in native builds from a source tree.
// Elsewhere (web, an installed app) the download in the panel writes into the app's data
// folder instead; in the web build that is browser memory, gone after a reload.
#ifdef ALP_SOURCE_DIR
constexpr const char* DEFAULT_SANDBOX_ROOT = ALP_SOURCE_DIR "/extern/avaframe_sandbox";
#else
constexpr const char* DEFAULT_SANDBOX_ROOT = "";
#endif

// Particles per cell are far above the 8 of Li et al. 2021 at every level (a 100 m release
// disc at 12.5 m cells is ~200 columns), so fewer particles mostly thin the rendered cloud
// rather than coarsen the mechanics. 12 layers still leave ~90 m of headroom above the
// terrain at 12.5 m cells. See 09-performance-analysis.md, "Defaults".
constexpr AvalanchePanel::DetailPreset DETAIL_PRESETS[] = {
    { "Low (fast)", 32768u, 10u },
    { "Medium", 65536u, 12u },
    { "High", 131072u, 16u },
};

// GPU milliseconds per frame the simulation may use, per pacing mode.
float pacing_budget_ms(AvalanchePanel::Pacing pacing)
{
    switch (pacing) {
    case AvalanchePanel::Pacing::Smooth:
        return 6.0f; // leaves a 60 Hz frame room for the terrain
    case AvalanchePanel::Pacing::Balanced:
        return 14.0f; // ~30 fps on a GPU that renders the terrain in a few ms
    case AvalanchePanel::Pacing::Fast:
        return 30.0f; // simulation first, ~20 fps
    default:
        return 0.0f;
    }
}
#ifndef __EMSCRIPTEN__
constexpr const char* SETTINGS_ORGANISATION = "weBIGeo";
constexpr const char* SETTINGS_APPLICATION = "avalanche";
#endif
} // namespace

void AvalanchePanel::load_startup_settings()
{
#ifdef __EMSCRIPTEN__
    // Web build: there is no per-user settings store worth relying on (Qt's WASM QSettings
    // backend loads asynchronously), and the deployment exists to show the simulation - so
    // it loads by default. The URL decides the rest:  ?avalanche=0 skips it, ?avalanche=play
    // also starts playing. Temporary for the release; a proper setting would go into the
    // site's own configuration.
    const int mode = EM_ASM_INT({
        const value = new URLSearchParams(window.location.search).get('avalanche');
        if (value === '0' || value === 'off' || value === 'false') return 0;
        if (value === 'play') return 2;
        return 1;
    });
    m_autostart = mode != 0;
    m_autoplay = mode == 2;
#else
    QSettings settings(SETTINGS_ORGANISATION, SETTINGS_APPLICATION);
    m_autostart = settings.value("startup/autostart", false).toBool();
    m_autoplay = settings.value("startup/autoplay", false).toBool();
    const auto read_mode = [&](const char* key, SimulationClock::Mode fallback) {
        return SimulationClock::Mode(std::clamp(settings.value(key, int(fallback)).toInt(), 0, int(SimulationClock::Mode::Accuracy)));
    };
    const auto read_pacing = [&](const char* key, Pacing fallback) { return Pacing(std::clamp(settings.value(key, int(fallback)).toInt(), 0, int(Pacing::Fast))); };
    m_map_playback.mode = read_mode("playback/map_mode", m_map_playback.mode);
    m_map_playback.pacing = read_pacing("playback/map_pacing", m_map_playback.pacing);
    m_sandbox_playback.mode = read_mode("playback/sandbox_mode", m_sandbox_playback.mode);
    // Before the two contexts, one pacing served both; it was mostly set for the sandbox.
    m_sandbox_playback.pacing = read_pacing("playback/sandbox_pacing", read_pacing("playback/pacing", m_sandbox_playback.pacing));
    m_playback_speed = std::clamp(settings.value("playback/speed", 1.0).toFloat(), SimulationClock::MIN_SPEED, SimulationClock::MAX_SPEED);
    m_playback_interpolate = settings.value("playback/interpolate", true).toBool();
    const auto read_entrainment = [&](const char* key, nodes::MpmSolverNode::EntrainmentMode fallback) {
        return nodes::MpmSolverNode::EntrainmentMode(std::clamp(settings.value(key, int(fallback)).toInt(), 0, int(nodes::MpmSolverNode::EROSION)));
    };
    m_map_entrainment = read_entrainment("entrainment/map_mode", m_map_entrainment);
    m_sandbox_entrainment = read_entrainment("entrainment/sandbox_mode", m_sandbox_entrainment);
    m_selected_scenario = std::clamp(settings.value("startup/scenario", 0).toInt(), 0, int(m_scenarios.size()) - 1);
    m_selected_material_preset = std::clamp(settings.value("startup/material_preset", 0).toInt(), 0, int(m_material_presets.size()) - 1);
    m_startup_sandbox = settings.value("startup/sandbox", false).toBool();
    m_sandbox_root = settings.value("sandbox/root", QString(DEFAULT_SANDBOX_ROOT)).toString().toStdString();
    m_sandbox_auto_export = settings.value("sandbox/auto_export", false).toBool();
    m_sandbox_view.set_layer(SandboxView::Layer(std::clamp(settings.value("sandbox/view_layer", 0).toInt(), 0, int(SandboxView::Layer::RefPressure))));
    m_sandbox_view.open = settings.value("sandbox/view_open", true).toBool();
#endif
    if (m_sandbox_root.empty())
        m_sandbox_root = DEFAULT_SANDBOX_ROOT;
    if (m_sandbox_root.empty())
        m_sandbox_root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("avaframe_sandbox").toStdString();
    scan_sandbox_datasets();
#ifndef __EMSCRIPTEN__
    // Restore the dataset by name: the list may have changed since.
    const std::string dataset = settings.value("sandbox/dataset").toString().toStdString();
    const std::string scenario = settings.value("sandbox/scenario").toString().toStdString();
    for (size_t i = 0; i < m_sandbox_datasets.size(); i++) {
        if (m_sandbox_datasets[i].name != dataset)
            continue;
        m_sandbox_dataset = int(i);
        const auto& scenarios = m_sandbox_datasets[i].scenarios;
        m_sandbox_scenario = int(std::find(scenarios.begin(), scenarios.end(), scenario) - scenarios.begin());
        if (m_sandbox_scenario >= int(scenarios.size()))
            m_sandbox_scenario = 0;
    }
#endif
}

void AvalanchePanel::save_startup_settings() const
{
    if (m_batch_phase != BatchPhase::Idle)
        return; // a batch must not change what the user has stored
#ifndef __EMSCRIPTEN__
    QSettings settings(SETTINGS_ORGANISATION, SETTINGS_APPLICATION);
    settings.setValue("startup/autostart", m_autostart);
    settings.setValue("startup/autoplay", m_autoplay);
    settings.setValue("startup/scenario", m_selected_scenario);
    settings.setValue("startup/material_preset", m_selected_material_preset);
    settings.setValue("playback/map_mode", int(m_map_playback.mode));
    settings.setValue("playback/map_pacing", int(m_map_playback.pacing));
    settings.setValue("playback/sandbox_mode", int(m_sandbox_playback.mode));
    settings.setValue("playback/sandbox_pacing", int(m_sandbox_playback.pacing));
    settings.setValue("playback/speed", double(m_playback_speed));
    settings.setValue("playback/interpolate", m_playback_interpolate);
    settings.setValue("entrainment/map_mode", int(m_map_entrainment));
    settings.setValue("entrainment/sandbox_mode", int(m_sandbox_entrainment));
    settings.setValue("startup/sandbox", m_startup_sandbox);
    settings.setValue("sandbox/root", QString::fromStdString(m_sandbox_root));
    settings.setValue("sandbox/auto_export", m_sandbox_auto_export);
    settings.setValue("sandbox/view_layer", int(m_sandbox_view.layer()));
    settings.setValue("sandbox/view_open", m_sandbox_view.open);
    if (m_sandbox_dataset < int(m_sandbox_datasets.size())) {
        const auto& dataset = m_sandbox_datasets[size_t(m_sandbox_dataset)];
        settings.setValue("sandbox/dataset", QString::fromStdString(dataset.name));
        if (m_sandbox_scenario < int(dataset.scenarios.size()))
            settings.setValue("sandbox/scenario", QString::fromStdString(dataset.scenarios[size_t(m_sandbox_scenario)]));
    }
#endif
}

void AvalanchePanel::ready()
{
    load_startup_settings();
#ifndef __EMSCRIPTEN__
    if (const char* batch = std::getenv("WEBIGEO_SANDBOX_BATCH"); batch != nullptr && *batch != '\0') {
        if (load_batch(QString::fromUtf8(batch)))
            start_batch_run();
        return;
    }
#endif
    if (!m_autostart)
        return;
    if (m_startup_sandbox) {
        load_sandbox();
        if (m_selected_material_preset > 0)
            apply_material_preset(m_material_presets[size_t(m_selected_material_preset)]);
        m_autoplay_pending = m_autoplay;
        return;
    }
    m_graph_panel->load_preset(MPM_GRAPH_PRESET);
    // Material first: it only writes solver settings. The scenario then writes the
    // remaining ones and runs the graph, which fetches the terrain and seeds.
    if (m_selected_material_preset > 0)
        apply_material_preset(m_material_presets[size_t(m_selected_material_preset)]);
    m_scenario_error = apply_scenario(m_scenarios[size_t(m_selected_scenario)]) ? std::string {} : std::string { "Autostart: could not apply the scenario." };
    m_autoplay_pending = m_autoplay;
}

void AvalanchePanel::draw_startup_settings()
{
    ImGui::TextDisabled("Startup");
#ifdef __EMSCRIPTEN__
    ImGui::TextWrapped("Web build: loads at launch. Add ?avalanche=0 to the URL to skip it, ?avalanche=play to start playing too.");
    return;
#endif
    bool changed = ImGui::Checkbox("Load this simulation at launch", &m_autostart);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Loads the MLS-MPM graph, applies the selected location and material preset\n"
                          "and fetches the terrain when the app starts. Stored per user, not in the graph.");
    if (m_autostart) {
        ImGui::SameLine();
        changed |= ImGui::Checkbox("and play", &m_autoplay);
    }
    if (changed)
        save_startup_settings();
}

template <typename T> T* AvalanchePanel::find_node() const
{
    auto* graph = m_graph_panel->node_graph();
    if (graph == nullptr)
        return nullptr;
    for (auto& [name, node] : graph->get_nodes()) {
        if (auto* typed = dynamic_cast<T*>(node.get()))
            return typed;
    }
    return nullptr;
}

nodes::MpmSolverNode* AvalanchePanel::find_solver() const { return find_node<nodes::MpmSolverNode>(); }

bool AvalanchePanel::apply_scenario(const Scenario& scenario)
{
    auto* graph = m_graph_panel->node_graph();
    auto* region = find_node<nodes::GeoRegionNode>();
    auto* solver = find_solver();
    if (graph == nullptr || region == nullptr || solver == nullptr)
        return false;

    auto region_settings = region->get_settings();
    region_settings.center_latitude = scenario.region_center.x;
    region_settings.center_longitude = scenario.region_center.y;
    region_settings.extent = scenario.region_extent;
    region->set_settings(region_settings);

    if (auto* tiles = find_node<nodes::SelectTilesNode>()) {
        auto tile_settings = tiles->get_settings();
        tile_settings.zoomlevel = scenario.zoomlevel;
        tiles->set_settings(tile_settings);
    }

    auto solver_settings = solver->get_settings();
    solver_settings.domain_center = scenario.domain_center;
    solver_settings.domain_size_xy = scenario.domain_size;
    solver_settings.grid_resolution_xy = scenario.grid_resolution;
    solver_settings.release_center = scenario.release_center;
    solver_settings.release_radius = scenario.release_radius;
    solver_settings.slab_thickness = scenario.slab_thickness;
    solver_settings.reset_on_next_run = true;
    solver->set_settings(solver_settings);

    solver->clock().pause(); // the terrain has to be fetched and stitched before stepping again
    m_frame_pending = &scenario; // m_scenarios never changes size, so the pointer stays valid

    // AvaFrame scenarios start with the release scenario the comparison used.
    m_map_release_scenario = 0;
    if (const SandboxDataset* dataset = map_release_dataset(scenario)) {
        const auto it = std::find(dataset->scenarios.begin(), dataset->scenarios.end(), scenario.avaframe_release);
        m_map_release_scenario = it == dataset->scenarios.end() ? 0 : int(it - dataset->scenarios.begin());
    }
    configure_map_release(scenario);
    graph->run();
    return true;
}

const AvalanchePanel::SandboxDataset* AvalanchePanel::map_release_dataset(const Scenario& scenario) const
{
    if (scenario.avaframe_dataset.empty())
        return nullptr;
    const auto it = std::find_if(
        m_sandbox_datasets.begin(), m_sandbox_datasets.end(), [&](const SandboxDataset& d) { return d.name == scenario.avaframe_dataset && d.geolocated; });
    return it == m_sandbox_datasets.end() ? nullptr : &*it;
}

void AvalanchePanel::configure_map_release(const Scenario& scenario)
{
    auto* node = find_node<nodes::AvaFrameReleaseNode>();
    if (node == nullptr)
        return; // a graph without the node (an older preset): the disc
    nodes::AvaFrameReleaseNode::AvaFrameReleaseSettings settings;
    if (const SandboxDataset* dataset = map_release_dataset(scenario)) {
        settings.dataset_path = QDir(QString::fromStdString(m_sandbox_root)).filePath(QString::fromStdString(dataset->name)).toStdString();
        settings.release_scenario = dataset->scenarios[size_t(std::clamp(m_map_release_scenario, 0, int(dataset->scenarios.size()) - 1))];
    }
    node->set_settings(settings);
}

void AvalanchePanel::draw_map_release()
{
    const Scenario& scenario = m_scenarios[size_t(m_selected_scenario)];
    if (scenario.avaframe_dataset.empty())
        return;
    const auto* node = find_node<nodes::AvaFrameReleaseNode>();
    const SandboxDataset* dataset = map_release_dataset(scenario);
    if (dataset != nullptr && node != nullptr && dataset->scenarios.size() > 1) {
        std::vector<const char*> names;
        for (const auto& name : dataset->scenarios)
            names.push_back(name.c_str());
        if (ImGui::Combo("Release area", &m_map_release_scenario, names.data(), int(names.size()))) {
            configure_map_release(scenario);
            if (auto* solver = find_solver()) {
                solver->request_reset();
                solver->clock().pause();
            }
            if (auto* graph = m_graph_panel->node_graph())
                graph->run();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("AvaFrame's release scenarios of this avalanche. %s is the one the AvaFrame comparison used.", scenario.avaframe_release.c_str());
    }
    if (const auto* area = node != nullptr ? node->release_area() : nullptr) {
        ImGui::TextDisabled("Release: AvaFrame %s, %.0f m3 on %.1f ha", area->label.c_str(), area->volume, area->area / 1e4);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("AvaFrame's release areas, resampled from its grid (Austria Lambert) onto the map's terrain through an\n"
                              "affine fit to the dataset's corners (good to under a DEM cell). An approximation: AvaFrame's outline\n"
                              "and depth (from AvaFrame's DEM), on the map's terrain. Exact in the AvaFrame sandbox.");
    } else if (dataset == nullptr) {
        ImGui::TextDisabled("Release: a disc of the same area as AvaFrame's.");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Download the AvaFrame data (AvaFrame sandbox, below) to release from AvaFrame's own release areas.");
    } else {
        ImGui::TextDisabled("Release: a disc (%s)", node != nullptr ? node->status().c_str() : "graph without an AvaFrame Release node");
    }
}

void AvalanchePanel::frame_scenario_when_ready(nodes::MpmSolverNode& solver)
{
    if (m_frame_pending == nullptr)
        return;
    // Wait for the run of this scenario: the domain sits on its centre and the terrain scan of
    // its reset has been read back (until then last_state() is invalid or the previous one).
    const auto& aabb = solver.domain_aabb();
    if (aabb.size().x <= 0.0 || !solver.last_state().valid)
        return;
    const glm::dvec2 centre = nucleus::srs::world_to_lat_long((aabb.min + aabb.max) * 0.5);
    if (glm::length(centre - m_frame_pending->domain_center) > 0.01)
        return;
    const auto& state = solver.last_state();
    const glm::dvec3 look_at(nucleus::srs::lat_long_to_world(m_frame_pending->domain_center), 0.5 * double(state.min_altitude + state.max_altitude));
    // Steeply from the south, far enough to frame the domain (world units are 1/cos(lat) m).
    const double distance = 1.5 * double(solver.domain_size_metres()) / std::max(solver.metres_per_world_unit(), 0.1);
    emit camera_requested(look_at + glm::normalize(glm::dvec3(0.0, -0.45, 1.0)) * distance, look_at);
    m_frame_pending = nullptr;
}

void AvalanchePanel::apply_material_preset(const MaterialPreset& preset)
{
    auto* solver = find_solver();
    if (solver == nullptr)
        return;

    auto s = solver->get_settings();
    s.constitutive_model = static_cast<nodes::MpmSolverNode::ConstitutiveModel>(preset.model);
    s.youngs_modulus = preset.youngs_modulus;
    s.poissons_ratio = preset.poissons_ratio;
    s.snow_density = preset.snow_density;
    s.terrain_friction = preset.terrain_friction;
    s.hardening = preset.hardening;
    s.critical_compression = preset.critical_compression;
    s.critical_stretch = preset.critical_stretch;
    s.dp_friction_angle = preset.dp_friction_angle;
    s.ccc_m = preset.ccc_m;
    s.ccc_beta = preset.ccc_beta;
    s.ccc_xi = preset.ccc_xi;
    s.ccc_p0_initial = preset.ccc_p0_initial;

    // A stiffer preset raises the wave speed and lowers the CFL bound - Li's 3 MPa is ~6x
    // faster than Stomakhin's 0.14 MPa. The node enforces the bound on every run; pulling dt
    // down here as well just shows the value it will run with right away.
    const float dx = s.domain_size_xy / float(std::max(s.grid_resolution_xy, 1u));
    const float cfl_dt = nodes::MpmSolverNode::max_stable_dt(s, dx);
    if (s.dt > cfl_dt)
        s.dt = 0.8f * cfl_dt;

    // The per-particle plastic state means different things per model (Jp, plastic
    // strain, alpha) - particles seeded under one model are garbage under another.
    s.reset_on_next_run = true;
    solver->set_settings(s);
}

void AvalanchePanel::scan_sandbox_datasets()
{
    m_sandbox_datasets.clear();
    m_sandbox_scan_error.clear();
    QFile index(QDir(QString::fromStdString(m_sandbox_root)).filePath("index.json"));
    if (m_sandbox_root.empty() || !index.open(QIODevice::ReadOnly)) {
        m_sandbox_scan_error = "No AvaFrame datasets yet - download them below (or run mpm-mls-doc/avaframe/prepare_dataset.py).";
        return;
    }
    for (const auto& value : QJsonDocument::fromJson(index.readAll()).object()["datasets"].toArray()) {
        const QJsonObject entry = value.toObject();
        SandboxDataset dataset;
        dataset.name = entry["name"].toString().toStdString();
        for (const auto& scenario : entry["release_scenarios"].toArray())
            dataset.scenarios.push_back(scenario.toString().toStdString());
        dataset.geolocated = entry["location"].isArray();
        dataset.has_reference = !entry["references"].toArray().isEmpty();
        if (!dataset.scenarios.empty())
            m_sandbox_datasets.push_back(std::move(dataset));
    }
    // Real terrain with a com1DFA reference first: those are the comparison cases.
    std::stable_sort(m_sandbox_datasets.begin(), m_sandbox_datasets.end(), [](const SandboxDataset& a, const SandboxDataset& b) {
        return (a.geolocated && a.has_reference) > (b.geolocated && b.has_reference);
    });
    m_sandbox_dataset = std::clamp(m_sandbox_dataset, 0, std::max(int(m_sandbox_datasets.size()) - 1, 0));
}

void AvalanchePanel::load_sandbox()
{
    m_graph_panel->load_preset(SANDBOX_GRAPH_PRESET);
    m_sandbox_configured.clear();
    m_startup_sandbox = true;
    save_startup_settings();
    m_scenario_error = apply_sandbox_dataset() ? std::string {} : std::string { "Could not load the sandbox dataset." };
}

bool AvalanchePanel::apply_sandbox_dataset()
{
    auto* graph = m_graph_panel->node_graph();
    auto* sandbox = find_node<nodes::AvaFrameSandboxNode>();
    if (graph == nullptr || sandbox == nullptr || m_sandbox_dataset >= int(m_sandbox_datasets.size()))
        return false;
    const auto& dataset = m_sandbox_datasets[size_t(m_sandbox_dataset)];
    auto settings = sandbox->get_settings();
    settings.dataset_path = QDir(QString::fromStdString(m_sandbox_root)).filePath(QString::fromStdString(dataset.name)).toStdString();
    settings.release_scenario = dataset.scenarios[size_t(std::clamp(m_sandbox_scenario, 0, int(dataset.scenarios.size()) - 1))];
    sandbox->set_settings(settings);
    if (auto* solver = find_solver()) {
        solver->request_reset();
        solver->clock().pause();
    }
    graph->run();
    return true;
}

void AvalanchePanel::configure_sandbox_solver(const nodes::AvaFrameSandboxNode& sandbox, nodes::MpmSolverNode& solver)
{
    const nodes::SandboxTerrain* terrain = sandbox.terrain();
    if (terrain == nullptr)
        return;
    const std::string key = terrain->dataset_path + "/" + terrain->scenario;
    if (key == m_sandbox_configured || solver.is_running())
        return;
    m_sandbox_configured = key;

    // Domain from the dataset's suggestion (release plus com1DFA footprint, with a margin),
    // cells as fine as the DEM where the grid limit allows: 5 m cells reach 2.56 km.
    auto settings = solver.get_settings();
    const double size = std::ceil(terrain->suggested_domain_size / 10.0) * 10.0;
    const double dx = std::max(terrain->cellsize, size / double(nodes::MpmSolverNode::MAX_GRID_RESOLUTION_XY));
    settings.domain_size_xy = float(size);
    settings.grid_resolution_xy = std::clamp(uint32_t(std::ceil(size / dx)), 64u, nodes::MpmSolverNode::MAX_GRID_RESOLUTION_XY);
    settings.splat_radius = float(dx);
    const float cfl_dt = nodes::MpmSolverNode::max_stable_dt(settings, float(size) / float(settings.grid_resolution_xy));
    if (settings.dt > cfl_dt)
        settings.dt = 0.8f * cfl_dt;
    settings.reset_on_next_run = true;
    solver.set_settings(settings);
    m_sandbox_stopped = false;
    m_sandbox_status.clear();
    step(solver);
    // A real dataset also shows on the map (AvaFrameSandboxNode::update_map_overlay): go there.
    if (terrain->sandbox_to_world)
        frame_on_map(*terrain);
}

void AvalanchePanel::look_at_flow(const nodes::MpmSolverNode& solver, double distance_metres)
{
    const auto* view = solver.particle_view();
    const auto& state = solver.last_state();
    if (!view->placed || !state.valid)
        return;
    // Centre of mass from solver metres to world: xy through the particle view, z like the
    // terrain (world units are 1/cos(latitude) metres, heights included).
    const glm::dvec3 com = state.centre_of_mass;
    const glm::dvec2 world_xy = glm::dvec2(view->world_from_metres * glm::dvec3(com.x, com.y, 1.0));
    const double world_per_metre = 1.0 / std::cos(glm::radians(nucleus::srs::world_to_lat_long(world_xy).x));
    const glm::dvec3 look_at(world_xy, com.z * world_per_metre);
    emit camera_requested(look_at + glm::normalize(glm::dvec3(0.0, -0.6, 1.0)) * distance_metres * world_per_metre, look_at);
}

void AvalanchePanel::frame_on_map(const nodes::SandboxTerrain& terrain)
{
    if (!terrain.sandbox_to_world)
        return;
    // Look at the avalanche (release and com1DFA footprint, the suggested domain) from the south,
    // steeply, at a distance that frames it. The fly-to of the search panel looks from 45 degrees
    // at 5 km, which hides north-facing gullies behind their ridge.
    const glm::dvec2 centre = terrain.suggested_domain_centre;
    const glm::dvec2 world = glm::dvec2(*terrain.sandbox_to_world * glm::dvec3(centre, 1.0));
    const double world_per_metre = glm::length(glm::dvec2((*terrain.sandbox_to_world)[0]));
    const glm::ivec2 cell = glm::clamp(glm::ivec2(int(centre.x / terrain.cellsize), int(terrain.grid_size.y) - 1 - int(centre.y / terrain.cellsize)),
        glm::ivec2(0), glm::ivec2(terrain.grid_size) - glm::ivec2(1));
    const double altitude = double(terrain.heights.pixel(glm::uvec2(cell)));
    const glm::dvec3 look_at(world, altitude);
    const double distance = 1.5 * terrain.suggested_domain_size * world_per_metre;
    emit camera_requested(look_at + glm::normalize(glm::dvec3(0.0, -0.45, 1.0)) * distance, look_at);
}

void AvalanchePanel::request_screenshot(const std::string& path)
{
    QFile::remove(QString::fromStdString(path));
    m_screenshot_path = path;
    m_phase_started = std::chrono::steady_clock::now();
    qInfo().nospace() << "AvaFrame batch: screenshot " << QString::fromStdString(path);
}

bool AvalanchePanel::screenshot_done()
{
    if (m_screenshot_path.empty() || QFile::exists(QString::fromStdString(m_screenshot_path)))
        return true;
    if (std::chrono::steady_clock::now() - m_phase_started > std::chrono::seconds(20)) {
        qWarning() << "AvaFrame batch: no screenshot after 20 s (is the capture script running?)" << QString::fromStdString(m_screenshot_path);
        return true;
    }
    return false;
}

void AvalanchePanel::export_sandbox_result(const std::string& stopped_by)
{
    auto* solver = find_solver();
    auto* sandbox = find_node<nodes::AvaFrameSandboxNode>();
    if (solver == nullptr || sandbox == nullptr || sandbox->terrain() == nullptr)
        return;
    QJsonObject run_info;
    QJsonObject solver_settings;
    solver->serialize_settings(solver_settings);
    run_info["solver_settings"] = solver_settings;
    run_info["material_preset"] = QString::fromStdString(m_material_presets[size_t(m_selected_material_preset)].name);
    run_info["stopped_by"] = QString::fromStdString(stopped_by);
    run_info["particles"] = int(solver->last_state().active_particles);
    // Entrainment, comparable with com1DFA's entrained mass and volume (at the cover's density).
    const auto& state = solver->last_state();
    run_info["entrainment"] = QJsonObject { { "mode", int(solver->get_settings().entrainment_mode) }, { "cover_volume_m3", state.cover_volume },
        { "cover_mass_kg", state.cover_mass }, { "entrained_particles", int(state.entrained_particles) }, { "particle_volume_m3", state.particle_volume },
        { "entrained_mass_kg", state.entrained_mass }, { "entrained_volume_m3", state.entrained_volume },
        { "pool_exhausted_volume_m3", state.pool_exhausted_volume } };
    run_info["gpu_ms_per_substep"] = double(solver->perf_stats().gpu_ms_per_substep);
    run_info["energy_line_mu_eff"] = double(solver->energy_line_friction());
    // Computing time: wall clock since the reset (pauses included, so meaningful for batch runs)
    // and the solver's own GPU time, measured per substep with timestamp queries.
    const auto& s = solver->get_settings();
    run_info["wall_time_s"] = double(solver->wall_time_since_reset());
    // Did the run touch the box? Shares of particle-substeps since the reset (see SimStateReadback).
    const auto& contacts = solver->contacts_since_reset();
    const double particle_substeps = std::max(double(contacts.particle_substeps), 1.0);
    run_info["wall_contact_share"] = double(contacts.wall) / particle_substeps;
    run_info["ceiling_contact_share"] = double(contacts.ceiling) / particle_substeps;
    run_info["domain_size_m"] = double(solver->domain_size_metres());
    run_info["gpu_time_s"] = double(solver->perf_stats().gpu_ms_per_substep) / 1000.0 * double(solver->simulated_time() / std::max(s.dt, 1e-6f));
    if (m_batch_phase != BatchPhase::Idle && m_batch_index < m_batch.size())
        run_info["label"] = QString::fromStdString(m_batch[m_batch_index].label);
    const float density = solver->get_settings().snow_density;
    const std::string key = sandbox->terrain()->dataset_path + "/" + sandbox->terrain()->scenario;
    m_sandbox_status = "Exporting...";
    solver->read_back_peaks([this, key, density, run_info](const nodes::MpmSolverNode::ColumnPeaks& peaks) {
        // The dataset may have changed while the readback was pending.
        auto* current = find_node<nodes::AvaFrameSandboxNode>();
        const nodes::SandboxTerrain* terrain = current ? current->terrain() : nullptr;
        if (terrain == nullptr || terrain->dataset_path + "/" + terrain->scenario != key)
            return;
        const auto rasters = nodes::resample_sandbox_peaks(*terrain, peaks, density);
        const auto summary = nodes::write_sandbox_result(*terrain, rasters, peaks, density, run_info);
        if (summary.directory.empty()) {
            m_sandbox_status = "Export failed: " + summary.error;
            m_last_export = "error: " + summary.error;
            qWarning() << "AvaFrame sandbox:" << QString::fromStdString(m_sandbox_status);
            return;
        }
        m_sandbox_status = "Exported to " + summary.directory;
        m_last_export = summary.directory;
        qInfo().nospace() << "AvaFrame sandbox: exported " << QString::fromStdString(summary.directory) << " - t " << peaks.simulated_time
                          << " s, max pft " << summary.max_thickness << " m, max pfv " << summary.max_speed << " m/s, footprint "
                          << summary.footprint_area / 1e4 << " ha (" << summary.footprint_area_10cm / 1e4 << " ha with pft > 0.1 m)";
    });
}

void AvalanchePanel::draw_avaframe_download()
{
    constexpr const char* POPUP = "Download AvaFrame data";
    if (m_avaframe_download != nullptr && m_avaframe_download->running()) {
        ImGui::ProgressBar(m_avaframe_download->progress(), ImVec2(-FLT_MIN, 0.0f));
        ImGui::TextWrapped("%s", qPrintable(m_avaframe_download->status()));
        if (ImGui::Button("Cancel download"))
            m_avaframe_download->cancel();
        return;
    }
    if (ImGui::Button(m_sandbox_datasets.empty() ? "Download AvaFrame data..." : "Download AvaFrame data again..."))
        ImGui::OpenPopup(POPUP);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Downloads AvaFrame's five real test avalanches from its GitHub repository\n"
                          "and prepares them for the sandbox (asks first).");
    if (m_avaframe_download != nullptr) {
        const auto state = m_avaframe_download->state();
        if (state == AvaFrameDownload::State::Failed)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", qPrintable(m_avaframe_download->status()));
        else if (state == AvaFrameDownload::State::Done || state == AvaFrameDownload::State::Cancelled)
            ImGui::TextDisabled("%s", qPrintable(m_avaframe_download->status()));
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal(POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
    ImGui::TextUnformatted("This downloads AvaFrame's five real test avalanches (Kontertal, Gallreide, Hintertux, Wolfsgruben, "
                           "Madlein) and prepares them for the sandbox: terrain, release areas and the com1DFA reference results.");
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "External data");
    ImGui::TextUnformatted("The files come from AvaFrame's public repository on GitHub (OpenNHM/AvaFrame, release 2.1). They are not "
                           "part of weBIGeo, and AvaFrame may move or remove them at any time; then the download fails.");
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "This takes a while");
    ImGui::TextUnformatted("About 40 MB of text rasters, then converting them. On a slow connection that can take several minutes.");
    ImGui::Spacing();
#ifdef __EMSCRIPTEN__
    ImGui::TextDisabled("Kept in the browser's memory; a reload of the page needs the download again.");
#else
    ImGui::TextDisabled("Saved to %s. Existing runs there are kept.", m_sandbox_root.c_str());
#endif
    ImGui::TextDisabled("Data sources: terrain Land Tirol - data.tirol.gv.at (CC BY 4.0); release areas: Austrian Torrent and "
                        "Avalanche Service (WLV), provided to AvaFrame.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ImGui::Button("Download")) {
        if (m_avaframe_download == nullptr) {
            m_avaframe_download = new AvaFrameDownload(this);
            connect(m_avaframe_download, &AvaFrameDownload::finished, this, &AvalanchePanel::avaframe_download_finished);
        }
        m_avaframe_download->start(QString::fromStdString(m_sandbox_root));
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void AvalanchePanel::avaframe_download_finished(bool ok)
{
    if (!ok)
        return;
    // Keep the selected dataset if it is still listed; the scan sorts the list anew.
    const std::string selected = m_sandbox_dataset < int(m_sandbox_datasets.size()) ? m_sandbox_datasets[size_t(m_sandbox_dataset)].name : std::string {};
    scan_sandbox_datasets();
    const auto found = std::find_if(m_sandbox_datasets.begin(), m_sandbox_datasets.end(), [&](const auto& d) { return d.name == selected; });
    if (found != m_sandbox_datasets.end()) {
        m_sandbox_dataset = int(found - m_sandbox_datasets.begin());
        m_sandbox_scenario = std::clamp(m_sandbox_scenario, 0, int(found->scenarios.size()) - 1);
    } else {
        m_sandbox_dataset = 0;
        m_sandbox_scenario = 0;
    }
    // Its files may have been rewritten: load it again.
    m_sandbox_configured.clear();
    if (find_node<nodes::AvaFrameSandboxNode>() != nullptr)
        m_scenario_error = apply_sandbox_dataset() ? std::string {} : std::string { "Could not load the sandbox dataset." };
}

void AvalanchePanel::draw_sandbox_section(nodes::AvaFrameSandboxNode& sandbox)
{
    ImGui::TextDisabled("AvaFrame sandbox: the solver runs on AvaFrame's DEM, not on the map.");
    if (m_sandbox_datasets.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", m_sandbox_scan_error.c_str());
    } else {
        std::vector<std::string> labels;
        for (const auto& dataset : m_sandbox_datasets)
            labels.push_back(dataset.name + (dataset.geolocated ? "" : " (synthetic)") + (dataset.has_reference ? "" : " - no reference"));
        std::vector<const char*> names;
        for (const auto& label : labels)
            names.push_back(label.c_str());
        bool changed = ImGui::Combo("Dataset", &m_sandbox_dataset, names.data(), int(names.size()));
        if (changed)
            m_sandbox_scenario = 0;
        const auto& scenarios = m_sandbox_datasets[size_t(m_sandbox_dataset)].scenarios;
        std::vector<const char*> scenario_names;
        for (const auto& scenario : scenarios)
            scenario_names.push_back(scenario.c_str());
        changed |= ImGui::Combo("Release", &m_sandbox_scenario, scenario_names.data(), int(scenario_names.size()));
        if (changed) {
            save_startup_settings();
            m_scenario_error = apply_sandbox_dataset() ? std::string {} : std::string { "Could not load the sandbox dataset." };
        }
    }
    draw_avaframe_download();

    if (const nodes::SandboxTerrain* t = sandbox.terrain()) {
        ImGui::TextDisabled("DEM %ux%u @ %.1f m, release %.0f m3 on %.2f ha, reference %s", t->grid_size.x, t->grid_size.y, t->cellsize,
            t->release_volume, t->release_area / 1e4, t->reference_name.empty() ? "none" : t->reference_name.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Release volume from AvaFrame's slope-normal thickness: sum of thickness x surface area.\n"
                              "Reference: the com1DFA benchmark run of this release scenario (AvaFrame v1.11).");
    }
    if (auto* solver = find_solver()) {
        auto settings = solver->get_settings();
        bool stop_at_rest = settings.stop_kinetic_fraction > 0.0f;
        if (ImGui::Checkbox("Stop at rest", &stop_at_rest)) {
            settings.stop_kinetic_fraction = stop_at_rest ? 0.01f : 0.0f;
            solver->set_settings(settings);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Pause once the kinetic energy has fallen below 1 %% of its peak - com1DFA's stop criterion\n"
                              "(the AvaFrame benchmarks: \"< 1.00 percent of PKE\").");
        ImGui::SameLine();
        if (ImGui::Checkbox("then export", &m_sandbox_auto_export))
            save_startup_settings();
        ImGui::SameLine();
        if (ImGui::Button("Export now"))
            export_sandbox_result("manual");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Peak flow thickness, velocity and pressure since the reset, as pft/pfv/ppr .asc on the DEM grid\n"
                              "in <dataset>/mpm/<time>_<release>/, with params.json - the same layout as AvaFrame's results.");
        if (!m_sandbox_status.empty())
            ImGui::TextWrapped("%s", m_sandbox_status.c_str());
    }
    if (ImGui::Button(m_sandbox_view.open ? "Hide sandbox view" : "Show sandbox view")) {
        m_sandbox_view.open = !m_sandbox_view.open;
        save_startup_settings();
    }
    if (const nodes::SandboxTerrain* t = sandbox.terrain(); t != nullptr && t->centre_lat_lon) {
        ImGui::SameLine();
        if (ImGui::Button("Show on the map"))
            frame_on_map(*t);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Flies the camera to the dataset. The overlay there shows the release (cyan), the MPM peak flow\n"
                              "thickness above 0.1 m (colour scale of the sandbox view) and the com1DFA footprint (green),\n"
                              "on weBIGeo's own terrain - placed by an affine fit to AvaFrame's DEM corners.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Back to the map")) {
        m_startup_sandbox = false;
        save_startup_settings();
        m_graph_panel->load_preset(MPM_GRAPH_PRESET);
        m_scenario_error = apply_scenario(m_scenarios[size_t(m_selected_scenario)]) ? std::string {} : std::string { "Could not apply the scenario." };
    }
}

bool AvalanchePanel::load_batch(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AvaFrame batch: cannot open" << path;
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (const auto& value : root["runs"].toArray()) {
        const QJsonObject r = value.toObject();
        BatchRun run;
        run.dataset = r["dataset"].toString().toStdString();
        run.scenario = r["scenario"].toString().toStdString();
        run.material_preset = r["material_preset"].toString().toStdString();
        run.settings = r["settings"].toObject();
        run.max_time = float(r["max_time"].toDouble(400.0));
        run.trajectories = r["model"].toString() == "trajectories";
        run.label = r["label"].toString(QString::fromStdString(run.dataset + "_" + std::to_string(m_batch.size()))).toStdString();
        m_batch.push_back(std::move(run));
    }
    m_batch_quit = root["quit"].toBool(true);
    m_batch_screenshots = root["screenshots"].toBool(false);
    m_batch_file = path;
    if (m_batch.empty()) {
        qWarning() << "AvaFrame batch: no runs in" << path;
        return false;
    }
    m_batch_phase = BatchPhase::Loading; // from here on, save_startup_settings() is a no-op
    m_sandbox_auto_export = false; // the batch exports itself (and plays at accuracy, Fast; see sync_clock)
    qInfo() << "AvaFrame batch:" << m_batch.size() << "runs from" << path;
    return true;
}

void AvalanchePanel::start_batch_run()
{
    while (m_batch_index < m_batch.size()) {
        const BatchRun& run = m_batch[m_batch_index];
        const auto dataset = std::find_if(m_sandbox_datasets.begin(), m_sandbox_datasets.end(), [&](const auto& d) { return d.name == run.dataset; });
        const auto preset = std::find_if(m_material_presets.begin(), m_material_presets.end(), [&](const auto& p) { return p.name == run.material_preset; });
        std::string error;
        if (dataset == m_sandbox_datasets.end())
            error = "unknown dataset " + run.dataset;
        else if (!run.material_preset.empty() && preset == m_material_presets.end())
            error = "unknown material preset " + run.material_preset;
        int scenario = 0;
        if (error.empty() && !run.scenario.empty()) {
            scenario = int(std::find(dataset->scenarios.begin(), dataset->scenarios.end(), run.scenario) - dataset->scenarios.begin());
            if (scenario >= int(dataset->scenarios.size()))
                error = "unknown release scenario " + run.scenario;
        }
        if (!error.empty()) {
            qWarning() << "AvaFrame batch: run" << QString::fromStdString(run.label) << "skipped:" << QString::fromStdString(error);
            m_batch_results.append(QJsonObject { { "label", QString::fromStdString(run.label) }, { "error", QString::fromStdString(error) } });
            m_batch_index++;
            continue;
        }
        qInfo().nospace() << "AvaFrame batch: run " << (m_batch_index + 1) << "/" << m_batch.size() << " " << QString::fromStdString(run.label);
        m_sandbox_dataset = int(dataset - m_sandbox_datasets.begin());
        m_sandbox_scenario = scenario;
        if (run.trajectories) {
            // A fresh graph per run: its layer buffers exist only once the model has run.
            m_graph_panel->load_preset(TRAJECTORIES_SANDBOX_GRAPH_PRESET);
            if (auto* model = find_node<nodes::ComputeAvalancheTrajectoriesNode>()) {
                QJsonObject settings;
                model->serialize_settings(settings);
                for (auto it = run.settings.begin(); it != run.settings.end(); ++it)
                    settings[it.key()] = it.value();
                model->deserialize_settings(settings);
            }
        } else if (find_node<nodes::AvaFrameSandboxNode>() == nullptr || find_solver() == nullptr) {
            m_graph_panel->load_preset(SANDBOX_GRAPH_PRESET);
        }
        if (preset != m_material_presets.end()) {
            m_selected_material_preset = int(preset - m_material_presets.begin());
            apply_material_preset(*preset);
        }
        m_sandbox_configured.clear(); // configure again even on the same dataset
        m_last_export.clear();
        m_batch_run_started = std::chrono::steady_clock::now();
        apply_sandbox_dataset();
        m_batch_phase = BatchPhase::Loading;
        return;
    }
    finish_batch();
}

void AvalanchePanel::batch_tick()
{
    const BatchRun& run = m_batch[m_batch_index];
    if (run.trajectories) {
        // The trajectory model runs once with the graph; export when its layers exist.
        auto* sandbox = find_node<nodes::AvaFrameSandboxNode>();
        auto* model = find_node<nodes::ComputeAvalancheTrajectoriesNode>();
        if (m_batch_phase == BatchPhase::Loading) {
            if (sandbox == nullptr || model == nullptr || sandbox->terrain() == nullptr || model->is_running())
                return;
            auto* zdelta = std::get<nodes::data_type<webgpu::raii::RawBuffer<uint32_t>*>()>(model->output_socket("layer1_zdelta").get_data());
            auto* counts = std::get<nodes::data_type<webgpu::raii::RawBuffer<uint32_t>*>()>(model->output_socket("layer2_cellCounts").get_data());
            if (zdelta == nullptr || counts == nullptr)
                return;
            QJsonObject settings;
            model->serialize_settings(settings);
            QJsonObject run_info { { "label", QString::fromStdString(run.label) }, { "trajectory_settings", settings },
                { "wall_time_s", std::chrono::duration<double>(std::chrono::steady_clock::now() - m_batch_run_started).count() } };
            m_last_export.clear();
            m_batch_phase = BatchPhase::Exporting;
            sandbox->export_trajectories(*zdelta, *counts, run_info, [this](const nodes::SandboxRunSummary& summary) {
                m_last_export = summary.directory.empty() ? "error: " + summary.error : summary.directory;
                qInfo().nospace() << "AvaFrame batch: trajectories exported " << QString::fromStdString(m_last_export) << ", footprint "
                                  << summary.footprint_area / 1e4 << " ha";
            });
            return;
        }
        if (m_batch_phase == BatchPhase::Exporting && !m_last_export.empty()) {
            m_batch_results.append(QJsonObject { { "label", QString::fromStdString(run.label) }, { "dataset", QString::fromStdString(run.dataset) },
                { "result", QString::fromStdString(m_last_export) } });
            m_batch_index++;
            start_batch_run();
        }
        return;
    }
    auto* solver_node = find_solver();
    if (solver_node == nullptr)
        return;
    auto& solver = *solver_node;
    switch (m_batch_phase) {
    case BatchPhase::Loading: {
        // Wait until the dataset is in and the solver sized for it, then apply the run's settings.
        auto* sandbox = find_node<nodes::AvaFrameSandboxNode>();
        const nodes::SandboxTerrain* t = sandbox ? sandbox->terrain() : nullptr;
        if (t == nullptr || m_sandbox_configured != t->dataset_path + "/" + t->scenario || !solver.has_valid_inputs() || solver.is_running())
            return;
        QJsonObject settings;
        solver.serialize_settings(settings);
        for (auto it = run.settings.begin(); it != run.settings.end(); ++it)
            settings[it.key()] = it.value();
        solver.deserialize_settings(settings); // also requests the reset
        // The time step from the CFL bound of this run's own grid and material, unless the run
        // sets one. Otherwise dt is whatever the previous run left (presets and the domain setup
        // only ever lower it), and a run's result would depend on its place in the batch.
        if (!run.settings.contains("dt")) {
            auto s = solver.get_settings();
            const float dx = std::min(s.domain_size_xy, float(std::max(t->size_metres.x, t->size_metres.y)))
                / float(std::clamp(s.grid_resolution_xy, 8u, nodes::MpmSolverNode::MAX_GRID_RESOLUTION_XY));
            s.dt = 0.8f * nodes::MpmSolverNode::max_stable_dt(s, dx);
            solver.set_settings(s);
        }
        m_sandbox_stopped = false;
        sync_clock(solver);
        solver.clock().play();
        m_batch_phase = BatchPhase::Running;
        return;
    }
    case BatchPhase::Running: {
        const bool rest = solver.at_rest();
        if (!rest && solver.simulated_time() < run.max_time)
            return;
        solver.clock().pause();
        m_sandbox_stopped = true;
        qInfo().nospace() << "AvaFrame batch: " << QString::fromStdString(run.label) << (rest ? " at rest after " : " reached the time limit at ")
                          << solver.simulated_time() << " s";
        m_last_export.clear();
        export_sandbox_result(rest ? "at rest" : "max time");
        m_batch_phase = BatchPhase::Exporting;
        return;
    }
    case BatchPhase::Exporting:
        if (m_last_export.empty())
            return;
        m_batch_results.append(QJsonObject { { "label", QString::fromStdString(run.label) }, { "dataset", QString::fromStdString(run.dataset) },
            { "result", QString::fromStdString(m_last_export) } });
        if (m_batch_screenshots && m_last_export.rfind("error", 0) != 0) {
            // The sandbox view with the MPM peak thickness first, then the map with the overlay.
            m_sandbox_view.open = true;
            m_sandbox_view.set_layer(SandboxView::Layer::MpmThickness);
            m_screenshot_path.clear();
            m_phase_started = std::chrono::steady_clock::now();
            m_batch_phase = BatchPhase::ShotSandbox;
            return;
        }
        m_batch_index++;
        start_batch_run();
        return;
    case BatchPhase::ShotSandbox:
    case BatchPhase::ShotMap: {
        const bool sandbox_shot = m_batch_phase == BatchPhase::ShotSandbox;
        if (m_screenshot_path.empty()) {
            // Wait until the view (and the map overlay) show this run's final peaks, then a moment
            // for the frames and the map tiles to settle.
            const bool settled = std::chrono::steady_clock::now() - m_phase_started > std::chrono::milliseconds(sandbox_shot ? 1500 : 2500);
            if (!m_sandbox_view.peaks_current(solver.simulated_time()) || !settled)
                return;
            request_screenshot(m_last_export + (sandbox_shot ? "/sandbox.png" : "/map.png"));
            return;
        }
        if (!screenshot_done())
            return;
        m_screenshot_path.clear();
        if (sandbox_shot) {
            m_sandbox_view.open = false;
            m_phase_started = std::chrono::steady_clock::now();
            m_batch_phase = BatchPhase::ShotMap;
            return;
        }
        m_sandbox_view.open = true;
        m_batch_index++;
        start_batch_run();
        return;
    }
    default:
        return;
    }
}

void AvalanchePanel::finish_batch()
{
    m_batch_phase = BatchPhase::Done;
    QFile out(m_batch_file + ".results.json");
    if (out.open(QIODevice::WriteOnly))
        out.write(QJsonDocument(QJsonObject { { "runs", m_batch_results } }).toJson());
    qInfo() << "AvaFrame batch: done," << m_batch_results.size() << "runs, results in" << out.fileName();
    if (m_batch_quit) {
        SDL_Event quit {};
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
    }
}

void AvalanchePanel::draw()
{
    // In the map graph, the 3D particles replace the 2D density overlay: show one or the other.
    if (auto* particles = find_node<nodes::ParticleRenderNode>()) {
        if (auto* overlay = find_node<nodes::OverlayRenderNode>())
            overlay->set_overlay_enabled(!particles->renderer_settings().enabled);
        float min_altitude = 0.0f, max_altitude = 0.0f; // unknown until the solver reports its terrain scan
        if (auto* solver = find_solver(); solver != nullptr && solver->last_state().valid) {
            min_altitude = solver->last_state().min_altitude;
            max_altitude = solver->last_state().max_altitude;
        }
        particles->sync_snow_cover(min_altitude, max_altitude);
    }

    if (auto* solver = find_solver(); solver != nullptr && find_node<nodes::AvaFrameSandboxNode>() == nullptr)
        frame_scenario_when_ready(*solver);

    if (m_batch_phase != BatchPhase::Idle && m_batch_phase != BatchPhase::Done && m_batch_index < m_batch.size()) {
        if (auto* sandbox = find_node<nodes::AvaFrameSandboxNode>(); sandbox != nullptr && find_solver() != nullptr)
            configure_sandbox_solver(*sandbox, *find_solver());
        batch_tick();
    }
    if (auto* sandbox = find_node<nodes::AvaFrameSandboxNode>()) {
        if (auto* solver = find_solver()) {
            configure_sandbox_solver(*sandbox, *solver);
            // com1DFA's stop criterion: pause once at rest and export if asked to. Play continues
            // past it; a reset (simulated time going back) re-arms it.
            if (solver->simulated_time() < m_sandbox_last_time)
                m_sandbox_stopped = false;
            m_sandbox_last_time = solver->simulated_time();
            if (solver->clock().playing() && solver->at_rest() && !m_sandbox_stopped && m_batch_phase == BatchPhase::Idle) {
                m_sandbox_stopped = true;
                solver->clock().pause();
                m_sandbox_status = "At rest after " + std::to_string(int(solver->simulated_time())) + " s (kinetic energy < 1 % of its peak).";
                qInfo().nospace() << "AvaFrame sandbox: at rest after " << solver->simulated_time() << " s";
                if (m_sandbox_auto_export)
                    export_sandbox_result("at rest");
            }
            m_sandbox_view.draw(*sandbox, *solver);
            if (m_sandbox_view.layer_changed) {
                m_sandbox_view.layer_changed = false;
                save_startup_settings();
            }
        }
    }

    // The clock follows the panel's playback settings every frame; SimulationDriver (ticked by
    // ImGuiManager) does the stepping, also with the GUI hidden.
    auto* solver = find_solver();
    if (solver == nullptr)
        return;
    sync_clock(*solver);
    sync_entrainment(*solver);
    if (m_autoplay_pending && solver->has_valid_inputs() && !solver->is_running()) {
        solver->clock().play();
        m_autoplay_pending = false;
    }
}

AvalanchePanel::Playback& AvalanchePanel::playback() { return find_node<nodes::AvaFrameSandboxNode>() != nullptr ? m_sandbox_playback : m_map_playback; }

void AvalanchePanel::sync_clock(nodes::MpmSolverNode& solver)
{
    // A batch measures the solver, not the GPU budget: accuracy, Fast, whatever the user set.
    const bool batch = m_batch_phase != BatchPhase::Idle && m_batch_phase != BatchPhase::Done;
    const Playback current = batch ? Playback { SimulationClock::Mode::Accuracy, Pacing::Fast } : playback();
    SimulationClock::Settings clock;
    clock.mode = current.mode;
    clock.speed = m_playback_speed;
    clock.interpolate = m_playback_interpolate;
    clock.budget_ms = pacing_budget_ms(current.pacing); // 0 for Manual
    clock.manual_steps_per_submit = solver.get_settings().substeps_per_submit;
    clock.manual_steps_per_run = solver.get_settings().substeps_per_run;
    solver.clock().set_settings(clock);
}

nodes::MpmSolverNode::EntrainmentMode& AvalanchePanel::entrainment_mode()
{
    return find_node<nodes::AvaFrameSandboxNode>() != nullptr ? m_sandbox_entrainment : m_map_entrainment;
}

void AvalanchePanel::sync_entrainment(nodes::MpmSolverNode& solver)
{
    // A batch run sets entrainment in its own settings, like everything else it measures.
    if (m_batch_phase != BatchPhase::Idle && m_batch_phase != BatchPhase::Done)
        return;
    const auto mode = entrainment_mode();
    auto settings = solver.get_settings();
    if (settings.entrainment_mode == mode)
        return;
    const bool on_off = (settings.entrainment_mode == nodes::MpmSolverNode::ENTRAINMENT_OFF) != (mode == nodes::MpmSolverNode::ENTRAINMENT_OFF);
    settings.entrainment_mode = mode;
    solver.set_settings(settings);
    if (on_off)
        solver.request_reset(); // the cover is seeded with the release
}

void AvalanchePanel::draw_entrainment(nodes::MpmSolverNode& solver, nodes::MpmSolverNode::MpmSolverSettings& settings, bool& settings_changed, bool& step_now)
{
    using Solver = nodes::MpmSolverNode;
    ImGui::TextDisabled("Entrainment");
    auto& mode = entrainment_mode();
    int selected = int(mode);
    if (ImGui::Combo("Entrainment", &selected, "Off\0Plowing\0Erosion (com1DFA default)\0")) {
        mode = Solver::EntrainmentMode(selected);
        settings.entrainment_mode = mode;
        settings_changed = true;
        sync_entrainment(solver); // reseeds when it went on or off
        settings = solver.get_settings();
        save_startup_settings();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The flow picks up the snow cover along its path, as AvaFrame's com1DFA does.\n"
                          "Plowing: the front takes the whole cover at once. Erosion: the flow erodes it at\n"
                          "basal shear stress x speed / erosion energy per area, so it is taken up gradually.\n"
                          "Switching it on or off restarts the simulation (the cover is seeded with the release).");
    if (mode == Solver::ENTRAINMENT_OFF)
        return;

    if (mode == Solver::EROSION) {
        settings_changed |= ImGui::DragFloat("Erosion energy", &settings.erosion_energy, 50.0f, 100.0f, 100000.0f, "%.0f J/kg");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Energy to erode one kilogram of cover (com1DFA's entEroEnergy, 5000).\nLower = the flow picks the cover up faster.");
    } else {
        settings_changed |= ImGui::DragFloat("Pick-up depth", &settings.plowing_depth, 0.01f, 0.0f, 3.0f, "%.2f m");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Flow depth over a cover particle at which the front takes it along.");
    }
    settings_changed |= ImGui::DragFloat("Minimum speed", &settings.entrainment_min_speed, 0.1f, 0.0f, 20.0f, "%.1f m/s");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Slower flow picks nothing up.");

    // Where the cover comes from. Its size changes the seed, so those edits reset on release.
    ImGui::TextDisabled("Snow cover (changes reset the run)");
    const auto* sandbox = find_node<nodes::AvaFrameSandboxNode>();
    const nodes::SandboxTerrain* terrain = sandbox != nullptr ? sandbox->terrain() : nullptr;
    const auto* release_node = sandbox == nullptr ? find_node<nodes::AvaFrameReleaseNode>() : nullptr;
    const nodes::ReleaseArea* area = release_node != nullptr ? release_node->release_area() : nullptr;
    if (terrain != nullptr) {
        if (terrain->entrainment_volume > 0.0)
            ImGui::TextDisabled("AvaFrame entrainment area %s: %.0f m3 on %.1f ha", terrain->entrainment_name.c_str(), terrain->entrainment_volume,
                terrain->entrainment_area / 1e4);
        else
            ImGui::TextDisabled("This dataset has no entrainment area (download the AvaFrame data again).");
    } else if (area != nullptr && area->entrainment_volume > 0.0) {
        ImGui::TextDisabled("AvaFrame entrainment area: %.0f m3 on %.1f ha", area->entrainment_volume, area->entrainment_area / 1e4);
    } else {
        settings_changed |= ImGui::DragFloat("Cover thickness", &settings.cover_thickness, 0.01f, 0.0f, 3.0f, "%.2f m");
        step_now |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Snow on the ground the avalanche can pick up, measured perpendicular to the slope\n"
                              "(AvaFrame uses 0.3 m). Everywhere in the domain outside the release, up to the slope below.");
        settings_changed |= ImGui::DragFloat("Cover up to", &settings.cover_max_slope, 0.5f, 0.0f, 85.0f, "%.0f deg");
        step_now |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Steeper slopes hold no snow cover.");
    }
    settings_changed |= ImGui::DragFloat("Cover density", &settings.entrainment_density, 1.0f, 10.0f, 600.0f, "%.0f kg/m3");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Density of the snow cover (com1DFA's rhoEnt, 100). It sets how much mass the cover holds.");
    if (step_now)
        settings.reset_on_next_run = true;

    const auto& state = solver.last_state();
    if (state.valid && state.cover_volume > 0.0) {
        ImGui::TextDisabled("Cover %.0f m3 (%.0f t); entrained %.0f m3 (%.0f t, %.1f %%)", state.cover_volume, state.cover_mass / 1000.0,
            state.entrained_volume, state.entrained_mass / 1000.0, 100.0 * state.entrained_volume / state.cover_volume);
        if (state.pool_exhausted_volume > 0.0) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Pool full: %.0f m3 of cover left on the ground.", state.pool_exhausted_volume);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("The flow took up as much snow as the pool of %.0f particles per release particle holds.\n"
                                  "A larger pool (graph editor, pool_size) lets it take more.", double(settings.pool_size));
        }
    } else if (state.valid) {
        ImGui::TextDisabled("No snow cover in the domain.");
    }
}

bool AvalanchePanel::playing() const
{
    const auto* solver = find_solver();
    return solver != nullptr && solver->clock().playing();
}

void AvalanchePanel::set_playing(bool playing)
{
    auto* solver = find_solver();
    if (solver == nullptr)
        return;
    if (playing) {
        sync_clock(*solver);
        solver->clock().play();
    } else {
        solver->clock().pause();
    }
}

void AvalanchePanel::step(nodes::MpmSolverNode& solver)
{
    sync_clock(solver);
    solver.clock().step(solver);
}

void AvalanchePanel::draw_playback(nodes::MpmSolverNode& solver)
{
    Playback& current = playback();
    bool changed = false;
    int mode = int(current.mode);
    if (ImGui::Combo("Playback", &mode, "Real time\0Accuracy (as fast as possible)\0")) {
        current.mode = SimulationClock::Mode(mode);
        changed = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Real time: simulated time follows the clock times the speed below, like a game engine's\n"
                          "fixed-step loop - for watching the avalanche. The time step stays fixed (CFL bound), so a\n"
                          "GPU that cannot keep up within the budget slows the simulation down instead.\n"
                          "Accuracy: as many substeps as the budget allows, as fast as the GPU goes - for results.\n"
                          "Remembered separately for the map and the sandbox.");
    if (current.mode == SimulationClock::Mode::RealTime) {
        changed |= ImGui::SliderFloat("Speed", &m_playback_speed, SimulationClock::MIN_SPEED, SimulationClock::MAX_SPEED, "%.2fx",
            ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Simulated seconds per second. Ctrl+click to type a value.");
        changed |= ImGui::Checkbox("Interpolate", &m_playback_interpolate);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Draws the particles between the last two simulated states, at the time the clock says,\n"
                              "like a game engine: every frame moves by the same amount, also at speeds where one\n"
                              "time step is longer than a frame. Off: every frame shows the latest simulated state.");
    }
    int pacing = int(current.pacing);
    if (ImGui::Combo("Pacing", &pacing, "Manual\0Smooth (60 fps)\0Balanced (30 fps)\0Fast simulation\0")) {
        current.pacing = Pacing(pacing);
        changed = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The GPU has one queue, so simulation and rendering take turns. The automatic modes give\n"
                          "the simulation a fixed GPU budget per frame (6 / 14 / 30 ms) and pick the substeps per\n"
                          "frame from the measured cost of one substep. Manual uses the values below as they are.");
    if (changed) {
        sync_clock(solver);
        save_startup_settings();
    }
}

void AvalanchePanel::draw_performance(nodes::MpmSolverNode& solver)
{
    const auto& perf = solver.perf_stats();
    const auto& clock = solver.clock();
    if (clock.playing() && clock.measured_speed() > 0.0f) {
        const float budget_steps = clock.steps_per_frame_budget();
        if (clock.settings().mode == SimulationClock::Mode::Accuracy) {
            ImGui::TextDisabled("Speed: %.2fx real time, %u substeps/frame", double(clock.measured_speed()), clock.steps_per_submit());
        } else if (clock.behind()) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Speed: %.2fx real time - behind %.2fx", double(clock.measured_speed()),
                double(clock.settings().speed));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("The GPU budget of the pacing mode allows %.1f substeps per frame, too few for this speed.\n"
                                  "A larger budget (Pacing), a lower Detail or a lower speed catches up.",
                    double(budget_steps));
        } else {
            ImGui::TextDisabled("Speed: %.2fx real time, budget %.1f substeps/frame", double(clock.measured_speed()), double(budget_steps));
        }
    }
    if (perf.gpu_ms_per_substep > 0.0f)
        ImGui::TextDisabled("GPU: %.2f ms/substep, %.0f ms/run, %.0f MiB", double(perf.gpu_ms_per_substep), double(perf.mean_run_ms),
            double(perf.gpu_bytes) / (1024.0 * 1024.0));
    else if (perf.gpu_bytes > 0)
        ImGui::TextDisabled("%.0f ms/run, %.0f MiB on the GPU", double(perf.mean_run_ms), double(perf.gpu_bytes) / (1024.0 * 1024.0));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("GPU time per MPM substep (timestamp queries on the solver's own pass), wall time\n"
                          "per run including frame waits, and the solver's own GPU buffers.");
}

void AvalanchePanel::draw_panel()
{
    // The buttons below can load another preset ("Back to the map", "AvaFrame sandbox"), which
    // replaces the graph and destroys every node in it, the solver included. Once that happened,
    // stop drawing this frame; the next one finds the new nodes.
    const auto* graph_at_start = m_graph_panel->node_graph();
    const auto graph_replaced = [&] { return m_graph_panel->node_graph() != graph_at_start; };
    auto* solver = find_solver();
    if (!ImGui::CollapsingHeader(ICON_FA_SNOWFLAKE "  Avalanche", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (solver == nullptr) {
        // The active graph has no MLS-MPM node - offer the preset instead of hiding.
        ImGui::TextDisabled("The active graph has no MLS-MPM simulation.");
        if (ImGui::Button("Load the MLS-MPM simulation")) {
            m_graph_panel->load_preset(MPM_GRAPH_PRESET);
            m_scenario_error = apply_scenario(m_scenarios[size_t(m_selected_scenario)]) ? std::string {} : std::string { "Could not apply the scenario." };
        }
        if (ImGui::Button("Load the AvaFrame sandbox"))
            load_sandbox();
        draw_startup_settings();
        return;
    }

    auto* sandbox = find_node<nodes::AvaFrameSandboxNode>();
    if (sandbox != nullptr) {
        draw_sandbox_section(*sandbox);
        if (!m_scenario_error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_scenario_error.c_str());
        ImGui::Separator();
    }

    // --- Location ---
    // Needs a GeoRegionNode in the graph; a GPX-driven graph has no coordinates to set.
    if (sandbox == nullptr) {
        const bool can_switch_location = find_node<nodes::GeoRegionNode>() != nullptr;
        ImGui::BeginDisabled(!can_switch_location);
        std::vector<const char*> names;
        names.reserve(m_scenarios.size());
        for (const auto& scenario : m_scenarios)
            names.push_back(scenario.name.c_str());
        if (ImGui::Combo("Location", &m_selected_scenario, names.data(), int(names.size()))) {
            m_scenario_error = apply_scenario(m_scenarios[size_t(m_selected_scenario)])
                ? std::string {}
                : std::string { "Could not apply - the active graph has no GeoRegionNode." };
            save_startup_settings();
        }
        ImGui::EndDisabled();

        if (!can_switch_location) {
            ImGui::TextDisabled("Load the MLS-MPM preset to switch location.");
        } else {
            ImGui::TextDisabled("%s", m_scenarios[size_t(m_selected_scenario)].note.c_str());
            draw_map_release();
        }
        if (!m_scenario_error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_scenario_error.c_str());
        if (ImGui::Button("AvaFrame sandbox"))
            load_sandbox();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Run the solver on an AvaFrame test case (DEM and release area) instead of the map,\n"
                              "for comparison with com1DFA. The datasets are downloaded from AvaFrame there.");

        ImGui::Separator();
    }

    if (graph_replaced())
        return;
    const bool ready = solver->has_valid_inputs();
    if (!ready) {
        solver->clock().pause();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Inputs not ready.");
        ImGui::TextDisabled("Run the compute graph once (Shift+R).");
        return;
    }

    auto settings = solver->get_settings();
    bool settings_changed = false;
    bool step_now = false;

    ImGui::Text("Simulated time: %.2f s", double(solver->simulated_time()));

    const auto& aabb = solver->domain_aabb();
    if (aabb.size().x > 0.0 && sandbox == nullptr) {
        const glm::dvec2 centre = nucleus::srs::world_to_lat_long((aabb.min + aabb.max) * 0.5);
        ImGui::TextDisabled("Centre: %.5f, %.5f", centre.x, centre.y);
    }

    // Read back from the GPU after each run, so it lags one run behind the display.
    const auto& state = solver->last_state();
    if (state.valid && state.active_particles > 0) {
        const float plastic_ratio = 100.0f * float(state.plastic_particles) / float(state.active_particles);
        ImGui::TextDisabled("%u particles, %.0f%% plastic, max %.1f m/s, terrain %.0f-%.0f m", state.active_particles, double(plastic_ratio),
            double(state.max_speed), double(state.min_altitude), double(state.max_altitude));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Plastic ratio: particles whose return mapping yielded in the last substep.\n"
                              "Same definition as Li et al. 2021, who report 77 / 26 / 10 / 34 %% for their cases I-IV\n"
                              "at t = 10 s - but at 0.5 m cells with an associative flow rule, so expect different values.");
        // Artificial boundaries: a run that touches them depends on the box, not just the physics.
        if (state.ceiling_contacts > 0 || state.wall_contacts > 0) {
            const double particle_substeps = double(state.active_particles) * double(std::max(state.run_substeps, 1u));
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Box edge reached: %.2f%% at the band ceiling, %.2f%% at the walls",
                100.0 * double(state.ceiling_contacts) / particle_substeps, 100.0 * double(state.wall_contacts) / particle_substeps);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Share of particle-substeps in the last run that reached the edge of the simulation box, where the\n"
                                  "grid stops outward flow. Ceiling: raise Grid layers. Walls: enlarge the domain or move it.\n"
                                  "Until this stays at 0, runout and deposits are not purely the model's result.");
        }
    } else if (state.valid) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "0 particles seeded - move the release zone or tick Seed anywhere.");
    }

    // Energy-line test (com1DFA section 5.2, App. A): Coulomb friction removes exactly mu of
    // energy height per horizontal metre each particle travels, so the fitted slope over the
    // mass-averaged path is the effective friction the flow experiences; under Coulomb the
    // excess over the set mu is internal (plastic) dissipation.
    const auto& energy_line = solver->energy_line();
    const float mu_eff = solver->energy_line_friction();
    if (!energy_line.empty()) {
        const bool coulomb = settings.basal_friction_model == nodes::MpmSolverNode::COULOMB;
        if (std::isfinite(mu_eff)) {
            ImGui::TextDisabled("Energy line: mu_eff %.3f over %.0f m (basal mu %.2f, %s %+.3f)", double(mu_eff), double(energy_line.back().path),
                double(settings.terrain_friction), coulomb ? "internal" : "drag + internal", double(mu_eff - settings.terrain_friction));
        } else {
            ImGui::TextDisabled("Energy line: %zu samples, not moving yet", energy_line.size());
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Mean energy height z + v^2/2g of the particles against their mean horizontal path.\n"
                              "Slope = -mu_eff. For pure Coulomb sliding mu_eff == mu; anything above it is dissipated\n"
                              "inside the snow - or, with Voellmy, also by the turbulent drag. Tonnel et al. 2023, com1DFA\n"
                              "section 5.2 and App. A.");
        if (energy_line.size() >= 2) {
            std::vector<float> heights;
            heights.reserve(energy_line.size());
            for (const auto& sample : energy_line)
                heights.push_back(sample.energy_height);
            ImGui::PlotLines("##energy_line", heights.data(), int(heights.size()), 0, "energy height vs run", FLT_MAX, FLT_MAX, ImVec2(0, 40));
        }
    }

    if (ImGui::Button(playing() ? ICON_FA_PAUSE "  Pause" : ICON_FA_PLAY "  Play", ImVec2(110, 0)))
        set_playing(!playing());
    ImGui::SameLine();
    if (ImGui::Button("Step", ImVec2(80, 0)))
        step_now = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(80, 0))) {
        solver->request_reset();
        settings = solver->get_settings();
        step_now = true;
    }

    draw_playback(*solver);
    draw_performance(*solver);

    ImGui::Separator();

    // --- 3D view of the particles (ParticleRenderNode -> AvalancheRenderer) ---
    if (auto* particles = find_node<nodes::ParticleRenderNode>()) {
        auto& view = particles->renderer_settings();
        ImGui::Checkbox("3D particles", &view.enabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Draws every particle as a small sphere on the map, lit like the terrain.\n"
                              "It replaces the 2D density overlay, which comes back when this is off.");
        if (view.enabled) {
            ImGui::SameLine();
            int colour = int(view.colour_mode);
            ImGui::SetNextItemWidth(110);
            if (ImGui::Combo("##particle_colour", &colour, "Snow\0Speed\0"))
                view.colour_mode = webgpu_engine::AvalancheRenderer::ColourMode(colour);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Speed: viridis from 0 to %.0f m/s.", double(view.speed_max));
            ImGui::Checkbox("Flow surface", &view.surface);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("A snow blanket over the dense part of the flow: the particles' volume spread with the\n"
                                  "solver's B-spline, drawn where it is at least as deep as %u particles over 3x3 grid\n"
                                  "columns. Particles above it and in thin parts stay visible as the front and spray.",
                    view.surface_min_count);
            ImGui::SameLine();
            if (ImGui::Button("Look at the flow"))
                look_at_flow(*solver, 400.0);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Moves the camera to 400 m from the snow's centre of mass, steeply from the south.");
            if (view.surface) {
                ImGui::Checkbox("Track", &view.track);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("A thin, duller layer wherever the flow has been since the release (its peak depth,\n"
                                      "like AvaFrame's peak flow thickness): the path stays visible where the flow has\n"
                                      "thinned out or moved on. Cleared by Reset.");
                ImGui::SameLine();
                ImGui::Checkbox("Entrainable snow", &view.entrainment_cover);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("With entrainment: the snow cover the flow can still pick up, as a layer of fresh,\n"
                                      "slightly blue snow. The flow erodes it as it passes, so it ends at the track. Nothing\n"
                                      "is drawn with entrainment off.");
                ImGui::SameLine();
            }
            ImGui::Checkbox("Snow cover", &view.snow_cover);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Snow on the terrain around the avalanche, lit like the terrain: a screen-space snow\n"
                                  "overlay that leaves the avalanche's own pixels alone. Snow line and slope limits are\n"
                                  "in the Overlays panel (\"Avalanche snow cover\").");
            if (view.surface) {
                int refine = int(view.surface_refine);
                if (ImGui::SliderInt("Surface detail", &refine, 1, 4, "%dx the grid"))
                    view.surface_refine = uint32_t(refine);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Render grid of the blanket, finer than the solver's grid. The snow is spread at the\n"
                                      "solver's resolution, so finer steps only draw the same field more smoothly.");
            }
            using Shape = webgpu_engine::AvalancheRenderer::Shape;
            int shape = int(view.shape);
            if (ImGui::Combo("Particle shape", &shape, "Spheres\0Strain-rate ellipsoids\0Powder (transparent)\0"))
                view.shape = Shape(shape);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ellipsoids stretch each particle along its current strain rate (the symmetric part of\n"
                                  "its velocity gradient): elongated where the flow stretches or shears, flattened where it\n"
                                  "compacts. They show what the flow does now, not how far it has deformed.\n"
                                  "Powder draws each particle as a large, soft, cloudy, transparent blob: a haze over the\n"
                                  "flow surface that shows where snow is airborne or thin, while the surface carries the mass.");
            if (view.shape == Shape::Powder) {
                ImGui::SliderFloat("Powder size", &view.powder_size, 1.0f, 10.0f, "%.1f x spacing");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Blob radius as a multiple of the particle spacing. Larger blobs merge into a cloud\n"
                                      "but cost more (each covers more pixels).");
                ImGui::SliderFloat("Powder opacity", &view.powder_opacity, 0.02f, 1.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Alpha at a blob's centre; it falls off towards the edge and is broken up by noise.");
            } else {
                ImGui::SliderFloat("Particle size", &view.radius_scale, 0.2f, 2.0f, "%.2f x spacing");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Sphere radius as a share of the particle spacing (the release volume per particle).");
                static constexpr uint32_t CHILD_COUNTS[] = { 1u, 8u, 27u };
                int children = int(std::find(std::begin(CHILD_COUNTS), std::end(CHILD_COUNTS), view.children) - std::begin(CHILD_COUNTS));
                if (ImGui::Combo("Detail particles", &children, "1 per particle\0" "8 per particle\0" "27 per particle\0"))
                    view.children = CHILD_COUNTS[std::clamp(children, 0, 2)];
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Rendering only: each simulated particle is drawn as several smaller ones scattered within\n"
                                      "two radii of it, with the same total volume. The detail is invented, not simulated; the\n"
                                      "clusters move with their particle and merge back into it from afar. 27 is heavy on\n"
                                      "integrated GPUs.");
            }
            if (view.shape == Shape::StrainRate) {
                ImGui::SliderFloat("Strain window", &view.strain_window, 0.02f, 1.0f, "%.2f s", ImGuiSliderFlags_Logarithmic);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("How long the current strain rate acts on the shape. Longer shows weaker strain;\n"
                                      "each axis is capped at +/-70 %%.");
            }
        }
        ImGui::Separator();
    }

    // --- Detail ---
    int detail = -1;
    for (int i = 0; i < int(std::size(DETAIL_PRESETS)); i++) {
        if (settings.num_particles == DETAIL_PRESETS[i].particles && settings.grid_layers == DETAIL_PRESETS[i].grid_layers)
            detail = i;
    }
    const char* detail_label = detail >= 0 ? DETAIL_PRESETS[detail].name : "(custom)";
    if (ImGui::BeginCombo("Detail", detail_label)) {
        for (int i = 0; i < int(std::size(DETAIL_PRESETS)); i++) {
            if (ImGui::Selectable(DETAIL_PRESETS[i].name, i == detail)) {
                settings.num_particles = DETAIL_PRESETS[i].particles;
                settings.grid_layers = DETAIL_PRESETS[i].grid_layers;
                settings.reset_on_next_run = true; // reallocates the particle and grid buffers
                settings_changed = true;
                step_now = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Particle count and grid layers above the terrain - the two settings that decide the GPU\n"
                          "cost. Changing it restarts the simulation.");

    const uint32_t min_substeps = 1, max_substeps = 512;
    ImGui::BeginDisabled(playback().pacing != Pacing::Manual);
    settings_changed |= ImGui::DragScalar("Substeps per run", ImGuiDataType_U32, &settings.substeps_per_run, 1.0f, &min_substeps, &max_substeps, "%u");
    settings_changed |= ImGui::DragScalar("Substeps per frame", ImGuiDataType_U32, &settings.substeps_per_submit, 1.0f, &min_substeps, &max_substeps, "%u");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("A run is submitted in chunks of this many substeps, one per frame: fewer = smoother view,\n"
                          "less simulated time per second. Both are set automatically unless Pacing is Manual.");
    settings_changed |= ImGui::DragFloat("Time step", &settings.dt, 0.0005f, 0.0001f, 0.5f, "%.4f s");
    settings_changed |= ImGui::DragFloat("Splat radius", &settings.splat_radius, 0.25f, 0.0f, 64.0f, "%.1f m");

    // Two independent choices - internal snow behaviour vs. contact with the ground - kept
    // side by side here so the distinction is visible. Combo strings must stay in enum order.
    ImGui::TextDisabled("Models");

    // Presets write the ordinary settings; they are not a third configuration path.
    {
        std::vector<const char*> preset_names;
        preset_names.reserve(m_material_presets.size());
        for (const auto& preset : m_material_presets)
            preset_names.push_back(preset.name.c_str());
        if (ImGui::Combo("Material preset", &m_selected_material_preset, preset_names.data(), int(preset_names.size()))) {
            save_startup_settings();
            if (m_selected_material_preset > 0) {
                apply_material_preset(m_material_presets[size_t(m_selected_material_preset)]);
                settings = solver->get_settings();
                step_now = true;
            }
        }
        ImGui::TextDisabled("%s", m_material_presets[size_t(m_selected_material_preset)].note.c_str());
    }

    const auto previous_model = settings.constitutive_model;
    const bool material_edited_before = settings_changed;
    settings_changed |= ImGui::Combo("Material", reinterpret_cast<int*>(&settings.constitutive_model), "Stomakhin 2013\0Drucker-Prager\0Cohesive Cam Clay\0");
    if (settings.constitutive_model != previous_model) {
        // plastic_state is model-specific; a model switch needs a reseed
        settings.reset_on_next_run = true;
        step_now = true;
    }
    if (settings.constitutive_model == nodes::MpmSolverNode::DRUCKER_PRAGER)
        settings_changed |= ImGui::DragFloat("Friction angle", &settings.dp_friction_angle, 0.25f, 0.0f, 89.0f, "%.1f deg");
    if (settings.constitutive_model == nodes::MpmSolverNode::COHESIVE_CAM_CLAY) {
        settings_changed |= ImGui::DragFloat("Friction M", &settings.ccc_m, 0.01f, 0.05f, 3.0f, "%.2f");
        settings_changed |= ImGui::DragFloat("Cohesion beta", &settings.ccc_beta, 0.01f, 0.0f, 2.0f, "%.2f");
        settings_changed |= ImGui::DragFloat("Hardening xi", &settings.ccc_xi, 0.01f, 0.0f, 20.0f, "%.3f");
        settings_changed |= ImGui::DragFloat("Consolidation p0", &settings.ccc_p0_initial, 100.0f, 0.0f, 200000.0f, "%.0f Pa");
    }
    if (settings_changed && !material_edited_before)
        m_selected_material_preset = 0; // hand-edited: no preset describes these values any more

    settings_changed |= ImGui::Combo("Basal friction", reinterpret_cast<int*>(&settings.basal_friction_model), "Coulomb\0Voellmy\0");
    settings_changed |= ImGui::DragFloat("Friction coefficient", &settings.terrain_friction, 0.01f, 0.0f, 2.0f, "%.2f");
    if (settings.basal_friction_model == nodes::MpmSolverNode::VOELLMY)
        settings_changed |= ImGui::DragFloat("Turbulent friction", &settings.voellmy_xi, 10.0f, 100.0f, 20000.0f, "%.0f m/s2");

    draw_entrainment(*solver, settings, settings_changed, step_now);

    // Domain changes reallocate GPU buffers, so they reseed - apply them on release only.
    ImGui::TextDisabled("Simulation domain (changes reset the run)");
    if (sandbox == nullptr) {
        settings_changed |= ImGui::DragScalarN("Domain lat/lon", ImGuiDataType_Double, &settings.domain_center.x, 2, 0.0001f, nullptr, nullptr, "%.5f");
        step_now |= ImGui::IsItemDeactivatedAfterEdit();
    }
    settings_changed |= ImGui::DragFloat("Domain size", &settings.domain_size_xy, 16.0f, 64.0f, 16384.0f, "%.0f m");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();

    const uint32_t min_res = 8, max_res = nodes::MpmSolverNode::MAX_GRID_RESOLUTION_XY, max_layers = nodes::MpmSolverNode::MAX_GRID_LAYERS;
    settings_changed |= ImGui::DragScalar("Grid resolution XY", ImGuiDataType_U32, &settings.grid_resolution_xy, 1.0f, &min_res, &max_res, "%u");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();
    settings_changed |= ImGui::DragScalar("Grid layers", ImGuiDataType_U32, &settings.grid_layers, 1.0f, &min_res, &max_layers, "%u");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Node layers stored above the terrain in each column. The grid follows the surface,\n"
                          "so this is headroom for piles and terrain steps, not the relief of the domain.");

    // The domain is clamped to the terrain the graph actually stitched, so report what the
    // solver settled on rather than what was asked for.
    const float effective_domain = solver->domain_size_metres();
    const float dx = effective_domain / float(std::max(settings.grid_resolution_xy, 1u));
    ImGui::TextDisabled("Effective: %.0f m box, %.1f m cells, %.0f m band above terrain", double(effective_domain), double(dx),
        double(dx * float(settings.grid_layers)));
    if (effective_domain + 1.0f < settings.domain_size_xy) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Clamped to the tiled region.");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The domain cannot exceed the terrain the graph loaded.\n"
                              "Lower the Select Tiles zoom level for a larger region.");
    }

    ImGui::Separator();

    // Release zone is independent of the domain: small start area, large runout box. The
    // sandbox takes it from the dataset.
    if (sandbox == nullptr) {
        ImGui::TextDisabled("Release zone (changes reset the run)");
        settings_changed |= ImGui::DragScalarN("Release lat/lon", ImGuiDataType_Double, &settings.release_center.x, 2, 0.0001f, nullptr, nullptr, "%.5f");
        step_now |= ImGui::IsItemDeactivatedAfterEdit();
        settings_changed |= ImGui::DragFloat("Release radius", &settings.release_radius, 2.0f, 1.0f, 2000.0f, "%.0f m");
        step_now |= ImGui::IsItemDeactivatedAfterEdit();

        if (ImGui::Checkbox("Seed anywhere (ignore release areas)", &settings.seed_anywhere)) {
            settings_changed = true;
            settings.reset_on_next_run = true;
            step_now = true;
        }
    }

    ImGui::TextDisabled("Full parameters: compute graph editor.");
    draw_startup_settings();

    if (settings_changed)
        solver->set_settings(settings);
    if (step_now)
        step(*solver);
}

} // namespace webgpu_app
