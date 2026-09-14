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

#include "compute/NodeGraphPanel.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <nucleus/srs.h>
#include <webgpu/compute/NodeGraph.h>
#include <webgpu/compute/nodes/GeoRegionNode.h>
#include <webgpu/compute/nodes/MpmSolverNode.h>
#include <webgpu/compute/nodes/SelectTilesNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

AvalanchePanel::AvalanchePanel(NodeGraphPanel* graph_panel)
    : m_graph_panel(graph_panel)
    , m_scenarios({
          Scenario { "Breite Ries (Schneeberg)", "Gully on Austria's easternmost 2000er, 1931 m to 1364 m.",
              glm::dvec2(47.77663, 15.81600), 8000.0f, 15, glm::dvec2(47.77663, 15.81600), 4000.0f, 320, glm::dvec2(47.77480, 15.81050), 100.0f, 1.5f },
          Scenario { "Grossglockner (Pasterze side)", "Austria's highest summit, 3798 m, above the Pasterze glacier.",
              glm::dvec2(47.07900, 12.70200), 8000.0f, 15, glm::dvec2(47.07900, 12.70200), 4000.0f, 320, glm::dvec2(47.07500, 12.69600), 150.0f, 2.0f },
          Scenario { "Dachstein (Hallstatt Glacier)", "North side of the Dachstein plateau above the glacier.",
              glm::dvec2(47.47800, 13.60600), 8000.0f, 15, glm::dvec2(47.47800, 13.60600), 4000.0f, 320, glm::dvec2(47.47300, 13.60400), 130.0f, 2.0f },
      })
    , m_material_presets({
          // name, note, model, E, nu, rho, mu, | Stomakhin xi, theta_c, theta_s | DP phi | CCC M, beta, xi, p0
          // Entry 0 is the "custom" placeholder and is never applied.
          MaterialPreset { "(custom)", "Parameters edited by hand.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
          MaterialPreset { "Stomakhin 2013 (film snow)", "Disney's parameters, tuned for metre-scale visuals, not measured snow.",
              nodes::MpmSolverNode::STOMAKHIN_2013, 1.4e5f, 0.2f, 400.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 0.7f, 0.2f, 0.002f, 3000.0f },
          MaterialPreset { "Cold dense (Li 2021 I)", "Fluid-like, fully sheared, low flow height, surges at the front.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 0.5f, 0.0f, 1.0f, 3000.0f },
          MaterialPreset { "Warm shear (Li 2021 II)", "Granulation, fluctuating surface, piles above initial height.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 1.5f, 0.3f, 1.0f, 30000.0f },
          MaterialPreset { "Sliding slab (Li 2021 III)", "Brittle; breaks into blocks shortly after release.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 1.5f, 0.5f, 1.0f, 42000.0f },
          MaterialPreset { "Warm plug (Li 2021 IV)", "Ductile block sheared only at the base. Lowest M, highest beta - not a typo.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 0.5f, 1.0f, 0.1f, 12000.0f },
          MaterialPreset { "Vallee de la Sionne 2003 (Li 2021 V)", "Back-calculated from the real avalanche of 7 Feb 2003. New snow.",
              nodes::MpmSolverNode::COHESIVE_CAM_CLAY, 3.0e6f, 0.3f, 200.0f, 0.49f, 10.0f, 2.5e-2f, 7.5e-3f, 30.0f, 0.7f, 0.2f, 0.002f, 3000.0f },
          MaterialPreset { "Cold dense, cohesionless (Drucker-Prager)", "Fallback for the cold-dense regime: friction cone, no cohesion. phi from M = 0.5.",
              nodes::MpmSolverNode::DRUCKER_PRAGER, 3.0e6f, 0.3f, 250.0f, 0.47f, 10.0f, 2.5e-2f, 7.5e-3f, 13.3f, 0.5f, 0.0f, 1.0f, 3000.0f },
      })
{
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

    m_playing = false; // the terrain has to be fetched and stitched before stepping again
    graph->run();
    return true;
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
    // faster than Stomakhin's 0.14 MPa. Pull dt under the bound rather than let the first
    // run explode.
    const float dx = s.domain_size_xy / float(std::max(s.grid_resolution_xy, 1u));
    const float wave_speed = std::sqrt(std::max(s.youngs_modulus, 1.0f) / std::max(s.snow_density, 1.0f));
    const float cfl_dt = 0.1f * dx / std::max(wave_speed, 1e-3f);
    if (s.dt > cfl_dt)
        s.dt = 0.8f * cfl_dt;

    // The per-particle plastic state means different things per model (Jp, plastic
    // strain, alpha) - particles seeded under one model are garbage under another.
    s.reset_on_next_run = true;
    solver->set_settings(s);
}

void AvalanchePanel::draw()
{
    // Stepping lives here rather than in draw_panel() so the animation keeps running with
    // the sidebar section collapsed and the node graph editor closed.
    if (!m_playing)
        return;

    auto* solver = find_solver();
    if (solver == nullptr || !solver->has_valid_inputs()) {
        m_playing = false;
        return;
    }
    // is_running() keeps runs from piling up when the GPU falls behind the frame rate.
    if (!solver->is_running())
        solver->rerun();
}

void AvalanchePanel::draw_panel()
{
    auto* solver = find_solver();
    if (solver == nullptr)
        return; // active graph has no avalanche simulation

    if (!ImGui::CollapsingHeader(ICON_FA_SNOWFLAKE "  Avalanche", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // --- Location ---
    // Needs a GeoRegionNode in the graph; a GPX-driven graph has no coordinates to set.
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
    }
    ImGui::EndDisabled();

    if (!can_switch_location) {
        ImGui::TextDisabled("Load the MLS-MPM preset to switch location.");
    } else {
        ImGui::TextDisabled("%s", m_scenarios[size_t(m_selected_scenario)].note.c_str());
    }
    if (!m_scenario_error.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_scenario_error.c_str());

    ImGui::Separator();

    const bool ready = solver->has_valid_inputs();
    if (!ready) {
        m_playing = false;
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Inputs not ready.");
        ImGui::TextDisabled("Run the compute graph once (Shift+R).");
        return;
    }

    auto settings = solver->get_settings();
    bool settings_changed = false;
    bool step_now = false;

    ImGui::Text("Simulated time: %.2f s", double(solver->simulated_time()));

    const auto& aabb = solver->domain_aabb();
    if (aabb.size().x > 0.0) {
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
            ImGui::SetTooltip("Plastic ratio: particles whose plastic state has left its initial value.\n"
                              "Li et al. 2021 report 77 / 26 / 10 / 34 %% for their cases I-IV at the end of the run.");
    } else if (state.valid) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "0 particles seeded - move the release zone or tick Seed anywhere.");
    }

    // Energy-line test (com1DFA section 5.2): Coulomb friction removes exactly mu of energy
    // height per horizontal metre of centre-of-mass travel, so the fitted slope is the
    // effective friction the flow experiences; the excess over the set mu is internal
    // (plastic) dissipation.
    const auto& energy_line = solver->energy_line();
    const float mu_eff = solver->energy_line_friction();
    if (!energy_line.empty()) {
        if (std::isfinite(mu_eff)) {
            ImGui::TextDisabled("Energy line: mu_eff %.3f over %.0f m (basal mu %.2f, internal %+.3f)", double(mu_eff),
                double(energy_line.back().path), double(settings.terrain_friction), double(mu_eff - settings.terrain_friction));
        } else {
            ImGui::TextDisabled("Energy line: %zu samples, not moving yet", energy_line.size());
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Energy height z + v^2/2g of the centre of mass against its horizontal path.\n"
                              "Slope = -mu_eff. For pure Coulomb sliding mu_eff == mu; anything above it\n"
                              "is dissipated inside the snow. Tonnel et al. 2023, com1DFA section 5.2.");
        if (energy_line.size() >= 2) {
            std::vector<float> heights;
            heights.reserve(energy_line.size());
            for (const auto& sample : energy_line)
                heights.push_back(sample.energy_height);
            ImGui::PlotLines("##energy_line", heights.data(), int(heights.size()), 0, "energy height vs run", FLT_MAX, FLT_MAX, ImVec2(0, 40));
        }
    }

    if (ImGui::Button(m_playing ? ICON_FA_PAUSE "  Pause" : ICON_FA_PLAY "  Play", ImVec2(110, 0)))
        m_playing = !m_playing;
    ImGui::SameLine();
    if (ImGui::Button("Step", ImVec2(80, 0)))
        step_now = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(80, 0))) {
        solver->request_reset();
        settings = solver->get_settings();
        step_now = true;
    }

    ImGui::Separator();

    const uint32_t min_substeps = 1, max_substeps = 512;
    settings_changed |= ImGui::DragScalar("Substeps per run", ImGuiDataType_U32, &settings.substeps_per_run, 1.0f, &min_substeps, &max_substeps, "%u");
    settings_changed |= ImGui::DragScalar("Substeps per frame", ImGuiDataType_U32, &settings.substeps_per_submit, 1.0f, &min_substeps, &max_substeps, "%u");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The GPU has one queue, so simulation and rendering take turns. A run is submitted in\n"
                          "chunks of this many substeps, one per frame: fewer = smoother view, less simulated\n"
                          "time per second. Equal to substeps per run = the whole run in one go.");
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
        if (ImGui::Combo("Material preset", &m_selected_material_preset, preset_names.data(), int(preset_names.size()))
            && m_selected_material_preset > 0) {
            apply_material_preset(m_material_presets[size_t(m_selected_material_preset)]);
            settings = solver->get_settings();
            step_now = true;
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

    // Domain changes reallocate GPU buffers, so they reseed - apply them on release only.
    ImGui::TextDisabled("Simulation domain (changes reset the run)");
    settings_changed |= ImGui::DragScalarN("Domain lat/lon", ImGuiDataType_Double, &settings.domain_center.x, 2, 0.0001f, nullptr, nullptr, "%.5f");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();
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
    const float effective_domain = float(aabb.size().x);
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

    // Release zone is independent of the domain: small start area, large runout box.
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

    ImGui::TextDisabled("Full parameters: compute graph editor.");

    if (settings_changed)
        solver->set_settings(settings);
    if (step_now && !solver->is_running())
        solver->rerun();
}

} // namespace webgpu_app
