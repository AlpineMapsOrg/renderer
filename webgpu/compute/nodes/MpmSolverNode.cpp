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

#include "MpmSolverNode.h"

#include <QDebug>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <nucleus/srs.h>
#include <unordered_map>

namespace webgpu_compute::nodes {

glm::uvec3 MpmSolverNode::PARTICLE_WORKGROUP_SIZE = { 256, 1, 1 };
glm::uvec3 MpmSolverNode::GRID_WORKGROUP_SIZE = { 8, 8, 1 };
glm::uvec3 MpmSolverNode::RASTER_WORKGROUP_SIZE = { 16, 16, 1 };

const uint32_t MpmSolverNode::MAX_PARTICLES = 1u << 21;
const uint32_t MpmSolverNode::MAX_GRID_RESOLUTION_XY = 512u;
const uint32_t MpmSolverNode::MAX_GRID_LAYERS = 64u;

namespace {

    /* Size of struct Particle in mpm_common.wgsl, in units of uint32. */
    constexpr uint32_t PARTICLE_STRIDE_U32 = 32u; // 128 bytes
    /* Size of struct GridNode in mpm_common.wgsl, in units of uint32. */
    constexpr uint32_t GRID_NODE_STRIDE_U32 = 5u; // 20 bytes
    /* grid_velocity in mpm_common.wgsl: one vec4f per node. */
    constexpr uint32_t GRID_VELOCITY_STRIDE_U32 = 4u; // 16 bytes
    /* Size of struct SimState in mpm_common.wgsl, in units of uint32. */
    constexpr uint32_t SIM_STATE_SIZE_U32 = 14u; // 56 bytes
    /* Mirrors the constants in mpm_common.wgsl. */
    constexpr double SUM_POSITION_SCALE = 1.0e4;
    constexpr double SUM_SPEED_SQ_SCALE = 1.0e5;

    /* Reassembles a lo/hi u32 pair written by the shader's carry-detecting atomic adds. */
    uint64_t wide(const std::vector<uint32_t>& data, size_t lo_slot) { return (uint64_t(data[lo_slot + 1]) << 32) | uint64_t(data[lo_slot]); }

    constexpr uint32_t div_ceil(uint32_t value, uint32_t divisor) { return (value + divisor - 1u) / divisor; }

    WGPUBindGroupLayoutEntry buffer_entry(uint32_t binding, WGPUBufferBindingType type)
    {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = binding;
        entry.visibility = WGPUShaderStage_Compute;
        entry.buffer.type = type;
        return entry;
    }

    /* The resource registry keeps every pipeline factory it is given, forever, and calls all
     * of them again on a shader reload (F5). A node that registered `[this]` would leave a
     * dangling factory behind each time a graph is replaced - which the avalanche autostart
     * does on every launch. So the solver registers one factory per registry, which recreates
     * the pipelines of whichever solver instances are alive at that moment. */
    struct PipelineRegistration {
        bool registered = false;
        bool device_ready = false; // the registry has called us at least once, i.e. it has a device
        std::vector<MpmSolverNode*> instances;
    };
    std::unordered_map<const webgpu::RenderResourceRegistry*, PipelineRegistration>& pipeline_registrations()
    {
        static std::unordered_map<const webgpu::RenderResourceRegistry*, PipelineRegistration> registrations;
        return registrations;
    }

    WGPUBindGroupLayoutEntry texture_entry(uint32_t binding, WGPUTextureSampleType sample_type)
    {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = binding;
        entry.visibility = WGPUShaderStage_Compute;
        entry.texture.sampleType = sample_type;
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        return entry;
    }

} // namespace

MpmSolverNode::MpmSolverNode(webgpu::Context& ctx)
    : MpmSolverNode(ctx, MpmSolverSettings())
{
}

MpmSolverNode::MpmSolverNode(webgpu::Context& ctx, const MpmSolverSettings& settings)
    : Node(
          {
              InputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>()),
              InputSocket(*this, "height texture", data_type<const webgpu::raii::TextureWithSampler*>()),
              InputSocket(*this, "release point texture", data_type<const webgpu::raii::TextureWithSampler*>()),
          },
          {
              OutputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
              OutputSocket(*this, "domain aabb", data_type<const radix::geometry::Aabb<2, double>*>(), [this]() { return &m_domain_aabb; }),
              OutputSocket(*this, "density buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>(), [this]() { return m_density_buffer.get(); }),
              OutputSocket(*this, "raster dimensions", data_type<glm::uvec2>(), [this]() { return m_output_dimensions; }),
              OutputSocket(*this, "particle buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>(), [this]() { return m_particle_buffer.get(); }),
          })
    , m_ctx(&ctx)
    , m_settings { settings }
    , m_settings_uniform(ctx.device(), WGPUBufferUsage(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform))
    , m_alive(std::make_shared<MpmSolverNode*>(this))
    , m_domain_aabb { glm::dvec2(0.0), glm::dvec2(0.0) }
{
    // Fixed size, so allocated exactly once: an asynchronous readback may be in flight while
    // ensure_resources() reallocates everything else.
    m_state_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(ctx.device(),
        WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc), SIM_STATE_SIZE_U32, "mpm state buffer");
    // Persistent staging buffer for the per-run diagnostics. The last chunk of a run copies
    // the state into it inside its own command buffer; RawBuffer::read_back_async() would
    // instead allocate a staging buffer and submit a separate copy on every run.
    m_state_readback = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        ctx.device(), WGPUBufferUsage(WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst), SIM_STATE_SIZE_U32, "mpm state readback");

    // GPU time per substep, for the adaptive pacing. Timestamp writes on our own compute
    // pass are core WebGPU (behind the timestamp-query feature), so this works on the web too.
    if (wgpuDeviceHasFeature(ctx.device(), WGPUFeatureName_TimestampQuery)) {
        WGPUQuerySetDescriptor query_desc {};
        query_desc.label = WGPUStringView { .data = "mpm solver timestamps", .length = WGPU_STRLEN };
        query_desc.type = WGPUQueryType_Timestamp;
        query_desc.count = 2 * TIMESTAMP_SLOTS;
        m_timestamp_queries = wgpuDeviceCreateQuerySet(ctx.device(), &query_desc);
        m_timestamp_resolve = std::make_unique<webgpu::raii::RawBuffer<uint64_t>>(ctx.device(),
            WGPUBufferUsage(WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc),
            TIMESTAMP_SLOTS * TIMESTAMP_SLOT_STRIDE_BYTES / sizeof(uint64_t), "mpm timestamp resolve");
        for (auto& buffer : m_timestamp_readback)
            buffer = std::make_unique<webgpu::raii::RawBuffer<uint64_t>>(
                ctx.device(), WGPUBufferUsage(WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst), 2, "mpm timestamp readback");
    }

    auto& reg = ctx.resource_registry();

    reg.register_shader("mpm_prepare", "webgpu_compute::mpm_prepare");
    reg.register_shader("mpm_seed", "webgpu_compute::mpm_seed");
    reg.register_shader("mpm_p2g", "webgpu_compute::mpm_p2g");
    reg.register_shader("mpm_grid_update", "webgpu_compute::mpm_grid_update");
    reg.register_shader("mpm_g2p", "webgpu_compute::mpm_g2p");
    reg.register_shader("mpm_refresh_stress", "webgpu_compute::mpm_refresh_stress");
    reg.register_shader("mpm_splat", "webgpu_compute::mpm_splat");
    reg.register_shader("mpm_rasterize", "webgpu_compute::mpm_rasterize");

    // All MPM kernels include mpm_common.wgsl and therefore declare the identical binding
    // set - one layout and one bind group serve every stage.
    reg.register_bind_group_layout("mpm_solver", [](WGPUDevice dev) {
        WGPUBindGroupLayoutEntry e7 {};
        e7.binding = 7;
        e7.visibility = WGPUShaderStage_Compute;
        e7.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
        e7.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
        e7.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

        return std::make_unique<webgpu::raii::BindGroupLayout>(dev,
            std::vector<WGPUBindGroupLayoutEntry> {
                buffer_entry(0, WGPUBufferBindingType_Uniform),
                texture_entry(1, WGPUTextureSampleType_UnfilterableFloat), // height texture is R32Float
                texture_entry(2, WGPUTextureSampleType_Float), // release points are RGBA8Unorm
                buffer_entry(3, WGPUBufferBindingType_Storage), // particles
                buffer_entry(4, WGPUBufferBindingType_Storage), // background grid
                buffer_entry(5, WGPUBufferBindingType_Storage), // simulation state
                buffer_entry(6, WGPUBufferBindingType_Storage), // density raster
                e7, // output texture
                buffer_entry(8, WGPUBufferBindingType_Storage), // column floors of the terrain-following grid
                buffer_entry(9, WGPUBufferBindingType_Storage), // grid node velocities
                buffer_entry(10, WGPUBufferBindingType_Storage), // active tile flags
            },
            "mpm solver bind group layout");
    });

    // See PipelineRegistration: one factory per registry, shared by all solver instances.
    auto& registration = pipeline_registrations()[&reg];
    registration.instances.push_back(this);
    if (!registration.registered) {
        registration.registered = true;
        const webgpu::RenderResourceRegistry* key = &reg;
        reg.register_pipeline([key](WGPUDevice device, const webgpu::RenderResourceRegistry& registry) {
            auto& entry = pipeline_registrations()[key];
            entry.device_ready = true;
            for (MpmSolverNode* instance : entry.instances)
                instance->create_pipelines(device, registry);
        });
    } else if (registration.device_ready) {
        create_pipelines(ctx.device(), reg);
    }
}

MpmSolverNode::~MpmSolverNode()
{
    *m_alive = nullptr; // in-flight callbacks now find the node gone
    if (m_timestamp_queries != nullptr)
        wgpuQuerySetRelease(m_timestamp_queries);
    auto& instances = pipeline_registrations()[&m_ctx->resource_registry()].instances;
    instances.erase(std::remove(instances.begin(), instances.end(), this), instances.end());
}

void MpmSolverNode::create_pipelines(WGPUDevice device, const webgpu::RenderResourceRegistry& reg)
{
    const std::vector<const webgpu::raii::BindGroupLayout*> layouts { &reg.bind_group_layout("mpm_solver") };
    const auto make = [&](const std::string& shader_name) {
        return std::make_unique<webgpu::raii::CombinedComputePipeline>(device, reg.shader(shader_name), layouts);
    };
    m_prepare_pipeline = make("mpm_prepare");
    m_seed_pipeline = make("mpm_seed");
    m_p2g_pipeline = make("mpm_p2g");
    m_grid_update_pipeline = make("mpm_grid_update");
    m_g2p_pipeline = make("mpm_g2p");
    m_refresh_stress_pipeline = make("mpm_refresh_stress");
    m_splat_pipeline = make("mpm_splat");
    m_rasterize_pipeline = make("mpm_rasterize");
}

bool MpmSolverNode::ensure_resources()
{
    // Every buffer is bound whole, so none may exceed the device's storage binding limit -
    // at the top of the UI ranges the grid (512^2 x 64 x 20 B = 320 MiB) would. Shrink the
    // offending dimension instead of failing validation (and losing the device on the web).
    WGPULimits limits {};
    wgpuDeviceGetLimits(m_ctx->device(), &limits);
    const uint64_t max_binding = std::max<uint64_t>(limits.maxStorageBufferBindingSize, 1u << 20);

    uint32_t num_particles = std::clamp(m_settings.num_particles, 1u, MAX_PARTICLES);
    num_particles = uint32_t(std::min<uint64_t>(num_particles, max_binding / (PARTICLE_STRIDE_U32 * 4u)));
    glm::uvec3 grid_res(std::clamp(m_settings.grid_resolution_xy, 8u, MAX_GRID_RESOLUTION_XY),
        std::clamp(m_settings.grid_resolution_xy, 8u, MAX_GRID_RESOLUTION_XY),
        std::clamp(m_settings.grid_layers, 8u, MAX_GRID_LAYERS));
    const uint64_t max_nodes = max_binding / (GRID_NODE_STRIDE_U32 * 4u);
    while (grid_res.z > 8u && uint64_t(grid_res.x) * grid_res.y * grid_res.z > max_nodes)
        grid_res.z--;
    while (uint64_t(grid_res.x) * grid_res.y * grid_res.z > max_nodes)
        grid_res.x = grid_res.y = grid_res.x - 8u;
    const bool limited = num_particles != std::clamp(m_settings.num_particles, 1u, MAX_PARTICLES)
        || grid_res.z != std::clamp(m_settings.grid_layers, 8u, MAX_GRID_LAYERS)
        || grid_res.x != std::clamp(m_settings.grid_resolution_xy, 8u, MAX_GRID_RESOLUTION_XY);
    const uint32_t raster_resolution = std::clamp(m_settings.raster_resolution, 64u, 4096u);

    if (m_particle_buffer && num_particles == m_allocated_particles && grid_res == m_allocated_grid_res
        && raster_resolution == m_allocated_raster_resolution) {
        return false;
    }
    if (limited) {
        qWarning() << "MpmSolverNode: reduced to" << num_particles << "particles and a" << grid_res.x << "x" << grid_res.y << "x" << grid_res.z
                   << "grid to stay within the device's storage buffer binding limit of" << max_binding / (1024 * 1024) << "MiB";
    }

    const auto storage_usage = WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc);

    m_particle_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), storage_usage, size_t(num_particles) * PARTICLE_STRIDE_U32, "mpm particle buffer");
    m_grid_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), storage_usage, size_t(grid_res.x) * grid_res.y * grid_res.z * GRID_NODE_STRIDE_U32, "mpm grid buffer");
    m_column_floor_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), storage_usage, size_t(grid_res.x) * grid_res.y, "mpm column floor buffer");
    m_grid_velocity_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), storage_usage, size_t(grid_res.x) * grid_res.y * grid_res.z * GRID_VELOCITY_STRIDE_U32, "mpm grid velocity buffer");
    const glm::uvec2 tiles(div_ceil(grid_res.x, GRID_WORKGROUP_SIZE.x), div_ceil(grid_res.y, GRID_WORKGROUP_SIZE.y));
    m_tile_flag_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(), storage_usage, size_t(tiles.x) * tiles.y, "mpm tile flags");
    m_density_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), storage_usage, size_t(raster_resolution) * raster_resolution, "mpm density raster");
    m_output_texture = create_output_texture(m_ctx->device(), raster_resolution, raster_resolution);

    m_allocated_particles = num_particles;
    m_allocated_grid_res = grid_res;
    m_allocated_raster_resolution = raster_resolution;
    m_output_dimensions = glm::uvec2(raster_resolution);
    m_resource_generation++;

    const auto bytes = [](const auto& buffer) { return uint64_t(buffer->size_in_byte()); };
    m_perf.gpu_bytes = bytes(m_particle_buffer) + bytes(m_grid_buffer) + bytes(m_grid_velocity_buffer) + bytes(m_tile_flag_buffer)
        + bytes(m_column_floor_buffer) + bytes(m_density_buffer) + bytes(m_state_buffer) + bytes(m_state_readback)
        + uint64_t(raster_resolution) * raster_resolution * 4u /* RGBA8 output texture */;

    // Fresh buffers hold garbage - the simulation has to be re-seeded.
    m_settings.reset_on_next_run = true;
    return true;
}

void MpmSolverNode::ensure_bind_group(const webgpu::raii::TextureWithSampler& height_texture, const webgpu::raii::TextureWithSampler& release_point_texture)
{
    if (m_bind_group && m_bind_group_generation == m_resource_generation && m_bind_group_run_id == get_run_id()
        && m_bind_group_height == height_texture.texture().handle() && m_bind_group_release == release_point_texture.texture().handle())
        return;
    m_bind_group_generation = m_resource_generation;
    m_bind_group_run_id = get_run_id();
    m_bind_group_height = height_texture.texture().handle();
    m_bind_group_release = release_point_texture.texture().handle();

    m_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
        m_ctx->resource_registry().bind_group_layout("mpm_solver"),
        std::vector<WGPUBindGroupEntry> {
            m_settings_uniform.raw_buffer().create_bind_group_entry(0),
            height_texture.texture_view().create_bind_group_entry(1),
            release_point_texture.texture_view().create_bind_group_entry(2),
            m_particle_buffer->create_bind_group_entry(3),
            m_grid_buffer->create_bind_group_entry(4),
            m_state_buffer->create_bind_group_entry(5),
            m_density_buffer->create_bind_group_entry(6),
            m_output_texture->texture_view().create_bind_group_entry(7),
            m_column_floor_buffer->create_bind_group_entry(8),
            m_grid_velocity_buffer->create_bind_group_entry(9),
            m_tile_flag_buffer->create_bind_group_entry(10),
        },
        "mpm solver bind group");
}

void MpmSolverNode::update_gpu_settings(const radix::geometry::Aabb<2, double>& region_aabb, const webgpu::raii::TextureWithSampler& height_texture)
{
    const glm::fvec2 region_size = glm::fvec2(region_aabb.size());
    // The domain cannot usefully exceed the terrain we actually have: outside the region the
    // height texture clamps to its edge texel, which would be a flat extrusion, not terrain.
    const float region_limit = std::max(std::min(region_size.x, region_size.y), 1.0f);
    const float domain_size = std::clamp(m_settings.domain_size_xy, 1.0f, region_limit);
    const float dx = domain_size / float(m_allocated_grid_res.x);

    // Place the domain inside the region and keep it fully covered by the terrain data.
    // Geographic anchors are converted here, where the region bounds are known, so tile
    // snapping cannot move the scenario relative to the ground.
    const glm::dvec2 domain_centre_world = nucleus::srs::lat_long_to_world(m_settings.domain_center);
    const glm::fvec2 requested_origin = glm::fvec2(domain_centre_world - region_aabb.min) - glm::fvec2(domain_size * 0.5f);
    const glm::fvec2 max_origin = glm::max(region_size - glm::fvec2(domain_size), glm::fvec2(0.0f));
    const glm::fvec2 domain_origin = glm::clamp(requested_origin, glm::fvec2(0.0f), max_origin);

    auto& data = m_settings_uniform.data;
    data.grid_res = m_allocated_grid_res;
    data.num_particles = m_allocated_particles;
    data.domain_origin = domain_origin;
    data.domain_size_xy = domain_size;
    data.dx = dx;
    data.region_size = region_size;
    data.height_texture_dim = glm::uvec2(height_texture.texture().width(), height_texture.texture().height());
    data.dt = m_settings.dt;
    data.gravity = m_settings.gravity;

    // Mass is normalised to 1 per particle. Scaling mass and volume by the same factor
    // leaves the MPM equations invariant, and it keeps the fixed point grid accumulators
    // in a range where an i32 cannot overflow regardless of the real snow mass involved.
    data.particle_mass = 1.0f;
    data.particle_volume = 1.0f / std::max(m_settings.snow_density, 1.0f);

    const float nu = std::clamp(m_settings.poissons_ratio, 0.0f, 0.45f);
    const float e = std::max(m_settings.youngs_modulus, 1.0f);
    data.mu_0 = e / (2.0f * (1.0f + nu));
    data.lambda_0 = e * nu / ((1.0f + nu) * (1.0f - 2.0f * nu));
    data.hardening = m_settings.hardening;
    data.critical_compression = m_settings.critical_compression;
    data.critical_stretch = m_settings.critical_stretch;
    data.terrain_friction = std::max(m_settings.terrain_friction, 0.0f);
    data.slab_thickness = std::max(m_settings.slab_thickness, 0.0f);
    data.random_seed = m_settings.random_seed;
    data.raster_dim = m_output_dimensions;
    data.domain_uv_min = domain_origin / region_size;
    data.domain_uv_size = glm::fvec2(domain_size) / region_size;
    data.seed_anywhere = m_settings.seed_anywhere ? 1.0f : 0.0f;

    // Splat radius in texels, capped so the per-particle loop stays bounded.
    const float metres_per_texel = domain_size / float(m_output_dimensions.x);
    const float radius_texels = std::clamp(std::max(m_settings.splat_radius, 0.0f) / metres_per_texel, 0.5f, 8.0f);
    data.splat_radius_texels = radius_texels;

    // Release zone, in region-relative metres. Clamped into the domain (with a margin for
    // its own radius) so a slightly misplaced scenario still seeds instead of silently
    // producing nothing.
    const glm::dvec2 release_world = nucleus::srs::lat_long_to_world(m_settings.release_center);
    data.release_radius = std::clamp(m_settings.release_radius, 1.0f, domain_size * 0.4f);
    const float margin = data.release_radius + 2.0f * dx;
    const glm::fvec2 release_centre = glm::clamp(glm::fvec2(release_world - region_aabb.min),
        domain_origin + glm::fvec2(margin),
        domain_origin + glm::fvec2(domain_size - margin));
    data.release_centre_x = release_centre.x;
    data.release_centre_y = release_centre.y;

    // Reference coverage for full opacity. Derived from the release disc rather than the
    // whole domain: the snow starts concentrated there, and basing the scale on the domain
    // would make a small avalanche in a large box saturate everywhere. The spread factor
    // keeps the runout visible as the same snow covers a larger area.
    constexpr float SPREAD_TOLERANCE = 6.0f;
    const float splat_area = 3.14159265f * radius_texels * radius_texels;
    const float release_radius_texels = std::max(data.release_radius / metres_per_texel, 1.0f);
    const float release_area = 3.14159265f * release_radius_texels * release_radius_texels;
    data.density_reference = std::max(float(m_allocated_particles) * splat_area / (SPREAD_TOLERANCE * release_area), 1.0f);

    data.constitutive_model = static_cast<uint32_t>(m_settings.constitutive_model);
    data.basal_friction_model = static_cast<uint32_t>(m_settings.basal_friction_model);
    data.voellmy_xi = std::max(m_settings.voellmy_xi, 1.0f);

    // Drucker-Prager cone slope from the friction angle (Klar et al. 2016, eq. after (27)):
    // alpha = sqrt(2/3) * 2 sin(phi) / (3 - sin(phi)).
    const float sin_phi = std::sin(glm::radians(std::clamp(m_settings.dp_friction_angle, 0.0f, 89.0f)));
    data.dp_alpha = std::sqrt(2.0f / 3.0f) * 2.0f * sin_phi / (3.0f - sin_phi);

    data.ccc_m = std::max(m_settings.ccc_m, 1e-3f);
    data.ccc_beta = std::max(m_settings.ccc_beta, 0.0f);
    data.ccc_xi = std::max(m_settings.ccc_xi, 1e-6f);
    data.ccc_p0_initial = std::max(m_settings.ccc_p0_initial, 0.0f);

    m_settings_uniform.update_gpu_data(m_ctx->queue());

    // World space bounds of the simulated area, so the result can be placed on the map.
    const glm::dvec2 domain_min = region_aabb.min + glm::dvec2(domain_origin);
    m_domain_aabb = { domain_min, domain_min + glm::dvec2(double(domain_size)) };
}

void MpmSolverNode::write_initial_state()
{
    std::array<uint32_t, SIM_STATE_SIZE_U32> initial {};
    const int32_t min_init = std::numeric_limits<int32_t>::max();
    const int32_t max_init = std::numeric_limits<int32_t>::lowest();
    std::memcpy(&initial[0], &min_init, sizeof(int32_t));
    std::memcpy(&initial[1], &max_init, sizeof(int32_t));
    initial[2] = 0u; // active particles
    initial[3] = 0u; // max speed
    // slots 4..13 (plastic count, reserved, the four 64-bit sums) start at zero

    m_state_buffer->write(m_ctx->queue(), initial.data(), initial.size(), 0);
}

void MpmSolverNode::reset_run_counters()
{
    // slots 3..13: max speed, plastic count, reserved, four 64-bit sums (lo/hi each)
    const std::array<uint32_t, 11> zeros {};
    m_state_buffer->write(m_ctx->queue(), zeros.data(), zeros.size(), 3);
}

void MpmSolverNode::read_back_state()
{
    // The run completes right away; the diagnostics arrive a frame or so later, as they always
    // did. The map is issued first: completing the run can start the next one synchronously
    // (queued runs, downstream nodes), and that one must see the staging buffer as busy.
    if (m_readback_copied)
        map_state_readback();
    finish_run();
}

void MpmSolverNode::map_state_readback()
{
    m_readback_in_flight = true;

    // Stamp the sample with the time of the run it describes; the callback fires later.
    struct Request {
        std::shared_ptr<MpmSolverNode*> alive;
        uint32_t reset_count;
        float time;
        float gravity;
    };
    auto* request = new Request { m_alive, m_reset_count, m_simulated_time, std::max(m_settings.gravity, 1e-3f) };

    const auto on_mapped = [](WGPUMapAsyncStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
        std::unique_ptr<Request> request(reinterpret_cast<Request*>(userdata));
        MpmSolverNode* self = *request->alive;
        if (self == nullptr)
            return; // node destroyed while the map was pending; its buffer went with it
        self->m_readback_in_flight = false;
        if (status != WGPUMapAsyncStatus_Success) {
            qWarning() << "MpmSolverNode: state readback failed, status" << int(status);
            return;
        }
        // A sample from before a reset would land in the fresh energy line; drop it.
        self->on_state_mapped(request->time, request->gravity, request->reset_count == self->m_reset_count);
    };

    WGPUBufferMapCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_mapped,
        .userdata1 = request,
        .userdata2 = nullptr,
    };
    wgpuBufferMapAsync(m_state_readback->handle(), WGPUMapMode_Read, 0, m_state_readback->size_in_byte(), callback_info);
}

void MpmSolverNode::on_state_mapped(float time, float gravity, bool current)
{
    std::vector<uint32_t> data(SIM_STATE_SIZE_U32);
    const auto* mapped = static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(m_state_readback->handle(), 0, m_state_readback->size_in_byte()));
    if (mapped != nullptr)
        std::memcpy(data.data(), mapped, data.size() * sizeof(uint32_t));
    wgpuBufferUnmap(m_state_readback->handle());
    if (mapped == nullptr || !current)
        return;

    const auto as_i32 = [&data](size_t slot) {
        int32_t value = 0;
        std::memcpy(&value, &data[slot], sizeof(int32_t));
        return value;
    };
    m_last_state.min_altitude = float(as_i32(0)) / 100.0f;
    m_last_state.max_altitude = float(as_i32(1)) / 100.0f;
    m_last_state.active_particles = data[2];
    m_last_state.max_speed = float(data[3]) / 1000.0f;
    m_last_state.plastic_particles = data[4];

    const double active = std::max(double(m_last_state.active_particles), 1.0);
    m_last_state.centre_of_mass = glm::dvec3(double(wide(data, 6)), double(wide(data, 8)), double(wide(data, 10))) / SUM_POSITION_SCALE / active;
    m_last_state.mean_speed_sq = float(double(wide(data, 12)) / SUM_SPEED_SQ_SCALE / active);
    m_last_state.valid = true;

    if (m_last_state.active_particles > 0)
        append_energy_sample(time, gravity);
}

void MpmSolverNode::append_energy_sample(float time, float gravity)
{
    // Energy line: path is the horizontal distance the centre of mass has covered.
    float path = 0.0f;
    if (!m_energy_line.empty()) {
        const auto& previous = m_energy_line.back();
        const glm::dvec2 delta = glm::dvec2(m_last_state.centre_of_mass) - m_last_com_xy;
        path = previous.path + float(glm::length(delta));
    }
    m_last_com_xy = glm::dvec2(m_last_state.centre_of_mass);
    m_energy_line.push_back(EnergySample { time, path, float(m_last_state.centre_of_mass.z),
        float(m_last_state.centre_of_mass.z) + m_last_state.mean_speed_sq / (2.0f * gravity) });

    // Keep it bounded: drop every other sample (never the latest). Path and time stay
    // monotonic, so the least-squares fit is unaffected apart from its sample count.
    if (m_energy_line.size() > MAX_ENERGY_SAMPLES) {
        std::vector<EnergySample> halved;
        halved.reserve(MAX_ENERGY_SAMPLES / 2 + 1);
        for (size_t i = 0; i < m_energy_line.size(); i += 2)
            halved.push_back(m_energy_line[i]);
        if ((m_energy_line.size() - 1) % 2 != 0)
            halved.push_back(m_energy_line.back());
        m_energy_line = std::move(halved);
    }
}

void MpmSolverNode::finish_run()
{
    const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - m_run_started).count();
    m_perf.last_run_ms = ms;
    m_perf.mean_run_ms = m_perf.runs == 0 ? ms : 0.9f * m_perf.mean_run_ms + 0.1f * ms;
    m_perf.runs++;
    complete_run();
}

float MpmSolverNode::energy_line_friction() const
{
    // Ordinary least squares of energy height on path. Needs movement to be meaningful:
    // a resting slab has path ~ 0 and the slope is undefined.
    if (m_energy_line.size() < 3)
        return std::numeric_limits<float>::quiet_NaN();
    // Skip the settling phase - the slab is seeded slightly above the terrain and compacts
    // before it slides, which drops energy height with almost no path and would dominate a
    // fit over few samples. Fit the sliding regime: samples past 10 % of the total path.
    const float skip = std::max(0.5f, 0.1f * m_energy_line.back().path);
    double sum_s = 0, sum_h = 0, sum_ss = 0, sum_sh = 0, n = 0;
    for (const auto& sample : m_energy_line) {
        if (sample.path < skip)
            continue;
        n += 1;
        sum_s += sample.path;
        sum_h += sample.energy_height;
        sum_ss += double(sample.path) * sample.path;
        sum_sh += double(sample.path) * sample.energy_height;
    }
    if (n < 3)
        return std::numeric_limits<float>::quiet_NaN();
    const double variance = sum_ss - sum_s * sum_s / n;
    if (variance < 1.0) // less than ~1 m^2 of spread in path: not moving yet
        return std::numeric_limits<float>::quiet_NaN();
    const double slope = (sum_sh - sum_s * sum_h / n) / variance;
    return float(-slope);
}

bool MpmSolverNode::has_valid_inputs()
{
    static constexpr const char* REQUIRED[] = { "region aabb", "height texture", "release point texture" };
    for (const char* name : REQUIRED) {
        if (!input_socket(name).is_socket_connected())
            return false;
    }
    // Connected is not the same as ready: an upstream node's output socket returns a null
    // pointer until that node has actually produced its resource. rerun() re-runs only this
    // node, so it can easily be reached before the graph has ever run end to end.
    if (std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("region aabb").get_connected_data()) == nullptr)
        return false;
    if (std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("height texture").get_connected_data()) == nullptr)
        return false;
    if (std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("release point texture").get_connected_data()) == nullptr)
        return false;
    return true;
}

void MpmSolverNode::run_impl()
{
    if (!has_valid_inputs()) {
        fail_run("MpmSolverNode inputs are not ready. Run the full graph (Shift+R) before stepping the solver.");
        return;
    }

    const auto* region_aabb = std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("region aabb").get_connected_data());
    const auto& height_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("height texture").get_connected_data());
    const auto& release_point_texture
        = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("release point texture").get_connected_data());

    m_run_started = std::chrono::steady_clock::now();
    ensure_resources();
    update_gpu_settings(*region_aabb, height_texture);
    ensure_bind_group(height_texture, release_point_texture);

    const bool reset = m_settings.reset_on_next_run;
    if (reset) {
        write_initial_state();
        m_simulated_time = 0.0f;
        m_last_state = {};
        m_energy_line.clear();
        m_perf.runs = 0;
        m_reset_count++;

        // Report where on the planet we are actually simulating - without this there is no
        // way to tell from the UI which slope the domain landed on.
        const glm::dvec2 south_west = nucleus::srs::world_to_lat_long(m_domain_aabb.min);
        const glm::dvec2 north_east = nucleus::srs::world_to_lat_long(m_domain_aabb.max);
        const glm::dvec2 centre = nucleus::srs::world_to_lat_long((m_domain_aabb.min + m_domain_aabb.max) * 0.5);
        qInfo().nospace() << "MpmSolverNode: simulating " << m_settings.domain_size_xy << " m domain at centre " << centre.x << ", " << centre.y
                          << " (lat " << south_west.x << ".." << north_east.x << ", lon " << south_west.y << ".." << north_east.y << "), "
                          << m_allocated_grid_res.x << "x" << m_allocated_grid_res.y << " grid, " << m_allocated_grid_res.z << " layers, dx "
                          << m_settings_uniform.data.dx << " m, " << m_allocated_particles << " particles";
    } else {
        // Per-run counters only; the terrain scan and seed count from the reset must survive.
        reset_run_counters();
    }

    m_run_substeps_left = std::clamp(m_settings.substeps_per_run, 1u, 4096u);
    m_run_first_chunk = true;
    m_run_is_reset = reset;
    // Particles cache the stress G2P computed with the previous parameters; after a material
    // edit that kept them, recompute it before the first P2G (a reseed computes it anyway).
    const auto& u = m_settings_uniform.data;
    const std::array<float, 11> stress_parameters { u.mu_0, u.lambda_0, u.hardening, u.critical_compression, u.critical_stretch,
        float(u.constitutive_model), u.dp_alpha, u.ccc_m, u.ccc_beta, u.ccc_xi, u.ccc_p0_initial };
    m_run_refresh_stress = !reset && stress_parameters != m_stress_parameters;
    m_stress_parameters = stress_parameters;
    m_readback_copied = false;
    m_settings.reset_on_next_run = false;
    m_chunks_in_flight = 0;
    // Two chunks in flight: the work-done callback of one arrives two frames after its
    // submission (GPU finish, then the next event pump), so a single chunk in flight would
    // leave the queue idle every other frame. With two, a frame always finds one chunk
    // ahead of its own command buffer and never more.
    submit_chunk();
    if (m_run_substeps_left > 0)
        submit_chunk();
}

void MpmSolverNode::submit_chunk()
{
    const glm::uvec3 particle_workgroups(div_ceil(m_allocated_particles, PARTICLE_WORKGROUP_SIZE.x), 1, 1);
    // One workgroup per tile of columns; each thread walks the layers of its column.
    const glm::uvec3 grid_workgroups(div_ceil(m_allocated_grid_res.x, GRID_WORKGROUP_SIZE.x), div_ceil(m_allocated_grid_res.y, GRID_WORKGROUP_SIZE.y), 1);
    const glm::uvec3 prepare_workgroups(
        div_ceil(m_allocated_grid_res.x, RASTER_WORKGROUP_SIZE.x), div_ceil(m_allocated_grid_res.y, RASTER_WORKGROUP_SIZE.y), 1);
    const glm::uvec3 raster_workgroups(
        div_ceil(m_output_dimensions.x, RASTER_WORKGROUP_SIZE.x), div_ceil(m_output_dimensions.y, RASTER_WORKGROUP_SIZE.y), 1);

    const uint32_t substeps = std::min(m_run_substeps_left, std::clamp(m_settings.substeps_per_submit, 1u, 4096u));
    m_run_substeps_left -= substeps;
    const bool last_chunk = m_run_substeps_left == 0;

    WGPUCommandEncoderDescriptor encoder_desc {};
    encoder_desc.label = WGPUStringView { .data = "mpm solver command encoder", .length = WGPU_STRLEN };
    webgpu::raii::CommandEncoder encoder(m_ctx->device(), encoder_desc);

    // Cleared outside the compute pass; the density raster only accumulates once per run.
    if (m_run_first_chunk)
        m_density_buffer->clear(encoder.handle());
    // The grid update zeroes the accumulators it consumed, so the grid stays clean from one
    // substep to the next without a clear pass. A reseed starts from a known state anyway,
    // in case a previous run was abandoned half way (graph replaced, device hiccup).
    if (m_run_first_chunk && m_run_is_reset) {
        m_grid_buffer->clear(encoder.handle());
        m_tile_flag_buffer->clear(encoder.handle());
    }

    // Time this chunk if its timestamp slot is free (its readback from two chunks ago is done).
    const int slot = int(m_chunk_counter++ % TIMESTAMP_SLOTS);
    const int timestamp_slot = (m_timestamp_queries != nullptr && !m_timestamp_busy[size_t(slot)]) ? slot : -1;
    WGPUPassTimestampWrites timestamp_writes {};
    if (timestamp_slot >= 0) {
        timestamp_writes.querySet = m_timestamp_queries;
        timestamp_writes.beginningOfPassWriteIndex = uint32_t(2 * timestamp_slot);
        timestamp_writes.endOfPassWriteIndex = uint32_t(2 * timestamp_slot + 1);
    }

    {
        WGPUComputePassDescriptor compute_pass_desc {};
        compute_pass_desc.label = WGPUStringView { .data = "mpm solver compute pass", .length = WGPU_STRLEN };
        compute_pass_desc.timestampWrites = timestamp_slot >= 0 ? &timestamp_writes : nullptr;
        webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

        // Every pipeline shares the layout, so the bind group is set once for the pass.
        wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, m_bind_group->handle(), 0, nullptr);

        if (m_run_first_chunk && m_run_is_reset) {
            m_prepare_pipeline->run(compute_pass, prepare_workgroups);
            m_seed_pipeline->run(compute_pass, particle_workgroups);
        } else if (m_run_first_chunk && m_run_refresh_stress) {
            m_refresh_stress_pipeline->run(compute_pass, particle_workgroups);
        }

        // Dispatches within one compute pass are ordered and see each other's storage
        // writes, which is exactly the dependency chain an MPM substep needs.
        for (uint32_t step = 0; step < substeps; step++) {
            m_p2g_pipeline->run(compute_pass, particle_workgroups);
            m_grid_update_pipeline->run(compute_pass, grid_workgroups);
            m_g2p_pipeline->run(compute_pass, particle_workgroups);
        }

        if (last_chunk) {
            m_splat_pipeline->run(compute_pass, particle_workgroups);
            m_rasterize_pipeline->run(compute_pass, raster_workgroups);
        }
    }

    if (timestamp_slot >= 0) {
        const size_t offset = size_t(timestamp_slot) * TIMESTAMP_SLOT_STRIDE_BYTES;
        wgpuCommandEncoderResolveQuerySet(encoder.handle(), m_timestamp_queries, uint32_t(2 * timestamp_slot), 2, m_timestamp_resolve->handle(), offset);
        m_timestamp_resolve->copy_to_buffer(encoder.handle(), offset, *m_timestamp_readback[size_t(timestamp_slot)], 0, 2 * sizeof(uint64_t));
        m_timestamp_busy[size_t(timestamp_slot)] = true;
    }

    // Diagnostics ride along in the last chunk's command buffer. Skipped while the staging
    // buffer is still mapped from an earlier run - a buffer cannot be written while mapped.
    if (last_chunk && !m_readback_in_flight) {
        m_state_buffer->copy_to_buffer(encoder.handle(), *m_state_readback);
        m_readback_copied = true;
    }

    WGPUCommandBufferDescriptor cmd_buffer_desc {};
    cmd_buffer_desc.label = WGPUStringView { .data = "mpm solver command buffer", .length = WGPU_STRLEN };
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_desc);
    wgpuQueueSubmit(m_ctx->queue(), 1, &command);
    wgpuCommandBufferRelease(command);

    m_run_first_chunk = false;
    m_chunks_in_flight++;
    m_simulated_time += float(substeps) * m_settings.dt;

    // The callback fires from the app's event pump, i.e. once per frame - which is what
    // lets a rendered frame slip in between two chunks.
    // The userdata is a heap copy of m_alive rather than `this`: if the graph is replaced
    // while a chunk is in flight, the node is gone by the time the callback runs.
    struct ChunkDone {
        std::shared_ptr<MpmSolverNode*> alive;
        int timestamp_slot;
        uint32_t substeps;
    };
    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              std::unique_ptr<ChunkDone> done(reinterpret_cast<ChunkDone*>(userdata));
              if (MpmSolverNode* self = *done->alive)
                  self->on_chunk_done(done->timestamp_slot, done->substeps);
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = new ChunkDone { m_alive, timestamp_slot, substeps },
        .userdata2 = nullptr,
    };

    wgpuQueueOnSubmittedWorkDone(m_ctx->queue(), callback_info);
}

void MpmSolverNode::on_chunk_done(int timestamp_slot, uint32_t substeps)
{
    if (timestamp_slot >= 0)
        read_back_timestamps(timestamp_slot, substeps);

    m_chunks_in_flight--;
    if (m_run_substeps_left > 0) {
        submit_chunk();
        return;
    }
    if (m_chunks_in_flight > 0)
        return; // the last chunk is still running; its own callback finishes the run
    read_back_state();
}

void MpmSolverNode::read_back_timestamps(int slot, uint32_t substeps)
{
    struct Request {
        std::shared_ptr<MpmSolverNode*> alive;
        int slot;
        uint32_t substeps;
    };
    const auto on_mapped = [](WGPUMapAsyncStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
        std::unique_ptr<Request> request(reinterpret_cast<Request*>(userdata));
        MpmSolverNode* self = *request->alive;
        if (self == nullptr)
            return;
        auto& buffer = *self->m_timestamp_readback[size_t(request->slot)];
        if (status == WGPUMapAsyncStatus_Success) {
            const auto* ticks = static_cast<const uint64_t*>(wgpuBufferGetConstMappedRange(buffer.handle(), 0, buffer.size_in_byte()));
            // Timestamps can come back reset or out of order (e.g. power state changes);
            // such a sample is dropped rather than averaged in.
            if (ticks != nullptr && ticks[1] > ticks[0]) {
                const float ms = float(double(ticks[1] - ticks[0]) / 1.0e6) / float(std::max(request->substeps, 1u));
                auto& average = self->m_perf.gpu_ms_per_substep;
                average = average <= 0.0f ? ms : 0.8f * average + 0.2f * ms;
            }
            wgpuBufferUnmap(buffer.handle());
        }
        self->m_timestamp_busy[size_t(request->slot)] = false;
    };

    WGPUBufferMapCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_mapped,
        .userdata1 = new Request { m_alive, slot, substeps },
        .userdata2 = nullptr,
    };
    auto& buffer = *m_timestamp_readback[size_t(slot)];
    wgpuBufferMapAsync(buffer.handle(), WGPUMapMode_Read, 0, buffer.size_in_byte(), callback_info);
}

std::unique_ptr<webgpu::raii::TextureWithSampler> MpmSolverNode::create_output_texture(WGPUDevice device, uint32_t width, uint32_t height)
{
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "mpm solver output texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat_RGBA8Unorm;
    texture_desc.usage = WGPUTextureUsage(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc);

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "mpm solver output sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode_Linear;
    sampler_desc.minFilter = WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    return std::make_unique<webgpu::raii::TextureWithSampler>(device, texture_desc, sampler_desc);
}

void MpmSolverNode::serialize_settings(QJsonObject& out) const
{
    const auto& s = m_settings;
    out["domain_center_lat"] = s.domain_center.x;
    out["domain_center_lon"] = s.domain_center.y;
    out["domain_size_xy"] = s.domain_size_xy;
    out["grid_resolution_xy"] = static_cast<int>(s.grid_resolution_xy);
    out["grid_layers"] = static_cast<int>(s.grid_layers);
    out["num_particles"] = static_cast<int>(s.num_particles);
    out["slab_thickness"] = s.slab_thickness;
    out["snow_density"] = s.snow_density;
    out["dt"] = s.dt;
    out["substeps_per_run"] = static_cast<int>(s.substeps_per_run);
    out["substeps_per_submit"] = static_cast<int>(s.substeps_per_submit);
    out["youngs_modulus"] = s.youngs_modulus;
    out["poissons_ratio"] = s.poissons_ratio;
    out["hardening"] = s.hardening;
    out["critical_compression"] = s.critical_compression;
    out["critical_stretch"] = s.critical_stretch;
    out["gravity"] = s.gravity;
    out["terrain_friction"] = s.terrain_friction;
    out["constitutive_model"] = static_cast<int>(s.constitutive_model);
    out["basal_friction_model"] = static_cast<int>(s.basal_friction_model);
    out["voellmy_xi"] = s.voellmy_xi;
    out["dp_friction_angle"] = s.dp_friction_angle;
    out["ccc_m"] = s.ccc_m;
    out["ccc_beta"] = s.ccc_beta;
    out["ccc_xi"] = s.ccc_xi;
    out["ccc_p0_initial"] = s.ccc_p0_initial;
    out["raster_resolution"] = static_cast<int>(s.raster_resolution);
    out["splat_radius"] = s.splat_radius;
    out["release_center_lat"] = s.release_center.x;
    out["release_center_lon"] = s.release_center.y;
    out["release_radius"] = s.release_radius;
    out["random_seed"] = static_cast<int>(s.random_seed);
    out["seed_anywhere"] = s.seed_anywhere;
}

void MpmSolverNode::deserialize_settings(const QJsonObject& in)
{
    auto& s = m_settings;
    const auto read_float = [&in](const char* key, float fallback) { return in.contains(key) ? float(in[key].toDouble(fallback)) : fallback; };
    const auto read_double = [&in](const char* key, double fallback) { return in.contains(key) ? in[key].toDouble(fallback) : fallback; };
    const auto read_uint
        = [&in](const char* key, uint32_t fallback) { return in.contains(key) ? uint32_t(in[key].toInt(static_cast<int>(fallback))) : fallback; };

    s.domain_center.x = read_double("domain_center_lat", s.domain_center.x);
    s.domain_center.y = read_double("domain_center_lon", s.domain_center.y);
    s.domain_size_xy = read_float("domain_size_xy", s.domain_size_xy);
    s.grid_resolution_xy = read_uint("grid_resolution_xy", s.grid_resolution_xy);
    s.grid_layers = read_uint("grid_layers", s.grid_layers);
    s.num_particles = read_uint("num_particles", s.num_particles);
    s.slab_thickness = read_float("slab_thickness", s.slab_thickness);
    s.snow_density = read_float("snow_density", s.snow_density);
    s.dt = read_float("dt", s.dt);
    s.substeps_per_run = read_uint("substeps_per_run", s.substeps_per_run);
    s.substeps_per_submit = read_uint("substeps_per_submit", s.substeps_per_submit);
    s.youngs_modulus = read_float("youngs_modulus", s.youngs_modulus);
    s.poissons_ratio = read_float("poissons_ratio", s.poissons_ratio);
    s.hardening = read_float("hardening", s.hardening);
    s.critical_compression = read_float("critical_compression", s.critical_compression);
    s.critical_stretch = read_float("critical_stretch", s.critical_stretch);
    s.gravity = read_float("gravity", s.gravity);
    s.terrain_friction = read_float("terrain_friction", s.terrain_friction);
    s.constitutive_model = static_cast<ConstitutiveModel>(read_uint("constitutive_model", s.constitutive_model));
    s.basal_friction_model = static_cast<BasalFrictionModel>(read_uint("basal_friction_model", s.basal_friction_model));
    s.voellmy_xi = read_float("voellmy_xi", s.voellmy_xi);
    s.dp_friction_angle = read_float("dp_friction_angle", s.dp_friction_angle);
    s.ccc_m = read_float("ccc_m", s.ccc_m);
    s.ccc_beta = read_float("ccc_beta", s.ccc_beta);
    s.ccc_xi = read_float("ccc_xi", s.ccc_xi);
    s.ccc_p0_initial = read_float("ccc_p0_initial", s.ccc_p0_initial);
    s.raster_resolution = read_uint("raster_resolution", s.raster_resolution);
    s.splat_radius = read_float("splat_radius", s.splat_radius);
    s.release_center.x = read_double("release_center_lat", s.release_center.x);
    s.release_center.y = read_double("release_center_lon", s.release_center.y);
    s.release_radius = read_float("release_radius", s.release_radius);
    s.random_seed = read_uint("random_seed", s.random_seed);
    if (in.contains("seed_anywhere"))
        s.seed_anywhere = in["seed_anywhere"].toBool(s.seed_anywhere);
    s.reset_on_next_run = true;
}

} // namespace webgpu_compute::nodes
