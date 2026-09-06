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
#include <array>
#include <cstring>
#include <limits>
#include <nucleus/srs.h>

namespace webgpu_compute::nodes {

glm::uvec3 MpmSolverNode::PARTICLE_WORKGROUP_SIZE = { 256, 1, 1 };
glm::uvec3 MpmSolverNode::GRID_WORKGROUP_SIZE = { 4, 4, 4 };
glm::uvec3 MpmSolverNode::RASTER_WORKGROUP_SIZE = { 16, 16, 1 };

const uint32_t MpmSolverNode::MAX_PARTICLES = 1u << 21;
const uint32_t MpmSolverNode::MAX_GRID_RESOLUTION = 256u;

namespace {

    /* Size of struct Particle in mpm_common.wgsl, in units of uint32. */
    constexpr uint32_t PARTICLE_STRIDE_U32 = 32u; // 128 bytes
    /* Size of struct GridNode in mpm_common.wgsl, in units of uint32. */
    constexpr uint32_t GRID_NODE_STRIDE_U32 = 4u; // 16 bytes
    /* Size of struct SimState in mpm_common.wgsl, in units of uint32. */
    constexpr uint32_t SIM_STATE_SIZE_U32 = 4u;

    constexpr uint32_t div_ceil(uint32_t value, uint32_t divisor) { return (value + divisor - 1u) / divisor; }

    WGPUBindGroupLayoutEntry buffer_entry(uint32_t binding, WGPUBufferBindingType type)
    {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = binding;
        entry.visibility = WGPUShaderStage_Compute;
        entry.buffer.type = type;
        return entry;
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
    , m_domain_aabb { glm::dvec2(0.0), glm::dvec2(0.0) }
{
    auto& reg = ctx.resource_registry();

    reg.register_shader("mpm_prepare", "webgpu_compute::mpm_prepare");
    reg.register_shader("mpm_seed", "webgpu_compute::mpm_seed");
    reg.register_shader("mpm_clear_grid", "webgpu_compute::mpm_clear_grid");
    reg.register_shader("mpm_p2g", "webgpu_compute::mpm_p2g");
    reg.register_shader("mpm_grid_update", "webgpu_compute::mpm_grid_update");
    reg.register_shader("mpm_g2p", "webgpu_compute::mpm_g2p");
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
            },
            "mpm solver bind group layout");
    });

    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        const std::vector<const webgpu::raii::BindGroupLayout*> layouts { &reg.bind_group_layout("mpm_solver") };
        const auto make = [&](const std::string& shader_name) {
            return std::make_unique<webgpu::raii::CombinedComputePipeline>(device, reg.shader(shader_name), layouts);
        };
        m_prepare_pipeline = make("mpm_prepare");
        m_seed_pipeline = make("mpm_seed");
        m_clear_grid_pipeline = make("mpm_clear_grid");
        m_p2g_pipeline = make("mpm_p2g");
        m_grid_update_pipeline = make("mpm_grid_update");
        m_g2p_pipeline = make("mpm_g2p");
        m_splat_pipeline = make("mpm_splat");
        m_rasterize_pipeline = make("mpm_rasterize");
    });
}

bool MpmSolverNode::ensure_resources()
{
    const uint32_t num_particles = std::clamp(m_settings.num_particles, 1u, MAX_PARTICLES);
    const glm::uvec3 grid_res(std::clamp(m_settings.grid_resolution_xy, 8u, MAX_GRID_RESOLUTION),
        std::clamp(m_settings.grid_resolution_xy, 8u, MAX_GRID_RESOLUTION),
        std::clamp(m_settings.grid_resolution_z, 8u, MAX_GRID_RESOLUTION));
    const uint32_t raster_resolution = std::clamp(m_settings.raster_resolution, 64u, 4096u);

    if (m_particle_buffer && num_particles == m_allocated_particles && grid_res == m_allocated_grid_res
        && raster_resolution == m_allocated_raster_resolution) {
        return false;
    }

    const auto storage_usage = WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc);

    m_particle_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), storage_usage, size_t(num_particles) * PARTICLE_STRIDE_U32, "mpm particle buffer");
    m_grid_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), storage_usage, size_t(grid_res.x) * grid_res.y * grid_res.z * GRID_NODE_STRIDE_U32, "mpm grid buffer");
    m_state_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(), storage_usage, SIM_STATE_SIZE_U32, "mpm state buffer");
    m_density_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(
        m_ctx->device(), storage_usage, size_t(raster_resolution) * raster_resolution, "mpm density raster");
    m_output_texture = create_output_texture(m_ctx->device(), raster_resolution, raster_resolution);

    m_allocated_particles = num_particles;
    m_allocated_grid_res = grid_res;
    m_allocated_raster_resolution = raster_resolution;
    m_output_dimensions = glm::uvec2(raster_resolution);

    // Fresh buffers hold garbage - the simulation has to be re-seeded.
    m_settings.reset_on_next_run = true;
    return true;
}

void MpmSolverNode::create_bind_group(const webgpu::raii::TextureWithSampler& height_texture, const webgpu::raii::TextureWithSampler& release_point_texture)
{
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

    m_state_buffer->write(m_ctx->queue(), initial.data(), initial.size(), 0);
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

    ensure_resources();
    update_gpu_settings(*region_aabb, height_texture);
    // The input textures may be recreated by upstream nodes between runs, so the bind
    // group is rebuilt every time rather than cached.
    create_bind_group(height_texture, release_point_texture);

    const bool reset = m_settings.reset_on_next_run;
    if (reset) {
        write_initial_state();
        m_simulated_time = 0.0f;

        // Report where on the planet we are actually simulating - without this there is no
        // way to tell from the UI which slope the domain landed on.
        const glm::dvec2 south_west = nucleus::srs::world_to_lat_long(m_domain_aabb.min);
        const glm::dvec2 north_east = nucleus::srs::world_to_lat_long(m_domain_aabb.max);
        const glm::dvec2 centre = nucleus::srs::world_to_lat_long((m_domain_aabb.min + m_domain_aabb.max) * 0.5);
        qInfo().nospace() << "MpmSolverNode: simulating " << m_settings.domain_size_xy << " m domain at centre " << centre.x << ", " << centre.y
                          << " (lat " << south_west.x << ".." << north_east.x << ", lon " << south_west.y << ".." << north_east.y << "), "
                          << m_allocated_grid_res.x << "x" << m_allocated_grid_res.y << "x" << m_allocated_grid_res.z << " grid, dx "
                          << m_settings_uniform.data.dx << " m, " << m_allocated_particles << " particles";
    }

    const uint32_t substeps = std::clamp(m_settings.substeps_per_run, 1u, 4096u);
    const glm::uvec3 particle_workgroups(div_ceil(m_allocated_particles, PARTICLE_WORKGROUP_SIZE.x), 1, 1);
    const glm::uvec3 grid_workgroups(div_ceil(m_allocated_grid_res.x, GRID_WORKGROUP_SIZE.x),
        div_ceil(m_allocated_grid_res.y, GRID_WORKGROUP_SIZE.y),
        div_ceil(m_allocated_grid_res.z, GRID_WORKGROUP_SIZE.z));
    const uint32_t grid_node_count = m_allocated_grid_res.x * m_allocated_grid_res.y * m_allocated_grid_res.z;
    const glm::uvec3 grid_clear_workgroups(div_ceil(grid_node_count, PARTICLE_WORKGROUP_SIZE.x), 1, 1);
    const glm::uvec3 prepare_workgroups(
        div_ceil(m_allocated_grid_res.x, RASTER_WORKGROUP_SIZE.x), div_ceil(m_allocated_grid_res.y, RASTER_WORKGROUP_SIZE.y), 1);
    const glm::uvec3 raster_workgroups(
        div_ceil(m_output_dimensions.x, RASTER_WORKGROUP_SIZE.x), div_ceil(m_output_dimensions.y, RASTER_WORKGROUP_SIZE.y), 1);

    WGPUCommandEncoderDescriptor encoder_desc {};
    encoder_desc.label = WGPUStringView { .data = "mpm solver command encoder", .length = WGPU_STRLEN };
    webgpu::raii::CommandEncoder encoder(m_ctx->device(), encoder_desc);

    // Cleared outside the compute pass; the density raster only accumulates once per run.
    m_density_buffer->clear(encoder.handle());

    {
        WGPUComputePassDescriptor compute_pass_desc {};
        compute_pass_desc.label = WGPUStringView { .data = "mpm solver compute pass", .length = WGPU_STRLEN };
        webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

        // Every pipeline shares the layout, so the bind group is set once for the pass.
        wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, m_bind_group->handle(), 0, nullptr);

        if (reset) {
            m_prepare_pipeline->run(compute_pass, prepare_workgroups);
            m_seed_pipeline->run(compute_pass, particle_workgroups);
        }

        // Dispatches within one compute pass are ordered and see each other's storage
        // writes, which is exactly the dependency chain an MPM substep needs.
        for (uint32_t step = 0; step < substeps; step++) {
            m_clear_grid_pipeline->run(compute_pass, grid_clear_workgroups);
            m_p2g_pipeline->run(compute_pass, particle_workgroups);
            m_grid_update_pipeline->run(compute_pass, grid_workgroups);
            m_g2p_pipeline->run(compute_pass, particle_workgroups);
        }

        m_splat_pipeline->run(compute_pass, particle_workgroups);
        m_rasterize_pipeline->run(compute_pass, raster_workgroups);
    }

    WGPUCommandBufferDescriptor cmd_buffer_desc {};
    cmd_buffer_desc.label = WGPUStringView { .data = "mpm solver command buffer", .length = WGPU_STRLEN };
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_desc);
    wgpuQueueSubmit(m_ctx->queue(), 1, &command);
    wgpuCommandBufferRelease(command);

    m_settings.reset_on_next_run = false;
    m_simulated_time += float(substeps) * m_settings.dt;

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              MpmSolverNode* _this = reinterpret_cast<MpmSolverNode*>(userdata);
              _this->complete_run();
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = this,
        .userdata2 = nullptr,
    };

    wgpuQueueOnSubmittedWorkDone(m_ctx->queue(), callback_info);
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
    out["grid_resolution_z"] = static_cast<int>(s.grid_resolution_z);
    out["num_particles"] = static_cast<int>(s.num_particles);
    out["slab_thickness"] = s.slab_thickness;
    out["snow_density"] = s.snow_density;
    out["dt"] = s.dt;
    out["substeps_per_run"] = static_cast<int>(s.substeps_per_run);
    out["youngs_modulus"] = s.youngs_modulus;
    out["poissons_ratio"] = s.poissons_ratio;
    out["hardening"] = s.hardening;
    out["critical_compression"] = s.critical_compression;
    out["critical_stretch"] = s.critical_stretch;
    out["gravity"] = s.gravity;
    out["terrain_friction"] = s.terrain_friction;
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
    s.grid_resolution_z = read_uint("grid_resolution_z", s.grid_resolution_z);
    s.num_particles = read_uint("num_particles", s.num_particles);
    s.slab_thickness = read_float("slab_thickness", s.slab_thickness);
    s.snow_density = read_float("snow_density", s.snow_density);
    s.dt = read_float("dt", s.dt);
    s.substeps_per_run = read_uint("substeps_per_run", s.substeps_per_run);
    s.youngs_modulus = read_float("youngs_modulus", s.youngs_modulus);
    s.poissons_ratio = read_float("poissons_ratio", s.poissons_ratio);
    s.hardening = read_float("hardening", s.hardening);
    s.critical_compression = read_float("critical_compression", s.critical_compression);
    s.critical_stretch = read_float("critical_stretch", s.critical_stretch);
    s.gravity = read_float("gravity", s.gravity);
    s.terrain_friction = read_float("terrain_friction", s.terrain_friction);
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
