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
#include <webgpu/base/raii/PipelineLayout.h>
#include <webgpu/base/raii/RenderPassEncoder.h>

namespace webgpu_engine {

namespace {
    // struct Particle in webgpu/compute/shaders/mpm_common.wgsl: eight vec4s.
    constexpr uint32_t PARTICLE_STRIDE_U32 = 32;
    constexpr uint32_t QUAD_VERTICES = 4; // a triangle strip
    constexpr uint32_t ACCUM_STRIDE_U32 = 3; // avalanche_surface_common.wgsl
    constexpr uint32_t FIELD_STRIDE_F32 = 4;
    constexpr uint32_t SPLAT_WORKGROUP_SIZE = 256;

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
    reg.register_shader("render_avalanche_powder", "webgpu_engine::render_avalanche_powder");
    reg.register_shader("avalanche_surface_splat", "webgpu_engine::avalanche_surface_splat");
    reg.register_shader("avalanche_surface_field", "webgpu_engine::avalanche_surface_field");
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
    reg.register_bind_group_layout("avalanche_surface_splat", [](WGPUDevice device) {
        return std::make_unique<webgpu::raii::BindGroupLayout>(device,
            std::vector<WGPUBindGroupLayoutEntry> {
                buffer_entry(0, WGPUShaderStage_Compute, WGPUBufferBindingType_ReadOnlyStorage), // particles
                buffer_entry(1, WGPUShaderStage_Compute, WGPUBufferBindingType_Storage), // accumulators
                height_texture_entry(2, WGPUShaderStage_Compute),
                buffer_entry(3, WGPUShaderStage_Compute, WGPUBufferBindingType_Uniform), // surface config
            },
            "avalanche surface splat bind group layout");
    });
    reg.register_bind_group_layout("avalanche_surface_field", [](WGPUDevice device) {
        return std::make_unique<webgpu::raii::BindGroupLayout>(device,
            std::vector<WGPUBindGroupLayoutEntry> {
                buffer_entry(0, WGPUShaderStage_Compute, WGPUBufferBindingType_ReadOnlyStorage), // accumulators
                buffer_entry(1, WGPUShaderStage_Compute, WGPUBufferBindingType_Storage), // field
                height_texture_entry(2, WGPUShaderStage_Compute),
                buffer_entry(3, WGPUShaderStage_Compute, WGPUBufferBindingType_Uniform), // surface config
                buffer_entry(4, WGPUShaderStage_Compute, WGPUBufferBindingType_Storage), // peak depth
            },
            "avalanche surface field bind group layout");
    });
    reg.register_bind_group_layout("avalanche_surface", [vertex_fragment](WGPUDevice device) {
        return std::make_unique<webgpu::raii::BindGroupLayout>(device,
            std::vector<WGPUBindGroupLayoutEntry> {
                buffer_entry(0, WGPUShaderStage_Vertex, WGPUBufferBindingType_ReadOnlyStorage), // field
                buffer_entry(1, vertex_fragment, WGPUBufferBindingType_Uniform), // surface config
            },
            "avalanche surface bind group layout");
    });

    reg.register_pipeline([this](WGPUDevice dev, const webgpu::RenderResourceRegistry& reg) {
        // Powder: premultiplied "over" onto the lit frame, no depth attachment (the shader fades
        // against the G-buffer depth itself), like the track lines.
        WGPUBlendState blend_state {};
        blend_state.color.operation = WGPUBlendOperation_Add;
        blend_state.color.srcFactor = WGPUBlendFactor_One;
        blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        blend_state.alpha.operation = WGPUBlendOperation_Add;
        blend_state.alpha.srcFactor = WGPUBlendFactor_Zero;
        blend_state.alpha.dstFactor = WGPUBlendFactor_One;
        WGPUColorTargetState color_target {};
        color_target.blend = &blend_state;
        color_target.writeMask = WGPUColorWriteMask_All;
        color_target.format = m_ctx->surface_texture_format();
        WGPUFragmentState fragment_state {};
        fragment_state.module = reg.shader("render_avalanche_powder").handle();
        fragment_state.entryPoint = WGPUStringView { .data = "fragmentMain", .length = WGPU_STRLEN };
        fragment_state.targetCount = 1;
        fragment_state.targets = &color_target;
        webgpu::raii::PipelineLayout powder_layout(dev,
            std::vector<WGPUBindGroupLayout> {
                reg.bind_group_layout("shared_config").handle(),
                reg.bind_group_layout("camera").handle(),
                reg.bind_group_layout("depth_texture").handle(),
                reg.bind_group_layout("avalanche").handle(),
            });
        WGPURenderPipelineDescriptor powder_desc {};
        powder_desc.label = WGPUStringView { .data = "avalanche powder pipeline", .length = WGPU_STRLEN };
        powder_desc.vertex.module = reg.shader("render_avalanche_powder").handle();
        powder_desc.vertex.entryPoint = WGPUStringView { .data = "vertexMain", .length = WGPU_STRLEN };
        powder_desc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
        powder_desc.primitive.frontFace = WGPUFrontFace_CCW;
        powder_desc.primitive.cullMode = WGPUCullMode_None;
        powder_desc.fragment = &fragment_state;
        powder_desc.multisample.count = 1;
        powder_desc.multisample.mask = ~0u;
        powder_desc.layout = powder_layout.handle();
        m_powder_pipeline = std::make_unique<webgpu::raii::RenderPipeline>(dev, powder_desc);

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
            std::vector<webgpu::util::SingleVertexBufferInfo> {}, // vertex pulling from the field buffer
            gbuffer_format(),
            std::vector<const webgpu::raii::BindGroupLayout*> {
                &reg.bind_group_layout("shared_config"),
                &reg.bind_group_layout("camera"),
                &reg.bind_group_layout("avalanche_surface"),
            });
        m_splat_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(dev, reg.shader("avalanche_surface_splat"),
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("avalanche_surface_splat") }, "avalanche surface splat");
        m_field_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(dev, reg.shader("avalanche_surface_field"),
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("avalanche_surface_field") }, "avalanche surface field");
        // The layouts may be new.
        m_bind_group.reset();
        m_splat_bind_group.reset();
        m_field_bind_group.reset();
        m_surface_bind_group.reset();
        m_bound_particles = nullptr;
        m_bound_height = nullptr;
        m_splat_bound_particles = nullptr;
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
    const float radius = std::max(settings.radius_scale, 0.01f) * frame.particle_spacing; // solid particles, also the surface's
    const bool powder = settings.shape == Shape::Powder;

    GpuConfig& c = m_config->data;
    c.xy_from_metres = xy_from_metres;
    c.offset_cws = offset_cws;
    c.radius = powder ? std::max(settings.powder_size, 0.1f) * frame.particle_spacing : radius;
    c.colour_mode = uint32_t(settings.colour_mode);
    c.min_pixel_radius = std::max(settings.min_pixel_radius, 0.0f);
    c.speed_max = std::max(settings.speed_max, 1.0f);
    c.particle_count = count;
    c.strain_window = settings.shape == Shape::StrainRate ? std::max(settings.strain_window, 0.0f) : 0.0f;
    c.children = powder ? 1u : std::clamp(settings.children, 1u, 64u);
    c.opacity = std::clamp(settings.powder_opacity, 0.0f, 1.0f);
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

    // Flow surface: splat the particles onto the render grid, then the depth field from it.
    if (!settings.surface || m_splat_pipeline == nullptr || m_field_pipeline == nullptr || frame.height_texture == nullptr || frame.columns.x < 2
        || frame.columns.y < 2)
        return;
    const auto& height_texture = frame.height_texture->texture();
    const uint32_t refine = std::clamp(settings.surface_refine, 1u, 4u);
    const float volume = frame.particle_spacing * frame.particle_spacing * frame.particle_spacing;
    SurfaceConfig& s = m_surface_config->data;
    s.xy_from_metres = xy_from_metres;
    s.offset_cws = offset_cws;
    s.domain_origin = frame.domain_origin;
    s.nodes = (frame.columns - 1u) * refine + 1u;
    s.spacing = frame.dx / float(refine);
    s.kernel = frame.dx;
    s.particle_count = count;
    s.colour_mode = uint32_t(settings.colour_mode);
    s.speed_max = std::max(settings.speed_max, 1.0f);
    s.volume = volume;
    s.min_depth = std::max(float(std::max(settings.surface_min_count, 1u)) * volume / (9.0f * frame.dx * frame.dx), 0.05f);
    s.clearance = 2.0f * frame.dx;
    s.region_size = frame.region_size;
    s.height_dim = glm::uvec2(uint32_t(height_texture.width()), uint32_t(height_texture.height()));
    s.radius = radius;
    s.track = settings.track ? 1u : 0u;
    m_surface_config->update_gpu_data(m_ctx->queue());

    const size_t node_count = size_t(s.nodes.x) * s.nodes.y;
    if (m_accum == nullptr || m_accum->size() != node_count * ACCUM_STRIDE_U32) {
        m_accum = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(),
            WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst), node_count * ACCUM_STRIDE_U32, "avalanche surface accumulators");
        m_field = std::make_unique<webgpu::raii::RawBuffer<float>>(m_ctx->device(), WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst),
            node_count * FIELD_STRIDE_F32, "avalanche surface field");
        m_peak = std::make_unique<webgpu::raii::RawBuffer<float>>(m_ctx->device(), WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst),
            node_count, "avalanche surface peak depth");
        m_peak->clear(encoder);
        m_peak_reset_count = frame.reset_count;
        m_splat_bind_group.reset();
        m_field_bind_group.reset();
        m_surface_bind_group.reset();
    }
    if (height_texture.handle() != m_bound_height) {
        m_splat_bind_group.reset();
        m_field_bind_group.reset();
        m_bound_height = height_texture.handle();
    }
    if (m_splat_bind_group == nullptr || frame.particles->handle() != m_splat_bound_particles) {
        m_splat_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
            m_ctx->resource_registry().bind_group_layout("avalanche_surface_splat"),
            std::vector<WGPUBindGroupEntry> {
                frame.particles->create_bind_group_entry(0),
                m_accum->create_bind_group_entry(1),
                frame.height_texture->texture_view().create_bind_group_entry(2),
                m_surface_config->raw_buffer().create_bind_group_entry(3),
            },
            "avalanche surface splat bind group");
        m_splat_bound_particles = frame.particles->handle();
    }
    if (m_field_bind_group == nullptr) {
        m_field_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
            m_ctx->resource_registry().bind_group_layout("avalanche_surface_field"),
            std::vector<WGPUBindGroupEntry> {
                m_accum->create_bind_group_entry(0),
                m_field->create_bind_group_entry(1),
                frame.height_texture->texture_view().create_bind_group_entry(2),
                m_surface_config->raw_buffer().create_bind_group_entry(3),
                m_peak->create_bind_group_entry(4),
            },
            "avalanche surface field bind group");
    }
    if (m_surface_bind_group == nullptr) {
        m_surface_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
            m_ctx->resource_registry().bind_group_layout("avalanche_surface"),
            std::vector<WGPUBindGroupEntry> {
                m_field->create_bind_group_entry(0),
                m_surface_config->raw_buffer().create_bind_group_entry(1),
            },
            "avalanche surface bind group");
    }

    if (frame.reset_count != m_peak_reset_count) { // a new release: the old track goes
        m_peak->clear(encoder);
        m_peak_reset_count = frame.reset_count;
    }
    m_accum->clear(encoder);
    WGPUComputePassDescriptor pass_desc {};
    pass_desc.label = WGPUStringView { .data = "avalanche surface splat", .length = WGPU_STRLEN };
    webgpu::raii::ComputePassEncoder pass(encoder, pass_desc);
    wgpuComputePassEncoderSetPipeline(pass.handle(), m_splat_pipeline->handle());
    wgpuComputePassEncoderSetBindGroup(pass.handle(), 0, m_splat_bind_group->handle(), 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass.handle(), (count + SPLAT_WORKGROUP_SIZE - 1) / SPLAT_WORKGROUP_SIZE, 1, 1);
    // Dispatches in one pass are ordered and see each other's writes: the field pass reads the splat.
    wgpuComputePassEncoderSetPipeline(pass.handle(), m_field_pipeline->handle());
    wgpuComputePassEncoderSetBindGroup(pass.handle(), 0, m_field_bind_group->handle(), 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass.handle(), (s.nodes.x + 15) / 16, (s.nodes.y + 15) / 16, 1);
    m_surface_ready = true;
}

void AvalancheRenderer::draw(WGPURenderPassEncoder render_pass)
{
    if (!m_frame || m_pipeline == nullptr || m_bind_group == nullptr)
        return;
    if (settings.shape != Shape::Powder) {
        wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline->pipeline().handle());
        wgpuRenderPassEncoderSetBindGroup(render_pass, 2, m_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass, QUAD_VERTICES, m_config->data.particle_count * m_config->data.children, 0, 0);
    }

    if (m_surface_ready && m_surface_pipeline != nullptr && m_surface_bind_group != nullptr) {
        const glm::uvec2 nodes = m_surface_config->data.nodes;
        wgpuRenderPassEncoderSetPipeline(render_pass, m_surface_pipeline->pipeline().handle());
        wgpuRenderPassEncoderSetBindGroup(render_pass, 2, m_surface_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass, QUAD_VERTICES, (nodes.x - 1) * (nodes.y - 1), 0, 0);
    }
}

void AvalancheRenderer::draw_transparent(WGPUCommandEncoder encoder,
    const webgpu::raii::BindGroup& shared_config,
    const webgpu::raii::BindGroup& camera,
    const webgpu::raii::BindGroup& depth_texture,
    const webgpu::raii::TextureView& color_texture)
{
    if (settings.shape != Shape::Powder || !m_frame || m_powder_pipeline == nullptr || m_bind_group == nullptr)
        return;
    WGPURenderPassColorAttachment color_attachment {};
    color_attachment.view = color_texture.handle();
    color_attachment.loadOp = WGPULoadOp_Load;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    WGPURenderPassDescriptor pass_desc {};
    pass_desc.label = WGPUStringView { .data = "avalanche powder render pass", .length = WGPU_STRLEN };
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attachment;

    webgpu::raii::RenderPassEncoder pass(encoder, pass_desc);
    wgpuRenderPassEncoderSetPipeline(pass.handle(), m_powder_pipeline->handle());
    wgpuRenderPassEncoderSetBindGroup(pass.handle(), 0, shared_config.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(pass.handle(), 1, camera.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(pass.handle(), 2, depth_texture.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(pass.handle(), 3, m_bind_group->handle(), 0, nullptr);
    wgpuRenderPassEncoderDraw(pass.handle(), QUAD_VERTICES, m_config->data.particle_count, 0, 0);
}

} // namespace webgpu_engine
