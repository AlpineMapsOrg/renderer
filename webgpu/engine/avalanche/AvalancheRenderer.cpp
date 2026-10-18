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

#include "AvalancheRenderer.h"

#include <algorithm>
#include <nucleus/camera/Definition.h>
#include <webgpu/base/Framebuffer.h>

namespace webgpu_engine {

namespace {
    // struct Particle in webgpu/compute/shaders/mpm_common.wgsl: eight vec4s.
    constexpr uint32_t PARTICLE_STRIDE_U32 = 32;
    constexpr uint32_t QUAD_VERTICES = 4; // a triangle strip
} // namespace

void AvalancheRenderer::init(webgpu::Context& ctx)
{
    m_ctx = &ctx;
    m_config = std::make_unique<webgpu::Buffer<GpuConfig>>(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);

    auto& reg = ctx.resource_registry();
    reg.register_shader("render_avalanche", "webgpu_engine::render_avalanche");
    reg.register_bind_group_layout("avalanche", [](WGPUDevice device) {
        WGPUBindGroupLayoutEntry particles_entry {};
        particles_entry.binding = 0;
        particles_entry.visibility = WGPUShaderStage_Vertex;
        particles_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        particles_entry.buffer.minBindingSize = 0;

        WGPUBindGroupLayoutEntry config_entry {};
        config_entry.binding = 1;
        config_entry.visibility = WGPUShaderStage(WGPUShaderStage_Vertex | WGPUShaderStage_Fragment);
        config_entry.buffer.type = WGPUBufferBindingType_Uniform;
        config_entry.buffer.minBindingSize = sizeof(GpuConfig);

        return std::make_unique<webgpu::raii::BindGroupLayout>(
            device, std::vector<WGPUBindGroupLayoutEntry> { particles_entry, config_entry }, "avalanche renderer bind group layout");
    });
    reg.register_pipeline([this](WGPUDevice dev, const webgpu::RenderResourceRegistry& reg) {
        // The G-buffer of TileMeshRenderer: the avalanche is drawn into the same pass.
        webgpu::FramebufferFormat format {};
        format.depth_format = WGPUTextureFormat_Depth24Plus;
        format.color_formats.emplace_back(WGPUTextureFormat_R32Uint); // albedo
        format.color_formats.emplace_back(WGPUTextureFormat_RGBA32Float); // position
        format.color_formats.emplace_back(WGPUTextureFormat_RG16Uint); // normal
        format.color_formats.emplace_back(WGPUTextureFormat_R32Uint); // overlay

        m_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(dev,
            reg.shader("render_avalanche"),
            reg.shader("render_avalanche"),
            std::vector<webgpu::util::SingleVertexBufferInfo> {}, // vertex pulling from the particle buffer
            format,
            std::vector<const webgpu::raii::BindGroupLayout*> {
                &reg.bind_group_layout("shared_config"),
                &reg.bind_group_layout("camera"),
                &reg.bind_group_layout("avalanche"),
            });
        m_bind_group.reset(); // the layout may be new
        m_bound_particles = nullptr;
    });
}

void AvalancheRenderer::draw(WGPURenderPassEncoder render_pass, const nucleus::camera::Definition& camera)
{
    if (!settings.enabled || !m_source || m_pipeline == nullptr)
        return;
    const std::optional<Frame> frame = m_source();
    if (!frame || frame->particles == nullptr || frame->particle_count == 0)
        return;

    // Camera-relative placement, computed in double: world coordinates are ~1e6, too large
    // for float once a metre matters.
    const glm::dmat3& m = frame->world_from_metres;
    const glm::dvec3 cam = camera.position();
    GpuConfig& c = m_config->data;
    c.xy_from_metres = glm::vec4(float(m[0][0]), float(m[0][1]), float(m[1][0]), float(m[1][1]));
    c.offset_cws = glm::vec2(glm::dvec2(m[2]) - glm::dvec2(cam));
    c.radius = std::max(settings.radius_scale, 0.01f) * frame->particle_spacing;
    c.colour_mode = uint32_t(settings.colour_mode);
    c.min_pixel_radius = std::max(settings.min_pixel_radius, 0.0f);
    c.speed_max = std::max(settings.speed_max, 1.0f);
    c.particle_count = frame->particle_count;
    c.strain_window = settings.shape == Shape::StrainRate ? std::max(settings.strain_window, 0.0f) : 0.0f;
    m_config->update_gpu_data(m_ctx->queue());

    // The solver reallocates its buffer when the particle count or grid changes.
    if (m_bind_group == nullptr || frame->particles->handle() != m_bound_particles) {
        m_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
            m_ctx->resource_registry().bind_group_layout("avalanche"),
            std::vector<WGPUBindGroupEntry> {
                frame->particles->create_bind_group_entry(0),
                m_config->raw_buffer().create_bind_group_entry(1),
            },
            "avalanche renderer bind group");
        m_bound_particles = frame->particles->handle();
    }

    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline->pipeline().handle());
    wgpuRenderPassEncoderSetBindGroup(render_pass, 2, m_bind_group->handle(), 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass, QUAD_VERTICES, std::min(frame->particle_count, uint32_t(frame->particles->size() / PARTICLE_STRIDE_U32)), 0, 0);
}

} // namespace webgpu_engine
