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
    constexpr uint32_t COLUMN_STRIDE_U32 = 4; // avalanche_surface_common.wgsl
    constexpr uint32_t BIN_WORKGROUP_SIZE = 256;

    // The G-buffer of TileMeshRenderer: the avalanche is drawn into the same pass.
    webgpu::FramebufferFormat gbuffer_format()
    {
        webgpu::FramebufferFormat format {};
        format.depth_format = WGPUTextureFormat_Depth24Plus;
        format.color_formats.emplace_back(WGPUTextureFormat_R32Uint); // albedo
        format.color_formats.emplace_back(WGPUTextureFormat_RGBA32Float); // position
        format.color_formats.emplace_back(WGPUTextureFormat_RG16Uint); // normal
        format.color_formats.emplace_back(WGPUTextureFormat_R32Uint); // overlay
        return format;
    }

    WGPUBindGroupLayoutEntry buffer_entry(uint32_t binding, WGPUShaderStage visibility, WGPUBufferBindingType type)
    {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = binding;
        entry.visibility = visibility;
        entry.buffer.type = type;
        entry.buffer.minBindingSize = 0;
        return entry;
    }

    WGPUBindGroupLayoutEntry height_texture_entry(uint32_t binding, WGPUShaderStage visibility)
    {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = binding;
        entry.visibility = visibility;
        entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat; // R32Float, read with textureLoad
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        return entry;
    }
} // namespace

void AvalancheRenderer::init(webgpu::Context& ctx)
{
    m_ctx = &ctx;
    m_config = std::make_unique<webgpu::Buffer<GpuConfig>>(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    m_surface_config = std::make_unique<webgpu::Buffer<SurfaceConfig>>(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);

    auto& reg = ctx.resource_registry();
    reg.register_shader("render_avalanche", "webgpu_engine::render_avalanche");
    reg.register_shader("avalanche_surface_bin", "webgpu_engine::avalanche_surface_bin");
    reg.register_shader("avalanche_surface_depth", "webgpu_engine::avalanche_surface_depth");
    reg.register_shader("render_avalanche_surface", "webgpu_engine::render_avalanche_surface");

    const auto vertex_fragment = WGPUShaderStage(WGPUShaderStage_Vertex | WGPUShaderStage_Fragment);
    reg.register_bind_group_layout("avalanche", [vertex_fragment](WGPUDevice device) {
        return std::make_unique<webgpu::raii::BindGroupLayout>(device,
            std::vector<WGPUBindGroupLayoutEntry> {
                buffer_entry(0, WGPUShaderStage_Vertex, WGPUBufferBindingType_ReadOnlyStorage), // particles
                buffer_entry(1, vertex_fragment, WGPUBufferBindingType_Uniform), // config
            },
            "avalanche renderer bind group layout");
    });
    reg.register_bind_group_layout("avalanche_surface_bin", [](WGPUDevice device) {
        return std::make_unique<webgpu::raii::BindGroupLayout>(device,
            std::vector<WGPUBindGroupLayoutEntry> {
                buffer_entry(0, WGPUShaderStage_Compute, WGPUBufferBindingType_ReadOnlyStorage), // particles
                buffer_entry(1, WGPUShaderStage_Compute, WGPUBufferBindingType_Storage), // columns
                buffer_entry(2, WGPUShaderStage_Compute, WGPUBufferBindingType_Uniform), // surface config
            },
            "avalanche surface binning bind group layout");
    });
    reg.register_bind_group_layout("avalanche_surface_depth", [](WGPUDevice device) {
        return std::make_unique<webgpu::raii::BindGroupLayout>(device,
            std::vector<WGPUBindGroupLayoutEntry> {
                buffer_entry(0, WGPUShaderStage_Compute, WGPUBufferBindingType_ReadOnlyStorage), // columns
                buffer_entry(1, WGPUShaderStage_Compute, WGPUBufferBindingType_Storage), // depth
                height_texture_entry(2, WGPUShaderStage_Compute),
                buffer_entry(3, WGPUShaderStage_Compute, WGPUBufferBindingType_Uniform), // surface config
            },
            "avalanche surface depth bind group layout");
    });
    reg.register_bind_group_layout("avalanche_surface", [vertex_fragment](WGPUDevice device) {
        return std::make_unique<webgpu::raii::BindGroupLayout>(device,
            std::vector<WGPUBindGroupLayoutEntry> {
                buffer_entry(0, WGPUShaderStage_Vertex, WGPUBufferBindingType_ReadOnlyStorage), // depth
                height_texture_entry(1, WGPUShaderStage_Vertex),
                buffer_entry(2, vertex_fragment, WGPUBufferBindingType_Uniform), // surface config
            },
            "avalanche surface bind group layout");
    });

    reg.register_pipeline([this](WGPUDevice dev, const webgpu::RenderResourceRegistry& reg) {
        m_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(dev,
            reg.shader("render_avalanche"),
            reg.shader("render_avalanche"),
            std::vector<webgpu::util::SingleVertexBufferInfo> {}, // vertex pulling from the particle buffer
            gbuffer_format(),
            std::vector<const webgpu::raii::BindGroupLayout*> {
                &reg.bind_group_layout("shared_config"),
                &reg.bind_group_layout("camera"),
                &reg.bind_group_layout("avalanche"),
            });
        m_surface_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(dev,
            reg.shader("render_avalanche_surface"),
            reg.shader("render_avalanche_surface"),
            std::vector<webgpu::util::SingleVertexBufferInfo> {}, // vertex pulling from the column buffer
            gbuffer_format(),
            std::vector<const webgpu::raii::BindGroupLayout*> {
                &reg.bind_group_layout("shared_config"),
                &reg.bind_group_layout("camera"),
                &reg.bind_group_layout("avalanche_surface"),
            });
        m_bin_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(dev, reg.shader("avalanche_surface_bin"),
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("avalanche_surface_bin") }, "avalanche surface binning");
        m_depth_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(dev, reg.shader("avalanche_surface_depth"),
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("avalanche_surface_depth") }, "avalanche surface depth");
        // The layouts may be new.
        m_bind_group.reset();
        m_bin_bind_group.reset();
        m_depth_bind_group.reset();
        m_surface_bind_group.reset();
        m_bound_particles = nullptr;
        m_bound_height = nullptr;
        m_bin_bound_particles = nullptr;
    });
}

void AvalancheRenderer::prepare(WGPUCommandEncoder encoder, const nucleus::camera::Definition& camera)
{
    m_frame.reset();
    m_surface_ready = false;
    if (!settings.enabled || !m_source || m_pipeline == nullptr)
        return;
    m_frame = m_source();
    if (!m_frame || m_frame->particles == nullptr || m_frame->particle_count == 0) {
        m_frame.reset();
        return;
    }
    const Frame& frame = *m_frame;
    const uint32_t count = std::min(frame.particle_count, uint32_t(frame.particles->size() / PARTICLE_STRIDE_U32));

    // Camera-relative placement, computed in double: world coordinates are ~1e6, too large
    // for float once a metre matters.
    const glm::dmat3& m = frame.world_from_metres;
    const glm::vec4 xy_from_metres { float(m[0][0]), float(m[0][1]), float(m[1][0]), float(m[1][1]) };
    const glm::vec2 offset_cws = glm::vec2(glm::dvec2(m[2]) - glm::dvec2(camera.position()));
    const float radius = std::max(settings.radius_scale, 0.01f) * frame.particle_spacing;

    GpuConfig& c = m_config->data;
    c.xy_from_metres = xy_from_metres;
    c.offset_cws = offset_cws;
    c.radius = radius;
    c.colour_mode = uint32_t(settings.colour_mode);
    c.min_pixel_radius = std::max(settings.min_pixel_radius, 0.0f);
    c.speed_max = std::max(settings.speed_max, 1.0f);
    c.particle_count = count;
    c.strain_window = settings.shape == Shape::StrainRate ? std::max(settings.strain_window, 0.0f) : 0.0f;
    m_config->update_gpu_data(m_ctx->queue());

    // The solver reallocates its buffer when the particle count or grid changes.
    if (m_bind_group == nullptr || frame.particles->handle() != m_bound_particles) {
        m_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
            m_ctx->resource_registry().bind_group_layout("avalanche"),
            std::vector<WGPUBindGroupEntry> {
                frame.particles->create_bind_group_entry(0),
                m_config->raw_buffer().create_bind_group_entry(1),
            },
            "avalanche renderer bind group");
        m_bound_particles = frame.particles->handle();
    }

    // Flow surface: bin the particles into the solver's grid columns.
    if (!settings.surface || m_bin_pipeline == nullptr || m_depth_pipeline == nullptr || frame.height_texture == nullptr || frame.columns.x < 2
        || frame.columns.y < 2)
        return;
    const auto& height_texture = frame.height_texture->texture();
    SurfaceConfig& s = m_surface_config->data;
    s.xy_from_metres = xy_from_metres;
    s.offset_cws = offset_cws;
    s.domain_origin = frame.domain_origin;
    s.columns = frame.columns;
    s.dx = frame.dx;
    s.radius = radius;
    s.particle_count = count;
    s.min_count = std::max(settings.surface_min_count, 1u);
    s.colour_mode = uint32_t(settings.colour_mode);
    s.speed_max = std::max(settings.speed_max, 1.0f);
    s.region_size = frame.region_size;
    s.height_dim = glm::uvec2(uint32_t(height_texture.width()), uint32_t(height_texture.height()));
    m_surface_config->update_gpu_data(m_ctx->queue());

    const size_t column_words = size_t(frame.columns.x) * frame.columns.y * COLUMN_STRIDE_U32;
    if (m_columns == nullptr || m_columns->size() != column_words) {
        m_columns = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(),
            WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst), column_words, "avalanche surface columns");
        m_depth = std::make_unique<webgpu::raii::RawBuffer<float>>(m_ctx->device(), WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst),
            size_t(frame.columns.x) * frame.columns.y * 2, "avalanche surface depth");
        m_bin_bind_group.reset();
        m_depth_bind_group.reset();
        m_surface_bind_group.reset();
    }
    if (height_texture.handle() != m_bound_height) {
        m_depth_bind_group.reset();
        m_surface_bind_group.reset();
        m_bound_height = height_texture.handle();
    }
    if (m_bin_bind_group == nullptr || frame.particles->handle() != m_bin_bound_particles) {
        m_bin_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
            m_ctx->resource_registry().bind_group_layout("avalanche_surface_bin"),
            std::vector<WGPUBindGroupEntry> {
                frame.particles->create_bind_group_entry(0),
                m_columns->create_bind_group_entry(1),
                m_surface_config->raw_buffer().create_bind_group_entry(2),
            },
            "avalanche surface binning bind group");
        m_bin_bound_particles = frame.particles->handle();
    }
    if (m_depth_bind_group == nullptr) {
        m_depth_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
            m_ctx->resource_registry().bind_group_layout("avalanche_surface_depth"),
            std::vector<WGPUBindGroupEntry> {
                m_columns->create_bind_group_entry(0),
                m_depth->create_bind_group_entry(1),
                frame.height_texture->texture_view().create_bind_group_entry(2),
                m_surface_config->raw_buffer().create_bind_group_entry(3),
            },
            "avalanche surface depth bind group");
    }
    if (m_surface_bind_group == nullptr) {
        m_surface_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
            m_ctx->resource_registry().bind_group_layout("avalanche_surface"),
            std::vector<WGPUBindGroupEntry> {
                m_depth->create_bind_group_entry(0),
                frame.height_texture->texture_view().create_bind_group_entry(1),
                m_surface_config->raw_buffer().create_bind_group_entry(2),
            },
            "avalanche surface bind group");
    }

    m_columns->clear(encoder);
    WGPUComputePassDescriptor pass_desc {};
    pass_desc.label = WGPUStringView { .data = "avalanche surface binning", .length = WGPU_STRLEN };
    webgpu::raii::ComputePassEncoder pass(encoder, pass_desc);
    wgpuComputePassEncoderSetPipeline(pass.handle(), m_bin_pipeline->handle());
    wgpuComputePassEncoderSetBindGroup(pass.handle(), 0, m_bin_bind_group->handle(), 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass.handle(), (count + BIN_WORKGROUP_SIZE - 1) / BIN_WORKGROUP_SIZE, 1, 1);
    // Dispatches in one pass are ordered and see each other's writes: the depth pass reads the bins.
    wgpuComputePassEncoderSetPipeline(pass.handle(), m_depth_pipeline->handle());
    wgpuComputePassEncoderSetBindGroup(pass.handle(), 0, m_depth_bind_group->handle(), 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass.handle(), (frame.columns.x + 15) / 16, (frame.columns.y + 15) / 16, 1);
    m_surface_ready = true;
}

void AvalancheRenderer::draw(WGPURenderPassEncoder render_pass)
{
    if (!m_frame || m_pipeline == nullptr || m_bind_group == nullptr)
        return;
    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline->pipeline().handle());
    wgpuRenderPassEncoderSetBindGroup(render_pass, 2, m_bind_group->handle(), 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass, QUAD_VERTICES, m_config->data.particle_count, 0, 0);

    if (m_surface_ready && m_surface_pipeline != nullptr && m_surface_bind_group != nullptr) {
        const glm::uvec2 columns = m_surface_config->data.columns;
        wgpuRenderPassEncoderSetPipeline(render_pass, m_surface_pipeline->pipeline().handle());
        wgpuRenderPassEncoderSetBindGroup(render_pass, 2, m_surface_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass, QUAD_VERTICES, (columns.x - 1) * (columns.y - 1), 0, 0);
    }
}

} // namespace webgpu_engine
