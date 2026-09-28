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

#include <webgpu/base/Buffer.h>
#include <webgpu/base/Context.h>
#include <webgpu/base/raii/CombinedComputePipeline.h>
#include <webgpu/base/raii/TextureWithSampler.h>

#include <array>
#include <chrono>
#include <memory>
#include <vector>

namespace webgpu_compute::nodes {

/// Real-time snow avalanche simulation using MLS-MPM (Hu et al. 2018) with the snow
/// constitutive model of Stomakhin et al. 2013.
///
/// The node owns the whole solver state on the GPU (particles, background grid) and
/// advances it by `substeps_per_run` MPM steps per run. Each step is three dispatches:
///
///     P2G (flags active tiles) -> grid update (active tiles only, clears the accumulators)
///     -> G2P + advection (also computes the stress for the next P2G)
///
/// Re-running the node continues the simulation from its current state, which is what
/// makes an animation possible without re-seeding; set `reset_on_next_run` (or call
/// request_reset()) to scan the terrain and seed a fresh snow slab instead.
///
/// The result is published both as a top-down density raster (storage buffer, for export
/// or the generic BufferToTextureNode) and as a ready-to-display RGBA texture together
/// with the AABB of the simulated domain, so it can be wired straight into an overlay.
class MpmSolverNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(MpmSolverNode)

    // TODO currently hardcoded in the shaders - could become pipeline overrides
    static glm::uvec3 PARTICLE_WORKGROUP_SIZE;
    static glm::uvec3 GRID_WORKGROUP_SIZE; // one workgroup per tile of columns, TILE_SIZE in mpm_common.wgsl
    static glm::uvec3 RASTER_WORKGROUP_SIZE;

    static const uint32_t MAX_PARTICLES;
    static const uint32_t MAX_GRID_RESOLUTION_XY;
    static const uint32_t MAX_GRID_LAYERS;

    /* The material law relating stress to elastic deformation - what makes the particles
     * behave like snow rather than water or sand. Values must match the constants in
     * mpm_material.wgsl. Adding a model: see the comment at the top of that file. */
    enum ConstitutiveModel : uint32_t {
        STOMAKHIN_2013 = 0, // fixed corotated + singular value clamp + exponential hardening
        DRUCKER_PRAGER = 1, // Hencky elasticity + friction cone (Klar 2016); cohesionless
        COHESIVE_CAM_CLAY = 2, // Hencky elasticity + Cam-Clay ellipse (Gaume 2018); the snow-science model
    };

    /* The contact law between flowing snow and the terrain surface. Kept separate from the
     * constitutive model on purpose: internal friction (within the snow) belongs to the
     * material law, basal friction (against the ground) is a boundary condition. Values
     * must match the constants in mpm_friction.wgsl. */
    enum BasalFrictionModel : uint32_t {
        COULOMB = 0, // contact impulse, mu * normal impact speed
        VOELLMY = 1, // Coulomb + turbulent drag g|v|^2/(xi h); pair with a lower mu (~0.155)
    };

    struct MpmSolverSettings {
        ConstitutiveModel constitutive_model = STOMAKHIN_2013;
        BasalFrictionModel basal_friction_model = COULOMB;

        /* Simulation domain. The solver works on a box that is normally much smaller than
         * the region the terrain nodes prepared. Anchored geographically rather than as a
         * fraction of the region, so a scenario stays put when the region changes size or
         * snaps to different tile boundaries. */
        glm::dvec2 domain_center = glm::dvec2(47.77663, 15.81600); // latitude, longitude
        float domain_size_xy = 1024.0f; // horizontal edge length [m]

        uint32_t grid_resolution_xy = 64u; // grid nodes along x and y

        /* The grid follows the terrain: only this many layers of nodes are stored per
         * (x, y) column, starting two cells below the surface (grid_slot() in
         * mpm_common.wgsl). Snow lives in the bottom few; the rest is headroom for piles
         * and for the terrain step between neighbouring columns. Memory and grid work
         * scale with resolution_xy^2 * layers, so the domain can be kilometres wide. */
        uint32_t grid_layers = 12u;

        /* Release (start) zone. Deliberately independent of the domain: a real avalanche
         * starts in a small area and runs out over a much larger one, so the seeded disc
         * is positioned in the region like the domain is, not derived from it. */
        glm::dvec2 release_center = glm::dvec2(47.77480, 15.81050); // latitude, longitude
        float release_radius = 120.0f; // [m]

        uint32_t num_particles = 65536u;
        float slab_thickness = 1.5f; // depth of the released snow slab [m]
        float snow_density = 400.0f; // [kg/m^3]

        /* An MPM step is CFL bound, so the time step has to stay small relative to
         * dx / wave speed. Several substeps are run per node execution to keep the ratio
         * of simulated time to graph overhead reasonable. */
        float dt = 0.01f; // [s]
        uint32_t substeps_per_run = 32u;

        /* WebGPU has a single queue, so a run submitted as one command buffer parks the
         * next rendered frame behind ~100 ms of compute and the view stutters. The run is
         * therefore submitted in chunks of this many substeps, one per frame (the next
         * chunk goes out from the previous one's work-done callback), so frames interleave
         * with the simulation. Smaller = smoother view, less simulated time per second. */
        uint32_t substeps_per_submit = 2u;

        /* Elastic stiffness, shared by every constitutive model - one knob, with per-model
         * recommended values coming from presets. Defaults are Stomakhin et al. 2013, table 1;
         * Li et al. 2021 use E = 3 MPa, nu = 0.3 for the Cam-Clay regimes. */
        float youngs_modulus = 1.4e5f;
        float poissons_ratio = 0.2f;

        /* Stomakhin 2013 only. */
        float hardening = 10.0f;
        float critical_compression = 2.5e-2f;
        float critical_stretch = 7.5e-3f;

        /* Drucker-Prager only. Internal friction angle of the material [degrees]. Relates to
         * the Cam-Clay slope M via sin(phi) = 3M / (6 + M): Li's cold-dense M = 0.5 is ~13
         * degrees, the warm-shear M = 1.5 is ~37 degrees. 30 is Klar's sand default. */
        float dp_friction_angle = 30.0f;

        /* Cohesive Cam Clay only (Gaume et al. 2018). Defaults are Li et al. 2021 Table 1,
         * Case V - the case back-calculated from the real Vallee de la Sionne avalanche of
         * 7 Feb 2003, which is the most defensible single default. The four flow-regime
         * cases (I-IV) become presets. */
        float ccc_m = 0.7f; // critical state line slope - internal friction
        float ccc_beta = 0.2f; // cohesion; tensile strength = beta * p0
        float ccc_xi = 0.002f; // hardening factor - brittleness
        float ccc_p0_initial = 3000.0f; // initial consolidation pressure [Pa]

        float gravity = 9.81f;
        /* Basal friction. mu = 0.47 is what Li et al. 2021 use on real terrain (0.49
         * back-calculated for their verification case); the Voellmy pairing in com1DFA is
         * mu = 0.155 with xi = 4000, the drag term carrying the rest of the resistance. */
        float terrain_friction = 0.47f; // Coulomb mu
        float voellmy_xi = 4000.0f; // [m/s^2], Voellmy only

        uint32_t raster_resolution = 512u; // output density raster / texture edge length

        /* Particles are points; splatting them as single texels makes the result invisible
         * at map scale. Each particle is drawn as a disc of this radius instead. */
        float splat_radius = 6.0f; // [m]

        uint32_t random_seed = 1u;

        /* Debug aid: fill the whole domain with snow instead of only the release areas.
         * Useful to confirm the solver and the domain placement before hunting for a
         * release area to sit on. */
        bool seed_anywhere = false;

        bool reset_on_next_run = true;
    };

private:
    /* Mirrors struct MpmSettings in mpm_common.wgsl - keep the two in sync. */
    struct MpmSolverSettingsUniform {
        glm::uvec3 grid_res;
        uint32_t num_particles;

        glm::fvec2 domain_origin;
        float domain_size_xy;
        float dx;

        glm::fvec2 region_size;
        glm::uvec2 height_texture_dim;

        float dt;
        float gravity;
        float particle_mass;
        float particle_volume;

        float mu_0;
        float lambda_0;
        float hardening;
        float critical_compression;

        float critical_stretch;
        float terrain_friction;
        float slab_thickness;
        uint32_t random_seed;

        glm::uvec2 raster_dim;
        glm::fvec2 domain_uv_min;

        glm::fvec2 domain_uv_size;
        float seed_anywhere; // 1.0 = ignore the release point mask when seeding
        float splat_radius_texels;

        float density_reference; // coverage count that maps to full opacity
        float release_centre_x; // region-relative metres
        float release_centre_y;
        float release_radius;

        uint32_t constitutive_model;
        uint32_t basal_friction_model;
        float voellmy_xi;
        float dp_alpha;

        float ccc_m;
        float ccc_beta;
        float ccc_xi;
        float ccc_p0_initial;
    };
    static_assert(sizeof(MpmSolverSettingsUniform) == 176, "uniform layout must match the WGSL struct");

public:
    MpmSolverNode(webgpu::Context& ctx);
    MpmSolverNode(webgpu::Context& ctx, const MpmSolverSettings& settings);
    ~MpmSolverNode() override;

    void set_settings(const MpmSolverSettings& settings) { m_settings = settings; }
    const MpmSolverSettings& get_settings() const { return m_settings; }
    MpmSolverSettings& settings() { return m_settings; }

    /// Re-scan the terrain and seed a fresh snow slab on the next run.
    void request_reset() { m_settings.reset_on_next_run = true; }

    /// Simulated time [s] accumulated since the last reset.
    float simulated_time() const { return m_simulated_time; }

    /// World space bounds of the simulated box. Only meaningful after the first run.
    const radix::geometry::Aabb<2, double>& domain_aabb() const { return m_domain_aabb; }

    /// Diagnostics read back from the GPU after each run. Arrives asynchronously, so it
    /// describes the *previous* completed run; `valid` is false until the first readback.
    struct SimStateReadback {
        bool valid = false;
        float min_altitude = 0.0f; // terrain scan in the domain [m]
        float max_altitude = 0.0f;
        uint32_t active_particles = 0; // seeded successfully
        float max_speed = 0.0f; // fastest particle in the last run [m/s]
        uint32_t plastic_particles = 0; // plastic state has left its initial value
        glm::dvec3 centre_of_mass = glm::dvec3(0.0); // region-relative x, y; absolute z [m]
        float mean_speed_sq = 0.0f; // mean |v|^2 over active particles [m^2/s^2]
    };
    const SimStateReadback& last_state() const { return m_last_state; }

    /// One point of the energy-line record (Tonnel et al. 2023, com1DFA section 5.2):
    /// the centre of mass's energy height z + v^2/(2g) against its horizontal path length.
    /// Coulomb friction removes exactly mu of energy height per horizontal metre, so the
    /// slope of this record is -mu_eff, and mu_eff - mu is the internal dissipation.
    struct EnergySample {
        float time; // simulated [s]
        float path; // horizontal distance travelled by the centre of mass [m]
        float altitude; // centre of mass [m]
        float energy_height; // altitude + mean_speed_sq / (2 g) [m]
    };
    /// Bounded: past MAX_ENERGY_SAMPLES every other sample is dropped, which keeps the fit
    /// (and the per-frame plot) cheap however long the simulation plays.
    const std::vector<EnergySample>& energy_line() const { return m_energy_line; }
    static constexpr size_t MAX_ENERGY_SAMPLES = 2048;

    /// Least-squares slope of energy height over path, negated: the effective friction
    /// coefficient the flow is experiencing. NaN until there are enough samples with
    /// actual movement.
    float energy_line_friction() const;

    /// Performance counters, for the panel and for benchmarking in the app.
    struct PerfStats {
        uint32_t runs = 0; // since the last reset
        float last_run_ms = 0.0f; // wall time of the last run, first submit to readback
        float mean_run_ms = 0.0f; // exponential average over runs
        uint64_t gpu_bytes = 0; // solver-owned buffers and textures
        /* GPU time per MPM substep, measured with timestamp queries on the solver's own
         * compute pass (exponential average). 0 until measured, and stays 0 on devices
         * without timestamp-query. Drives the adaptive pacing in the avalanche panel. */
        float gpu_ms_per_substep = 0.0f;
    };
    const PerfStats& perf_stats() const { return m_perf; }

    /// True when every input is both connected and actually carrying a resource.
    /// Callers driving the solver directly (e.g. rerun() from the UI) must check this:
    /// upstream nodes hand out null pointers until they have produced their outputs.
    [[nodiscard]] bool has_valid_inputs();

    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;
    /// Encodes and submits the next chunk of the current run; chains itself through the
    /// queue's work-done callback until the run's substeps are exhausted.
    void submit_chunk();

private:
    /// (Re)allocates GPU buffers and the output texture if the configured sizes changed.
    /// Returns true if anything was recreated, which also invalidates the bind group.
    bool ensure_resources();
    /// Builds the bind group unless the cached one still refers to the current resources.
    void ensure_bind_group(const webgpu::raii::TextureWithSampler& height_texture, const webgpu::raii::TextureWithSampler& release_point_texture);
    void create_pipelines(WGPUDevice device, const webgpu::RenderResourceRegistry& reg);
    void update_gpu_settings(const radix::geometry::Aabb<2, double>& region_aabb, const webgpu::raii::TextureWithSampler& height_texture);
    void write_initial_state();
    /// Zeroes the per-run counters (max speed, plastic particles) without touching the
    /// terrain scan or the seed count.
    void reset_run_counters();
    /// Completes the run and maps the staging copy of SimState that its last chunk made;
    /// m_last_state is filled in when the map resolves.
    void read_back_state();
    void map_state_readback();
    void on_state_mapped(float time, float gravity, bool current);
    void finish_run();
    void append_energy_sample(float time, float gravity);
    /// Work-done callback of one chunk: chains the next one, or finishes the run.
    void on_chunk_done(int timestamp_slot, uint32_t substeps);
    void read_back_timestamps(int slot, uint32_t substeps);

    static std::unique_ptr<webgpu::raii::TextureWithSampler> create_output_texture(WGPUDevice device, uint32_t width, uint32_t height);

private:
    webgpu::Context* m_ctx;

    MpmSolverSettings m_settings;
    webgpu::Buffer<MpmSolverSettingsUniform> m_settings_uniform;

    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_particle_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_grid_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_column_floor_buffer; // i32 per (x, y) column
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_state_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_state_readback; // MapRead staging copy of m_state_buffer
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_density_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_grid_velocity_buffer; // vec4f per node
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_tile_flag_buffer; // u32 per tile of columns
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
    std::unique_ptr<webgpu::raii::BindGroup> m_bind_group;

    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_prepare_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_seed_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_p2g_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_grid_update_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_g2p_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_splat_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_rasterize_pipeline;

    /* Bind group cache key: rebuilt when our buffers or the upstream textures change. A full
     * graph run gets a new run id, which covers upstream nodes recreating their textures. */
    uint64_t m_resource_generation = 0;
    uint64_t m_bind_group_generation = ~uint64_t(0);
    uint64_t m_bind_group_run_id = ~uint64_t(0);
    WGPUTexture m_bind_group_height = nullptr;
    WGPUTexture m_bind_group_release = nullptr;

    /* Async callbacks (queue work done, buffer map) carry a copy of this instead of `this`.
     * The destructor clears the pointee, so a callback that fires after the node is gone -
     * e.g. the graph was replaced while a run was in flight - finds nullptr and returns. */
    std::shared_ptr<MpmSolverNode*> m_alive;

    /* Sizes the currently allocated resources were created for. */
    uint32_t m_allocated_particles = 0;
    glm::uvec3 m_allocated_grid_res = glm::uvec3(0);
    uint32_t m_allocated_raster_resolution = 0;

    radix::geometry::Aabb<2, double> m_domain_aabb;
    glm::uvec2 m_output_dimensions = glm::uvec2(0);
    float m_simulated_time = 0.0f;

    /* State of the run in flight, see submit_chunk(). */
    uint32_t m_run_substeps_left = 0;
    uint32_t m_chunks_in_flight = 0;
    bool m_run_first_chunk = true;
    bool m_run_is_reset = false;
    bool m_readback_in_flight = false; // m_state_readback is being mapped - skip this run's copy
    bool m_readback_copied = false; // the last chunk of the current run copied the state
    uint32_t m_reset_count = 0; // tags readbacks, so one issued before a reset is dropped

    /* GPU timing of the chunks: two query pairs, one per chunk in flight, each resolved into
     * its own MapRead buffer. A slot whose buffer is still mapped is simply not timed. */
    static constexpr int TIMESTAMP_SLOTS = 2;
    /* ResolveQuerySet's destination offset must be a multiple of 256 (the spec's
     * QUERY_RESOLVE_BUFFER_ALIGNMENT), so each slot gets a 256 byte stride in the resolve
     * buffer even though it only holds two u64 timestamps. */
    static constexpr size_t TIMESTAMP_SLOT_STRIDE_BYTES = 256;
    WGPUQuerySet m_timestamp_queries = nullptr;
    std::unique_ptr<webgpu::raii::RawBuffer<uint64_t>> m_timestamp_resolve;
    std::array<std::unique_ptr<webgpu::raii::RawBuffer<uint64_t>>, TIMESTAMP_SLOTS> m_timestamp_readback;
    std::array<bool, TIMESTAMP_SLOTS> m_timestamp_busy {};
    uint32_t m_chunk_counter = 0;
    std::chrono::steady_clock::time_point m_run_started;
    PerfStats m_perf;
    SimStateReadback m_last_state;
    std::vector<EnergySample> m_energy_line;
    glm::dvec2 m_last_com_xy = glm::dvec2(0.0); // for the path increment between samples
};

} // namespace webgpu_compute::nodes
