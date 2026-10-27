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
#include "../TimeSteppedSimulation.h"

#include <webgpu/base/Buffer.h>
#include <webgpu/base/Context.h>
#include <webgpu/base/raii/CombinedComputePipeline.h>
#include <webgpu/base/raii/TextureWithSampler.h>

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace webgpu_compute::nodes {

struct SandboxTerrain;

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
///
/// Two ways to feed it terrain. On the map, the region is in world units (Web Mercator) and
/// domain and release are placed by latitude/longitude, the release being a disc masked by
/// the release point texture. With the optional "sandbox" input connected (AvaFrameSandboxNode),
/// the region is a DEM in metres, the domain is centred on the dataset's avalanche and the
/// release texture holds the release depth per cell.
///
/// Playback: the node is a TimeSteppedSimulation, so SimulationDriver advances it every frame
/// through its clock: advance() runs (accuracy) or advance_frame() chunks (real time, with the
/// particles before each chunk kept for interpolation). A plain rerun() uses `substeps_per_run`
/// and `substeps_per_submit` from the settings.
/// What a renderer needs to draw the particles where they are, read straight from the GPU
/// buffer (layout: struct Particle in mpm_common.wgsl, PARTICLE_STRIDE_U32 words each). Owned
/// by the solver and kept current across reallocations, so a renderer can hold the pointer for
/// the solver's lifetime and read it every frame.
struct MpmParticleView {
    const webgpu::raii::RawBuffer<uint32_t>* particles = nullptr; // nullptr before the first run
    uint32_t particle_count = 0; // allocated; inactive ones have mass 0
    /* Real-time playback with interpolation (SimulationClock): the particles as they were before
     * the last chunk (same layout), and the share of the way from them to `particles` that a
     * frame should show. nullptr when the last chunk did not keep them (after a reset, in
     * accuracy playback): then show `particles`. */
    const webgpu::raii::RawBuffer<uint32_t>* previous_particles = nullptr;
    float interpolation = 1.0f;
    /* Solver metres (particle.position.xy) -> weBIGeo world xy (Web Mercator), as
     * world = M * (x, y, 1). Map: region offset and 1/cos(latitude). Sandbox: the affine fit of
     * SandboxTerrain::sandbox_to_world. Heights are real metres; world z = altitude / cos(latitude). */
    glm::dmat3 world_from_metres = glm::dmat3(1.0);
    bool placed = false; // false for a synthetic sandbox dataset, which has no place on the map
    float particle_spacing = 1.0f; // [m] cube root of the release volume per particle
    /* The solver's grid columns (nodes at domain_origin + index * dx, in metres), for renderers
     * that aggregate the particles per column, like the flow surface. */
    glm::vec2 domain_origin = glm::vec2(0.0f);
    float dx = 1.0f;
    glm::uvec2 columns = glm::uvec2(0u);
    /* struct ColumnStats of mpm_common.wgsl per column (y * columns.x + x), six words; words 4 and 5
     * are the entrainment cover still on the column's cell and the eroded rest [m^3 of cover]. */
    const webgpu::raii::RawBuffer<uint32_t>* column_stats = nullptr;
    /* The terrain the solver runs on (R32Float, altitudes in metres, uv = metres / region_size
     * with v pointing south), so a renderer can drape things over the same ground. */
    const webgpu::raii::TextureWithSampler* height_texture = nullptr;
    glm::vec2 region_size = glm::vec2(1.0f); // [m]
    uint32_t reset_count = 0; // changes when the particles are released anew (Reset), so per-run memory can be cleared
};

class MpmSolverNode : public Node, public TimeSteppedSimulation {
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
        COHESIVE_CAM_CLAY = 2, // Hencky elasticity + Cam-Clay ellipse (Gaume 2018), NACC return (Wolper 2019)
    };

    /* The contact law between flowing snow and the terrain surface. Kept separate from the
     * constitutive model on purpose: internal friction (within the snow) belongs to the
     * material law, basal friction (against the ground) is a boundary condition. Values
     * must match the constants in mpm_friction.wgsl. */
    enum BasalFrictionModel : uint32_t {
        COULOMB = 0, // contact impulse, mu * normal impact speed
        VOELLMY = 1, // Coulomb + turbulent drag g|v|^2/(xi h), h the local flow depth; pair with a lower mu (~0.155)
    };

    /* Entrainment: how the flow picks up the snow cover along its path (com1DFA's two modes,
     * Tonnel et al. 2023). Values must match the constants in mpm_common.wgsl. See
     * mpm-mls-doc/15-entrainment.md. */
    enum EntrainmentMode : uint32_t {
        ENTRAINMENT_OFF = 0,
        PLOWING = 1, // the front takes the whole cover
        EROSION = 2, // the flow erodes the cover at tau_b |u| / e_b per area (com1DFA's default)
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
         * dx / wave speed; the node pulls it under max_stable_dt() before every run. Several
         * substeps are run per node execution to keep the ratio of simulated time to graph
         * overhead reasonable. */
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

        /* Drucker-Prager only. Internal friction angle of the material [degrees], fixed (Klar
         * et al. 2016 harden it, section 7.3; not done here). Relates to the Cam-Clay slope M
         * via sin(phi) = 3M / (6 + M): Li's cold-dense M = 0.5 is ~13 degrees, the warm-shear
         * M = 1.5 is ~37 degrees. 30 is a typical sand value, inside the 20-40 degree range
         * Klar sweep (their hardened sand starts at 25 and rises towards 35). */
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
         * back-calculated for their verification case). com1DFA's Voellmy option uses
         * mu = 0.155 with xi = 4000, the drag term carrying the rest of the resistance (its
         * default friction model is samosAT, not Voellmy). */
        float terrain_friction = 0.47f; // Coulomb mu
        float voellmy_xi = 4000.0f; // [m/s^2], Voellmy only

        uint32_t raster_resolution = 512u; // output density raster / texture edge length

        /* Particles are points; splatting them as single texels makes the result invisible
         * at map scale. Each particle is drawn as a disc of this radius instead. */
        float splat_radius = 6.0f; // [m]

        uint32_t random_seed = 1u;

        /* Stop criterion of com1DFA: the flow has stopped once the kinetic energy has fallen
         * below this fraction of its peak since the reset - "< 1.00 percent of PKE" in the
         * AvaFrame benchmark runs. 0 = never. Reported by at_rest(); the caller decides. */
        float stop_kinetic_fraction = 0.01f;

        /* Debug aid: fill the whole domain with snow instead of only the release areas.
         * Useful to confirm the solver and the domain placement before hunting for a
         * release area to sit on. */
        bool seed_anywhere = false;

        /* Entrainment. The snow cover is a field per grid column, set at the reset; the grid
         * update erodes it where the flow passes and turns the eroded snow into particles from a
         * pool after the release particles (entrain_column() in mpm_grid_update.wgsl). The cover
         * comes from AvaFrame's entrainment areas where given (sandbox, AvaFrame scenarios on the
         * map), else from cover_thickness. Switching it on or off, and changes to the cover, take
         * effect with the next reset. Defaults are com1DFA's (com1DFACfg.ini, 2.1). */
        EntrainmentMode entrainment_mode = ENTRAINMENT_OFF;
        float entrainment_density = 100.0f; // [kg/m^3] of the cover (rhoEnt)
        float erosion_energy = 5000.0f; // e_b [J/kg] (entEroEnergy), EROSION
        float plowing_depth = 0.1f; // [m] flow depth over a column that takes its cover, PLOWING
        float entrainment_min_speed = 1.0f; // [m/s] slower flow picks nothing up
        /* The cover where no entrainment areas are given: one slope-normal thickness on every
         * slope up to cover_max_slope, outside the release. 0 = no cover there. */
        float cover_thickness = 0.3f; // [m] (entThIfMissingInShp)
        float cover_max_slope = 50.0f; // [deg] steeper slopes hold no snow (Kyburz et al. 2024 cut at 50)
        /* Pool for the entrained snow, in particles per release particle. When it runs out the
         * rest of the cover stays on the ground (pool_exhausted_volume in the readback). */
        float pool_size = 2.0f;

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

        uint32_t release_mode; // 0 = disc masked by release points, 1 = depth per cell (sandbox)
        float release_volume; // [m^3], release_mode 1
        float release_max_depth; // [m], release_mode 1
        float release_half_y; // half height of the release bounds [m], release_mode 1 (release_radius = half width)

        uint32_t entrainment_mode;
        uint32_t cover_mode; // 0 none, 1 texture, 2 uniform thickness
        uint32_t release_particles; // particles [0, release_particles) are the release, the rest the entrainment pool
        uint32_t pool_particles;

        float cover_thickness; // uniform: slope-normal [m]
        float cover_cos_max_slope;
        float cover_density_ratio; // cover density / snow density
        float erosion_energy;

        float plowing_depth;
        float entrainment_min_speed;
        float snow_density;
        float entrainment_density;
    };
    static_assert(sizeof(MpmSolverSettingsUniform) == 240, "uniform layout must match the WGSL struct");

public:
    MpmSolverNode(webgpu::Context& ctx);
    MpmSolverNode(webgpu::Context& ctx, const MpmSolverSettings& settings);
    ~MpmSolverNode() override;

    void set_settings(const MpmSolverSettings& settings) { m_settings = settings; }
    const MpmSolverSettings& get_settings() const { return m_settings; }
    MpmSolverSettings& settings() { return m_settings; }

    /// Re-scan the terrain and seed a fresh snow slab on the next run.
    void request_reset() { m_settings.reset_on_next_run = true; }

    /// Largest time step the explicit solver resolves at cell size dx: a CFL bound on the
    /// P-wave speed of the base stiffness. Runs pull dt below it (to 80 %) automatically.
    static float max_stable_dt(const MpmSolverSettings& settings, float dx);

    /// Simulated time [s] accumulated since the last reset.
    float simulated_time() const override { return m_simulated_time; }
    /// Wall-clock seconds since the last reset, pauses included.
    float wall_time_since_reset() const { return std::chrono::duration<float>(std::chrono::steady_clock::now() - m_reset_wall_start).count(); }

    /// World space bounds of the simulated box. Only meaningful after the first run.
    const radix::geometry::Aabb<2, double>& domain_aabb() const { return m_domain_aabb; }

    /// Edge length of the simulated box in metres, after clamping to the region. The solver
    /// works in metres; world units are Web Mercator (see update_gpu_settings).
    float domain_size_metres() const { return m_settings_uniform.data.domain_size_xy; }
    /// Metres per world unit at the region, cos(latitude). 1 until the first run, and 1 in
    /// the sandbox, whose region is already in metres.
    double metres_per_world_unit() const { return m_metres_per_world_unit; }
    /// The particles for a renderer, see MpmParticleView. The pointer stays valid for the
    /// solver's lifetime.
    const MpmParticleView* particle_view() const { return &m_particle_view; }

    /// The sandbox the solver runs on, or nullptr when it runs on the map (the optional
    /// "sandbox" input is not connected or its node has not run).
    const SandboxTerrain* sandbox();
    /// The release areas given per cell on the map (the optional "release area" input, from
    /// AvaFrameReleaseNode), or nullptr: then the map releases the disc. Ignored in the sandbox.
    const ReleaseArea* release_area();

    /// Diagnostics read back from the GPU after each run. Arrives asynchronously, so it
    /// describes the *previous* completed run; `valid` is false until the first readback.
    struct SimStateReadback {
        bool valid = false;
        float min_altitude = 0.0f; // terrain scan in the domain [m]
        float max_altitude = 0.0f;
        uint32_t active_particles = 0; // seeded successfully
        float max_speed = 0.0f; // fastest particle in the last run [m/s]
        uint32_t plastic_particles = 0; // yielded in the last substep (instantaneous, as in Li et al. 2021)
        glm::dvec3 centre_of_mass = glm::dvec3(0.0); // region-relative x, y; absolute z [m]
        float mean_speed_sq = 0.0f; // mean |v|^2 over active particles [m^2/s^2]
        float mean_path = 0.0f; // horizontal path per particle since the reset, mass-averaged [m]
        /* Particle-substeps in the last run in contact with an artificial boundary (the
         * stencil reaches the nodes where the grid stops outward flow): the ceiling of the
         * terrain-following band, and the walls of the domain box. Anything above zero means
         * the result depends on the box size, not only on the physics. */
        uint32_t ceiling_contacts = 0;
        uint32_t wall_contacts = 0;
        uint32_t run_substeps = 0; // substeps of the run these counters describe
        float release_area_share = 0.0f; // share of the release disc that holds snow (seeding estimate)

        /* Entrainment, from the reset on. Every particle has the same mass, so the entrained
         * mass is a count times the mass of a particle (its real volume times the snow density). */
        double cover_volume = 0.0; // [m^3] of cover in the domain at the reset
        uint32_t entrained_particles = 0; // pool particles the flow has taken up
        double pool_exhausted_volume = 0.0; // [m^3] of cover the flow could not take: the pool was full
        double particle_volume = 0.0; // [m^3] real snow volume of one particle
        double cover_mass = 0.0; // [kg]
        double entrained_mass = 0.0; // [kg]
        double entrained_volume = 0.0; // [m^3] as cover, at the cover's density - com1DFA's "entrained volume"
    };
    const SimStateReadback& last_state() const { return m_last_state; }

    /// True once the mean kinetic energy has fallen below stop_kinetic_fraction of its peak
    /// since the reset (com1DFA's stop criterion). Like last_state(), one run behind.
    bool at_rest() const override { return m_at_rest; }

    /// Box contacts (see SimStateReadback) summed over every run since the reset, and the
    /// particle-substeps they are a share of. Non-zero walls mean the runout depends on the box.
    struct BoundaryContacts {
        uint64_t ceiling = 0;
        uint64_t wall = 0;
        uint64_t particle_substeps = 0;
    };
    const BoundaryContacts& contacts_since_reset() const { return m_contacts_since_reset; }

    /// Peak flow depth and depth-averaged speed per grid column since the last reset, read
    /// back from the GPU (column_peaks in mpm_common.wgsl). Column (i, j) is the grid node at
    /// domain origin + (i, j) dx; j grows northwards.
    struct ColumnPeaks {
        glm::uvec2 size = glm::uvec2(0);
        glm::dvec2 domain_origin = glm::dvec2(0.0); // region-relative metres (sandbox metres in the sandbox)
        float dx = 0.0f;
        float simulated_time = 0.0f;
        std::vector<float> depth; // [m], vertical: snow volume per horizontal area
        std::vector<float> speed; // [m/s]
    };
    /// Copies the peaks to a staging buffer and maps it; the callback fires from the event pump
    /// a frame or so later, and not at all if the node is destroyed first.
    void read_back_peaks(std::function<void(const ColumnPeaks&)> callback);

    /// One point of the energy-line record (Tonnel et al. 2023, com1DFA section 5.2 and
    /// App. A): mass-averaged energy height z + v^2/(2g) against the mass-averaged horizontal
    /// path. Coulomb friction removes exactly mu of energy height per horizontal metre, so
    /// the slope of this record is -mu_eff, and under the Coulomb basal law mu_eff - mu is
    /// the internal dissipation (under Voellmy the drag adds to it).
    struct EnergySample {
        float time; // simulated [s]
        float path; // mean horizontal distance travelled per particle [m]
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

    /* TimeSteppedSimulation. advance() leaves substeps_per_run / substeps_per_submit alone:
     * they stay what a plain rerun() (the node editor's Step) and Manual pacing use. */
    bool can_advance() const override;
    bool is_advancing() const override { return is_running(); }
    void advance(uint32_t steps, uint32_t chunks) override;
    bool advance_frame(uint32_t steps, bool end_run) override;
    void set_interpolation(float alpha) override { m_particle_view.interpolation = alpha; }
    float time_step() const override { return m_settings.dt; }
    float gpu_ms_per_step() const override { return m_perf.gpu_ms_per_substep; }
    void reset() override { request_reset(); }

    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;
    /// Encodes and submits one chunk of `substeps` of the current run; the last one also splats,
    /// rasterises and copies the diagnostics for the readback.
    void submit_chunk(uint32_t substeps, bool last_chunk);
    /// The next chunk of a chained run (its share of the run's substeps); chains itself through
    /// the queue's work-done callback until they are exhausted.
    void submit_chained_chunk();
    /// Closes a real-time run after its last chunk: readback and completion.
    void close_frame_run();

private:
    /// (Re)allocates GPU buffers and the output texture if the configured sizes changed.
    /// Returns true if anything was recreated, which also invalidates the bind group.
    bool ensure_resources();
    /// Builds the bind group unless the cached one still refers to the current resources.
    void ensure_bind_group(const webgpu::raii::TextureWithSampler& height_texture, const webgpu::raii::TextureWithSampler& release_texture);
    /// The real snow volume one particle stands for [m^3], from the last readback (0 before).
    double particle_volume_from(const std::vector<uint32_t>& state) const;
    void create_pipelines(WGPUDevice device, const webgpu::RenderResourceRegistry& reg);
    void update_gpu_settings(const radix::geometry::Aabb<2, double>& region_aabb, const webgpu::raii::TextureWithSampler& height_texture,
        const SandboxTerrain* sandbox, const ReleaseArea* release_area);

    /// Where the domain lies in the region, in metres.
    struct DomainPlacement {
        double metres_per_world_unit = 1.0;
        glm::fvec2 region_size = glm::fvec2(0.0f); // [m]
        glm::fvec2 origin = glm::fvec2(0.0f); // region-relative [m]
        float size = 0.0f; // [m]
    };
    DomainPlacement place_domain(const radix::geometry::Aabb<2, double>& region_aabb, const SandboxTerrain* sandbox) const;

    /// The snow cover for the next reset: where it comes from and the size of the pool the
    /// entrained snow is drawn from (m_cover). Called before the buffers are sized.
    void plan_cover(const SandboxTerrain* sandbox, const ReleaseArea* release_area);
    struct CoverPlan {
        uint32_t mode = 0; // as cover_mode in the uniform
        uint32_t slot_count = 0; // pool particles
        const webgpu::raii::TextureWithSampler* texture = nullptr; // COVER_TEXTURE
    };
    void write_initial_state();
    /// Zeroes the per-run counters (max speed, plastic particles) without touching the
    /// terrain scan or the seed count.
    void reset_run_counters();
    /// Completes the run and maps the staging copy of SimState that its last chunk made;
    /// m_last_state is filled in when the map resolves.
    void read_back_state();
    void map_state_readback();
    void on_state_mapped(float time, float gravity, uint32_t substeps, bool current);
    void finish_run();
    void append_energy_sample(float time, float gravity);
    /// Work-done callback of one chunk: chains the next one, or finishes the run.
    void on_chunk_done(int timestamp_slot, uint32_t substeps, uint64_t run_serial);
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
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_column_peak_buffer; // ColumnStats per (x, y) column
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
    std::unique_ptr<webgpu::raii::BindGroup> m_bind_group;

    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_prepare_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_seed_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_p2g_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_grid_update_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_g2p_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_column_stats_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_refresh_stress_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_splat_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_rasterize_pipeline;

    /* Bind group cache key: rebuilt when our buffers or the upstream textures change. A full
     * graph run gets a new run id, which covers upstream nodes recreating their textures. */
    uint64_t m_resource_generation = 0;
    uint64_t m_bind_group_generation = ~uint64_t(0);
    uint64_t m_bind_group_run_id = ~uint64_t(0);
    WGPUTexture m_bind_group_height = nullptr;
    WGPUTexture m_bind_group_release = nullptr;
    WGPUTexture m_bind_group_cover = nullptr;

    CoverPlan m_cover; // the cover of the current seed
    /* Bound in place of the cover texture when there is none (a binding cannot be left out). */
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_no_cover_texture;


    /* Async callbacks (queue work done, buffer map) carry a copy of this instead of `this`.
     * The destructor clears the pointee, so a callback that fires after the node is gone -
     * e.g. the graph was replaced while a run was in flight - finds nullptr and returns. */
    std::shared_ptr<MpmSolverNode*> m_alive;

    /* Sizes the currently allocated resources were created for. */
    uint32_t m_allocated_particles = 0; // release particles plus the slots of the snow cover
    uint32_t m_release_particles = 0;
    glm::uvec3 m_allocated_grid_res = glm::uvec3(0);
    uint32_t m_allocated_raster_resolution = 0;

    radix::geometry::Aabb<2, double> m_domain_aabb;
    double m_metres_per_world_unit = 1.0;
    MpmParticleView m_particle_view;
    glm::uvec2 m_output_dimensions = glm::uvec2(0);
    float m_simulated_time = 0.0f;

    /* State of the run in flight, see submit_chunk(). */
    uint32_t m_run_substeps_left = 0;
    uint32_t m_run_substeps = 0; // total of the current chained run
    uint32_t m_run_substeps_submitted = 0; // of the current run so far
    uint32_t m_run_chunks = 1; // the substeps are spread evenly over this many chunks
    uint32_t m_run_chunk_index = 0; // next chunk to submit
    bool m_run_per_frame = false; // a real-time run: chunks from advance_frame(), not chained
    bool m_frame_run_open = false; // a real-time run takes more chunks
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_previous_particles; // before the last real-time chunk
    uint64_t m_run_serial = 0; // tells a chunk's callback whether its run is still the current one
    uint32_t m_gpu_chunks_in_flight = 0; // of every run, for the PerFrame throttle
    static constexpr uint32_t MAX_FRAME_CHUNKS_IN_FLIGHT = 3u;
    /* Set by advance() for the next run only; 0 = from the settings. */
    uint32_t m_advance_substeps = 0;
    uint32_t m_advance_chunks = 0;
    bool m_advance_frame = false; // advance_frame(): a real-time run of one chunk so far
    bool m_advance_end_run = false;
    uint32_t m_chunks_in_flight = 0;
    bool m_run_first_chunk = true;
    bool m_run_is_reset = false;
    bool m_run_refresh_stress = false; // material edited without a reseed, see mpm_refresh_stress.wgsl
    bool m_run_column_stats = false; // sum particle speeds per column (peak velocity), sandbox only
    /* The uniform fields the cached particle stress depends on, as of the last run. */
    std::array<float, 11> m_stress_parameters {};
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
    std::chrono::steady_clock::time_point m_reset_wall_start = std::chrono::steady_clock::now();
    PerfStats m_perf;
    SimStateReadback m_last_state;
    float m_peak_mean_speed_sq = 0.0f; // since the reset, for the stop criterion
    BoundaryContacts m_contacts_since_reset;
    bool m_at_rest = false;
    std::vector<EnergySample> m_energy_line;
};

} // namespace webgpu_compute::nodes
