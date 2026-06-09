/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "TrackRenderer.h"

#include "nucleus/srs.h"
#include "nucleus/track/GPX.h"
#include <QString>
#include <webgpu/RenderResourceRegistry.h>
#include <webgpu/raii/BindGroupLayout.h>
#include <webgpu/raii/PipelineLayout.h>

namespace webgpu_engine {

TrackRenderer::TrackRenderer()
    : QObject { nullptr }
    , m_ctx { nullptr }
{
}

radix::geometry::Aabb3d TrackRenderer::load_track(const std::string& path)
{
    std::unique_ptr<nucleus::track::Gpx> gpx_track = nucleus::track::parse(QString::fromStdString(path));

    std::vector<glm::dvec3> points;
    for (const auto& segment : gpx_track->track) {
        points.reserve(points.size() + segment.size());
        for (const auto& point : segment) {
            points.push_back({ point.latitude, point.longitude, point.elevation });
        }
    }
    add_track(points);

    return nucleus::track::compute_world_aabb(*gpx_track);
}

void TrackRenderer::init(webgpu::Context& ctx)
{
    m_ctx = &ctx;

    auto& reg = ctx.resource_registry();
    reg.register_shader("render_lines", "render_lines.wgsl");
    reg.register_bind_group_layout("lines", [](WGPUDevice device) {
        WGPUBindGroupLayoutEntry input_positions_entry {};
        input_positions_entry.binding = 0;
        input_positions_entry.visibility = WGPUShaderStage_Vertex;
        input_positions_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        input_positions_entry.buffer.minBindingSize = 0;

        WGPUBindGroupLayoutEntry input_config_entry {};
        input_config_entry.binding = 1;
        input_config_entry.visibility = WGPUShaderStage_Fragment;
        input_config_entry.buffer.type = WGPUBufferBindingType_Uniform;
        input_config_entry.buffer.minBindingSize = 0;

        return std::make_unique<webgpu::raii::BindGroupLayout>(
            device, std::vector<WGPUBindGroupLayoutEntry> { input_positions_entry, input_config_entry }, "line renderer, bind group layout");
    });
    reg.register_pipeline([this](WGPUDevice dev, const webgpu::RenderResourceRegistry& reg) {
        WGPUBlendState blend_state {};
        blend_state.color.operation = WGPUBlendOperation_Add;
        blend_state.color.srcFactor = WGPUBlendFactor_One;
        blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        blend_state.alpha.operation = WGPUBlendOperation_Add;
        blend_state.alpha.srcFactor = WGPUBlendFactor_Zero;
        blend_state.alpha.dstFactor = WGPUBlendFactor_One;

        WGPUColorTargetState color_target_state {};
        color_target_state.blend = &blend_state;
        color_target_state.writeMask = WGPUColorWriteMask_All;
        color_target_state.format = WGPUTextureFormat_BGRA8Unorm;

        WGPUFragmentState fragment_state {};
        fragment_state.module = reg.shader("render_lines").handle();
        fragment_state.entryPoint = WGPUStringView { .data = "fragmentMain", .length = WGPU_STRLEN };
        fragment_state.constantCount = 0;
        fragment_state.constants = nullptr;
        fragment_state.targetCount = 1;
        fragment_state.targets = &color_target_state;

        std::vector<WGPUBindGroupLayout> bind_group_layout_handles {
            reg.bind_group_layout("shared_config").handle(),
            reg.bind_group_layout("camera").handle(),
            reg.bind_group_layout("depth_texture").handle(),
            reg.bind_group_layout("lines").handle(),
        };
        webgpu::raii::PipelineLayout layout(dev, bind_group_layout_handles);

        WGPURenderPipelineDescriptor pipeline_desc {};
        pipeline_desc.label = WGPUStringView { .data = "line render pipeline", .length = WGPU_STRLEN };
        pipeline_desc.vertex.module = reg.shader("render_lines").handle();
        pipeline_desc.vertex.entryPoint = WGPUStringView { .data = "vertexMain", .length = WGPU_STRLEN };
        pipeline_desc.vertex.bufferCount = 0;
        pipeline_desc.vertex.buffers = nullptr;
        pipeline_desc.vertex.constantCount = 0;
        pipeline_desc.vertex.constants = nullptr;
        pipeline_desc.primitive.topology = WGPUPrimitiveTopology::WGPUPrimitiveTopology_LineStrip;
        pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat::WGPUIndexFormat_Uint16;
        pipeline_desc.primitive.frontFace = WGPUFrontFace::WGPUFrontFace_CCW;
        pipeline_desc.primitive.cullMode = WGPUCullMode::WGPUCullMode_None;
        pipeline_desc.fragment = &fragment_state;
        pipeline_desc.depthStencil = nullptr;
        pipeline_desc.multisample.count = 1;
        pipeline_desc.multisample.mask = ~0u;
        pipeline_desc.multisample.alphaToCoverageEnabled = false;
        pipeline_desc.layout = layout.handle();

        m_pipeline = std::make_unique<webgpu::raii::RenderPipeline>(dev, pipeline_desc);
    });
}

void TrackRenderer::add_track(const Track& track, const glm::vec4& color)
{
    std::vector<glm::fvec4> gpu_points;
    gpu_points.reserve(track.size());
    for (const glm::dvec3& coords : track) {
        gpu_points.push_back(glm::fvec4(nucleus::srs::lat_long_alt_to_world(coords), 1));
    }
    add_world_positions(gpu_points, color);
}

void TrackRenderer::add_world_positions(const std::vector<glm::vec4>& world_positions, const glm::vec4& color)
{
    assert(!world_positions.empty());

    m_position_buffers.emplace_back(std::make_unique<webgpu::raii::RawBuffer<glm::fvec4>>(
        m_ctx->device(), WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, world_positions.size(), "track renderer, storage buffer for points"));
    m_position_buffers.back()->write(m_ctx->queue(), world_positions.data(), world_positions.size());

    m_line_config_buffers.emplace_back(std::make_unique<webgpu::Buffer<LineConfig>>(m_ctx->device(), WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst));
    m_line_config_buffers.back()->data.line_color = color;
    m_line_config_buffers.back()->update_gpu_data(m_ctx->queue());

    m_bind_groups.emplace_back(std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(),
        m_ctx->resource_registry().bind_group_layout("lines"),
        std::initializer_list<WGPUBindGroupEntry> {
            m_position_buffers.back()->create_bind_group_entry(0), m_line_config_buffers.back()->raw_buffer().create_bind_group_entry(1) }));
}

void TrackRenderer::render(WGPUCommandEncoder command_encoder,
    const webgpu::raii::BindGroup& shared_config,
    const webgpu::raii::BindGroup& camera_config,
    const webgpu::raii::BindGroup& depth_texture,
    const webgpu::raii::TextureView& color_texture)
{
    WGPURenderPassColorAttachment color_attachment {};
    color_attachment.view = color_texture.handle();
    color_attachment.resolveTarget = nullptr;
    color_attachment.loadOp = WGPULoadOp::WGPULoadOp_Load;
    color_attachment.storeOp = WGPUStoreOp::WGPUStoreOp_Store;
    color_attachment.clearValue = WGPUColor { 0.0, 0.0, 0.0, 0.0 };
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    /*WGPURenderPassDepthStencilAttachment depth_stencil_attachment {};
    depth_stencil_attachment.view = depth_texture_view.handle();
    depth_stencil_attachment.depthLoadOp = WGPULoadOp_Undefined;
    depth_stencil_attachment.depthStoreOp = WGPUStoreOp_Undefined;
    depth_stencil_attachment.depthReadOnly = true;
    depth_stencil_attachment.stencilLoadOp = WGPULoadOp::WGPULoadOp_Undefined;
    depth_stencil_attachment.stencilStoreOp = WGPUStoreOp::WGPUStoreOp_Undefined;
    depth_stencil_attachment.stencilReadOnly = true;*/

    WGPURenderPassDescriptor render_pass_descriptor {};
    render_pass_descriptor.label = WGPUStringView { .data = "line render render pass", .length = WGPU_STRLEN };
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;
    render_pass_descriptor.depthStencilAttachment = nullptr;
    render_pass_descriptor.timestampWrites = nullptr;

    auto render_pass = webgpu::raii::RenderPassEncoder(command_encoder, render_pass_descriptor);
    wgpuRenderPassEncoderSetPipeline(render_pass.handle(), m_pipeline->handle());
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 0, shared_config.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 1, camera_config.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 2, depth_texture.handle(), 0, nullptr);
    for (size_t i = 0; i < m_bind_groups.size(); i++) {
        wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 3, m_bind_groups.at(i)->handle(), 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass.handle(), uint32_t(m_position_buffers.at(i)->size()), 1, 0, 0);
    }
}

} // namespace webgpu_engine
