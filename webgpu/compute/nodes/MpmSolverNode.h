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

namespace webgpu_compute::nodes {

/// Real-time snow avalanche simulation using MLS-MPM (Hu et al. 2018) with the snow
/// constitutive model of Stomakhin et al. 2013.
///
/// The node owns the whole solver state on the GPU (particles, background grid) and
/// advances it by `substeps_per_run` MPM steps per run. Each step is the classic
/// four-stage loop, dispatched as separate compute passes:
///
///     grid clear -> P2G -> grid update -> G2P + advection
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
    static glm::uvec3 GRID_WORKGROUP_SIZE;
    static glm::uvec3 RASTER_WORKGROUP_SIZE;

    static const uint32_t MAX_PARTICLES;
    static const uint32_t MAX_GRID_RESOLUTION;

    struct MpmSolverSettings {
        /* Simulation domain. The solver works on a box that is normally much smaller than
         * the region the terrain nodes prepared - `domain_center` positions it inside that
         * region in normalized coordinates. */
        glm::fvec2 domain_center = glm::fvec2(0.5f, 0.5f);
        float domain_size_xy = 1024.0f; // horizontal edge length [m]

        uint32_t grid_resolution_xy = 64u; // grid nodes along x and y
        uint32_t grid_resolution_z = 64u; // grid nodes along the vertical axis

        /* Release (start) zone. Deliberately independent of the domain: a real avalanche
         * starts in a small area and runs out over a much larger one, so the seeded disc
         * is positioned in the region like the domain is, not derived from it. */
        glm::fvec2 release_center = glm::fvec2(0.5f, 0.5f); // normalized position in the region
        float release_radius = 120.0f; // [m]

        uint32_t num_particles = 65536u;
        float slab_thickness = 1.5f; // depth of the released snow slab [m]
        float snow_density = 400.0f; // [kg/m^3]

        /* An MPM step is CFL bound, so the time step has to stay small relative to
         * dx / wave speed. Several substeps are run per node execution to keep the ratio
         * of simulated time to graph overhead reasonable. */
        float dt = 0.01f; // [s]
        uint32_t substeps_per_run = 32u;

        /* Snow parameters from Stomakhin et al. 2013, table 1. */
        float youngs_modulus = 1.4e5f;
        float poissons_ratio = 0.2f;
        float hardening = 10.0f;
        float critical_compression = 2.5e-2f;
        float critical_stretch = 7.5e-3f;

        float gravity = 9.81f;
        float terrain_friction = 0.4f; // Coulomb friction against the terrain

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
    };
    static_assert(sizeof(MpmSolverSettingsUniform) == 144, "uniform layout must match the WGSL struct");

public:
    MpmSolverNode(webgpu::Context& ctx);
    MpmSolverNode(webgpu::Context& ctx, const MpmSolverSettings& settings);

    void set_settings(const MpmSolverSettings& settings) { m_settings = settings; }
    const MpmSolverSettings& get_settings() const { return m_settings; }
    MpmSolverSettings& settings() { return m_settings; }

    /// Re-scan the terrain and seed a fresh snow slab on the next run.
    void request_reset() { m_settings.reset_on_next_run = true; }

    /// Simulated time [s] accumulated since the last reset.
    float simulated_time() const { return m_simulated_time; }

    /// World space bounds of the simulated box. Only meaningful after the first run.
    const radix::geometry::Aabb<2, double>& domain_aabb() const { return m_domain_aabb; }

    /// True when every input is both connected and actually carrying a resource.
    /// Callers driving the solver directly (e.g. rerun() from the UI) must check this:
    /// upstream nodes hand out null pointers until they have produced their outputs.
    [[nodiscard]] bool has_valid_inputs();

    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;

private:
    /// (Re)allocates GPU buffers and the output texture if the configured sizes changed.
    /// Returns true if anything was recreated, which also invalidates the bind group.
    bool ensure_resources();
    void create_bind_group(const webgpu::raii::TextureWithSampler& height_texture, const webgpu::raii::TextureWithSampler& release_point_texture);
    void update_gpu_settings(const radix::geometry::Aabb<2, double>& region_aabb, const webgpu::raii::TextureWithSampler& height_texture);
    void write_initial_state();

    static std::unique_ptr<webgpu::raii::TextureWithSampler> create_output_texture(WGPUDevice device, uint32_t width, uint32_t height);

private:
    webgpu::Context* m_ctx;

    MpmSolverSettings m_settings;
    webgpu::Buffer<MpmSolverSettingsUniform> m_settings_uniform;

    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_particle_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_grid_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_state_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_density_buffer;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
    std::unique_ptr<webgpu::raii::BindGroup> m_bind_group;

    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_prepare_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_seed_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_clear_grid_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_p2g_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_grid_update_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_g2p_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_splat_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_rasterize_pipeline;

    /* Sizes the currently allocated resources were created for. */
    uint32_t m_allocated_particles = 0;
    glm::uvec3 m_allocated_grid_res = glm::uvec3(0);
    uint32_t m_allocated_raster_resolution = 0;

    radix::geometry::Aabb<2, double> m_domain_aabb;
    glm::uvec2 m_output_dimensions = glm::uvec2(0);
    float m_simulated_time = 0.0f;
};

} // namespace webgpu_compute::nodes
