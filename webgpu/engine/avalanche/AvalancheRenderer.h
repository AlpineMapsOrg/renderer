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

#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <webgpu/base/Buffer.h>
#include <webgpu/base/Context.h>
#include <webgpu/base/raii/BindGroup.h>
#include <webgpu/base/raii/CombinedComputePipeline.h>
#include <webgpu/base/raii/Pipeline.h>
#include <webgpu/base/raii/RawBuffer.h>
#include <webgpu/base/raii/TextureWithSampler.h>

namespace nucleus::camera {
class Definition;
}

namespace webgpu_engine {

/// Draws the particles of the MLS-MPM avalanche solver in 3D, as sphere impostors written
/// into the G-buffer: each particle is a camera-facing quad whose fragment shader intersects
/// a sphere and writes its exact depth, normal and albedo. The compose pass then lights and
/// hazes the avalanche exactly like the terrain. Drawn in the G-buffer pass, after the tiles.
///
/// The engine does not know the solver (webgpu_compute). An app-side node sets a source
/// callback that hands over the particle buffer every frame; the buffer layout is the
/// solver's struct Particle (mpm_common.wgsl), 32 words per particle, position first.
///
/// Optionally a flow surface over the particles: prepare() bins them into the solver's grid
/// columns (a compute pass, before the G-buffer pass), and draw() lays a blanket over the
/// covered columns, so the dense core reads as a continuous snow mass while particles above it
/// show the front and the spray. See mpm-mls-doc/12-rendering-plan.md.
class AvalancheRenderer {
public:
    enum class ColourMode : uint32_t { Snow = 0, Speed = 1 };
    /// Spheres, or ellipsoids stretched along the particle's current strain rate: the
    /// symmetric part of its velocity gradient (the APIC matrix C), integrated over a short
    /// window. They show where the flow shears, stretches or compacts right now. Not the
    /// accumulated deformation: the solver keeps only the elastic part of F, which stays
    /// within about 1 % for snow, and the total deformation of a flow grows without bound.
    enum class Shape : uint32_t { Spheres = 0, StrainRate = 1 };

    struct Settings {
        bool enabled = true;
        ColourMode colour_mode = ColourMode::Snow;
        float radius_scale = 0.6f; // sphere radius as a share of the particle spacing
        Shape shape = Shape::Spheres;
        float strain_window = 0.15f; // [s] time over which the strain rate stretches an ellipsoid
        float min_pixel_radius = 1.5f; // [px] far away, particles keep at least this size
        float speed_max = 60.0f; // [m/s] top of the speed colour scale
        bool surface = true; // the flow surface over the covered columns
        uint32_t surface_min_count = 8; // particles in the 3x3 columns around a column for it to belong to the surface
    };

    /// One frame's particles. world_from_metres maps particle.position.xy (metres) to world
    /// xy as world = M * (x, y, 1); particle.position.z is the altitude in metres.
    struct Frame {
        const webgpu::raii::RawBuffer<uint32_t>* particles = nullptr;
        uint32_t particle_count = 0;
        glm::dmat3 world_from_metres = glm::dmat3(1.0);
        float particle_spacing = 1.0f; // [m]
        glm::vec2 domain_origin = glm::vec2(0.0f); // [m] the solver's grid node (0, 0)
        float dx = 1.0f; // [m] its node spacing
        glm::uvec2 columns = glm::uvec2(0u); // its nodes in x and y
        // The solver's terrain (R32Float metres, uv = metres / region_size, v south), for the surface.
        const webgpu::raii::TextureWithSampler* height_texture = nullptr;
        glm::vec2 region_size = glm::vec2(1.0f); // [m]
    };
    using Source = std::function<std::optional<Frame>()>;

    AvalancheRenderer() = default;
    void init(webgpu::Context& ctx);

    /// The callback that delivers the particles each frame. The owner must clear it before
    /// whatever the callback reads is destroyed; clearing only takes effect for the owner that
    /// set the current source, so a stale owner cannot remove a newer one.
    void set_source(Source source, const void* owner)
    {
        m_source = std::move(source);
        m_source_owner = owner;
    }
    void clear_source(const void* owner)
    {
        if (owner != m_source_owner)
            return;
        m_source = {};
        m_source_owner = nullptr;
    }
    bool has_source() const { return bool(m_source); }

    Settings settings;

    /// Fetches this frame's particles and, for the flow surface, records the binning compute
    /// pass. Call before the G-buffer render pass; draw() uses what it fetched.
    void prepare(WGPUCommandEncoder encoder, const nucleus::camera::Definition& camera);
    /// Records the draws into a G-buffer render pass whose groups 0 (shared config) and 1
    /// (camera) are already bound.
    void draw(WGPURenderPassEncoder render_pass);

private:
    struct GpuConfig {
        glm::vec4 xy_from_metres = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // columns of the 2x2 linear part
        glm::vec2 offset_cws = glm::vec2(0.0f); // world offset minus camera xy
        float radius = 1.0f; // [m]
        uint32_t colour_mode = 0u;
        float min_pixel_radius = 1.5f;
        float speed_max = 60.0f;
        uint32_t particle_count = 0u;
        float strain_window = 0.0f; // [s], 0 = spheres
    };
    // avalanche_surface_common.wgsl
    struct SurfaceConfig {
        glm::vec4 xy_from_metres = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec2 offset_cws = glm::vec2(0.0f);
        glm::vec2 domain_origin = glm::vec2(0.0f);
        glm::uvec2 columns = glm::uvec2(0u);
        float dx = 1.0f;
        float radius = 1.0f;
        uint32_t particle_count = 0u;
        uint32_t min_count = 2u;
        uint32_t colour_mode = 0u;
        float speed_max = 60.0f;
        glm::vec2 region_size = glm::vec2(1.0f);
        glm::uvec2 height_dim = glm::uvec2(1u);
    };

    webgpu::Context* m_ctx = nullptr;
    Source m_source;
    const void* m_source_owner = nullptr;
    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_pipeline;
    std::unique_ptr<webgpu::Buffer<GpuConfig>> m_config;
    std::unique_ptr<webgpu::raii::BindGroup> m_bind_group;
    WGPUBuffer m_bound_particles = nullptr; // the buffer m_bind_group refers to

    std::optional<Frame> m_frame; // fetched by prepare() for this frame's draw()

    /* Flow surface: per-column accumulators (avalanche_surface_common.wgsl), the binning
     * compute pipeline and the surface render pipeline. */
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_bin_pipeline;
    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_surface_pipeline;
    std::unique_ptr<webgpu::Buffer<SurfaceConfig>> m_surface_config;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_columns;
    std::unique_ptr<webgpu::raii::RawBuffer<float>> m_depth; // per column: flow depth [m], top speed [m/s]
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_depth_pipeline;
    std::unique_ptr<webgpu::raii::BindGroup> m_bin_bind_group;
    std::unique_ptr<webgpu::raii::BindGroup> m_depth_bind_group;
    std::unique_ptr<webgpu::raii::BindGroup> m_surface_bind_group;
    WGPUBuffer m_bin_bound_particles = nullptr;
    WGPUTexture m_bound_height = nullptr; // the height texture the depth and surface bind groups use
    bool m_surface_ready = false; // prepare() binned this frame's particles
};

} // namespace webgpu_engine
