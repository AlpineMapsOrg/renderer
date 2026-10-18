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
#include <webgpu/base/raii/Pipeline.h>
#include <webgpu/base/raii/RawBuffer.h>

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
/// See mpm-mls-doc/12-rendering-plan.md.
class AvalancheRenderer {
public:
    enum class ColourMode : uint32_t { Snow = 0, Speed = 1 };

    struct Settings {
        bool enabled = true;
        ColourMode colour_mode = ColourMode::Snow;
        float radius_scale = 0.6f; // sphere radius as a share of the particle spacing
        float min_pixel_radius = 1.5f; // [px] far away, particles keep at least this size
        float speed_max = 60.0f; // [m/s] top of the speed colour scale
    };

    /// One frame's particles. world_from_metres maps particle.position.xy (metres) to world
    /// xy as world = M * (x, y, 1); particle.position.z is the altitude in metres.
    struct Frame {
        const webgpu::raii::RawBuffer<uint32_t>* particles = nullptr;
        uint32_t particle_count = 0;
        glm::dmat3 world_from_metres = glm::dmat3(1.0);
        float particle_spacing = 1.0f; // [m]
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

    /// Records the draw into a G-buffer render pass whose groups 0 (shared config) and 1
    /// (camera) are already bound.
    void draw(WGPURenderPassEncoder render_pass, const nucleus::camera::Definition& camera);

private:
    struct GpuConfig {
        glm::vec4 xy_from_metres = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // columns of the 2x2 linear part
        glm::vec2 offset_cws = glm::vec2(0.0f); // world offset minus camera xy
        float radius = 1.0f; // [m]
        uint32_t colour_mode = 0u;
        float min_pixel_radius = 1.5f;
        float speed_max = 60.0f;
        uint32_t particle_count = 0u;
        float padding = 0.0f;
    };

    webgpu::Context* m_ctx = nullptr;
    Source m_source;
    const void* m_source_owner = nullptr;
    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_pipeline;
    std::unique_ptr<webgpu::Buffer<GpuConfig>> m_config;
    std::unique_ptr<webgpu::raii::BindGroup> m_bind_group;
    WGPUBuffer m_bound_particles = nullptr; // the buffer m_bind_group refers to
};

} // namespace webgpu_engine
