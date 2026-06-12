/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2025 Markus Rampp
 * Copyright (C) 2026 Wendelin Muth
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

#include "Window.h"
#include "nucleus/tile/drawing.h"
#include "overlay/OverlayRenderer.h"
#include "webgpu/base/raii/RenderPassEncoder.h"
#include "webgpu/engine/Context.h"
#include <ktx.h>
#include <webgpu/base/RenderResourceRegistry.h>
#include <webgpu/base/util/VertexBufferInfo.h>

#include <webgpu/webgpu.h>

namespace webgpu_engine {

Window::Window() { }

Window::~Window() { }

void Window::set_context(Context* context)
{
    m_context = context;
    connect(m_context, &Context::redraw_requested, this, &Window::request_redraw);
}

void Window::initialise_gpu()
{
    assert(m_context != nullptr); // just make sure that context is set

    create_buffers();

    auto& reg = m_context->webgpu_ctx().resource_registry();
    reg.register_shader("compose_pass", "webgpu_engine::compose_pass");
    reg.register_pipeline([this](WGPUDevice dev, const webgpu::RenderResourceRegistry& reg) {
        webgpu::FramebufferFormat format {};
        format.depth_format = WGPUTextureFormat_Depth24Plus;
        format.color_formats.emplace_back(WGPUTextureFormat_BGRA8Unorm);
        m_compose_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(dev,
            reg.shader("compose_pass"),
            reg.shader("compose_pass"),
            std::vector<webgpu::util::SingleVertexBufferInfo> {},
            format,
            std::vector<const webgpu::raii::BindGroupLayout*> {
                &reg.bind_group_layout("shared_config"),
                &reg.bind_group_layout("camera"),
                &reg.bind_group_layout("compose"),
            });
    });

    m_context->webgpu_ctx().resource_registry().recreate_all(m_context->webgpu_ctx().device());

    create_bind_groups();

    m_shadow_texture = create_shadow_texture(1, 1, 1);
}

std::unique_ptr<webgpu::raii::TextureWithSampler> Window::create_shadow_texture(uint32_t width, uint32_t height, uint32_t mip_levels)
{
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "shadow texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = mip_levels;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat::WGPUTextureFormat_R16Float;
    texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "shadow sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.maxAnisotropy = 1.0;

    return std::make_unique<webgpu::raii::TextureWithSampler>(m_context->webgpu_ctx().device(), texture_desc, sampler_desc);
}

void Window::on_shadow_texture_updated(const QByteArray& data)
{
    ktxTexture* ktx_texture;
    KTX_error_code result = ktxTexture_CreateFromMemory(
        reinterpret_cast<const ktx_uint8_t*>(data.constData()), data.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);

    if (result != KTX_SUCCESS) {
        qWarning() << "Failed to create ktx texture from memory";
        return;
    }

    m_shadow_texture = create_shadow_texture(ktx_texture->baseWidth, ktx_texture->baseHeight, ktx_texture->numLevels);

    size_t level_0_size = ktxTexture_GetLevelSize(ktx_texture, 0);
    size_t level_0_offset = 0;
    ktxTexture_GetImageOffset(ktx_texture, 0, 0, 0, &level_0_offset);
    std::span byte_span { ktxTexture_GetData(ktx_texture) + level_0_offset, level_0_size };

    WGPUTexelCopyTextureInfo image_copy_texture {};
    image_copy_texture.texture = m_shadow_texture->texture().handle();
    image_copy_texture.aspect = WGPUTextureAspect::WGPUTextureAspect_All;
    image_copy_texture.mipLevel = 0;
    image_copy_texture.origin = WGPUOrigin3D { 0, 0, 0 };

    WGPUTexelCopyBufferLayout texture_data_layout {};
    texture_data_layout.bytesPerRow = 2 * ktx_texture->baseWidth;
    texture_data_layout.rowsPerImage = ktx_texture->baseHeight;
    texture_data_layout.offset = 0;

    WGPUExtent3D copy_extent { ktx_texture->baseWidth, ktx_texture->baseHeight, 1 };
    wgpuQueueWriteTexture(m_context->webgpu_ctx().queue(), &image_copy_texture, byte_span.data(), byte_span.size_bytes(), &texture_data_layout, &copy_extent);

    ktxTexture_Destroy(ktx_texture);

    recreate_compose_bind_group();
}

void Window::resize_framebuffer(int w, int h)
{
    m_swapchain_size = glm::vec2(w, h);

    m_gbuffer_format = webgpu::FramebufferFormat(m_context->tile_mesh_renderer()->render_tiles_pipeline().framebuffer_format());
    m_gbuffer_format.size = glm::uvec2 { w, h };
    m_gbuffer = std::make_unique<webgpu::Framebuffer>(m_context->webgpu_ctx().device(), m_gbuffer_format);

    m_context->atmosphere_renderer()->resize(w, h);

    m_depth_texture_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_context->webgpu_ctx().device(),
        m_context->webgpu_ctx().resource_registry().bind_group_layout("depth_texture"),
        std::initializer_list<WGPUBindGroupEntry> {
            m_gbuffer->depth_texture_view().create_bind_group_entry(0), // depth
        });

    m_context->cloud_renderer()->resize(w, h);
    m_context->overlay_renderer()->resize(w, h);

    recreate_compose_bind_group(); // Do late
}

void Window::paint(webgpu::Framebuffer* framebuffer, WGPUCommandEncoder command_encoder)
{
    m_needs_redraw = false;

    // ToDo only update on change?
    m_shared_config_ubo->data = m_context->shared_config();
    m_shared_config_ubo->update_gpu_data(m_context->webgpu_ctx().queue());

    // render atmosphere to color buffer
    m_context->atmosphere_renderer()->draw(command_encoder, m_camera_bind_group->handle());

    // render tiles to geometry buffers
    {
        std::unique_ptr<webgpu::raii::RenderPassEncoder> render_pass = m_gbuffer->begin_render_pass(command_encoder);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, m_shared_config_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 1, m_camera_bind_group->handle(), 0, nullptr);

        using namespace nucleus::tile;
        const auto draw_list = drawing::compute_bounds(
            drawing::limit(drawing::generate_list(m_camera, m_context->aabb_decorator(), m_max_zoom_level), 1024), m_context->aabb_decorator());
        const auto culled_draw_list = drawing::sort(drawing::cull(draw_list, m_camera), m_camera.position());

        m_context->tile_mesh_renderer()->draw(render_pass->handle(), m_camera, culled_draw_list);
    }

    // render clouds
    if (m_context->shared_config().m_clouds_enabled) {
        m_context->cloud_renderer()->draw(
            command_encoder, m_depth_texture_bind_group->handle(), m_shared_config_bind_group->handle(), m_camera, m_paint_number);
        m_needs_redraw |= m_context->cloud_renderer()->needs_redraw(); // Repaint for TAAU
    }

    // render overlay textures (height lines, tile debug, etc.)
    m_context->overlay_renderer()->draw(command_encoder,
        m_gbuffer->color_texture_view(1),
        m_gbuffer->color_texture_view(2),
        m_gbuffer->color_texture_view(3),
        m_shared_config_bind_group->handle(),
        m_camera_bind_group->handle());

    // render geometry buffers to target framebuffer
    {
        std::unique_ptr<webgpu::raii::RenderPassEncoder> render_pass = framebuffer->begin_render_pass(command_encoder);
        wgpuRenderPassEncoderSetPipeline(render_pass->handle(), m_compose_pipeline->pipeline().handle());
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, m_shared_config_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 1, m_camera_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 2, m_compose_bind_groups[m_paint_number % 2]->handle(), 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass->handle(), 3, 1, 0, 0);
    }

    // render lines to color buffer
    if (m_context->shared_config().m_track_render_mode > 0) {
        m_context->track_renderer()->render(
            command_encoder, *m_shared_config_bind_group, *m_camera_bind_group, *m_depth_texture_bind_group, framebuffer->color_texture_view(0));
    }

    m_paint_number++;
}

glm::vec4 Window::synchronous_position_readback(const glm::dvec2& ndc)
{
    if (m_position_readback_buffer->map_state() == WGPUBufferMapState_Unmapped) {
        // A little bit silly, but we have to transform it back to device coordinates
        glm::uvec2 device_coordinates = { (ndc.x + 1) * 0.5 * m_swapchain_size.x, (1 - (ndc.y + 1) * 0.5) * m_swapchain_size.y };

        // clamp device coordinates to the swapchain size
        device_coordinates = glm::clamp(device_coordinates, glm::uvec2(0), glm::uvec2(m_swapchain_size - glm::vec2(1.0)));

        const auto& src_texture = m_gbuffer->color_texture(1);
        // Need to read a multiple of 16 values to fit requirement for texture_to_buffer copy
        src_texture.copy_to_buffer(
            m_context->webgpu_ctx().device(), *m_position_readback_buffer.get(), glm::uvec3(device_coordinates.x, device_coordinates.y, 0), glm::uvec2(16, 1));

        std::vector<glm::vec4> pos_buffer;
        WGPUMapAsyncStatus result
            = m_position_readback_buffer->read_back_sync(m_context->webgpu_ctx().instance(), m_context->webgpu_ctx().device(), pos_buffer);
        if (result == WGPUMapAsyncStatus_Success) {
            m_last_position_readback = pos_buffer[0];
        }
    } // else qDebug() << "Dropped position readback request, buffer still mapping.";

    // qDebug() << "Position:" << glm::to_string(m_last_position_readback);
    return m_last_position_readback;
}

void Window::set_max_zoom_level(uint32_t max_zoom_level) { m_max_zoom_level = max_zoom_level; }

float Window::depth([[maybe_unused]] const glm::dvec2& normalised_device_coordinates)
{
    auto position = synchronous_position_readback(normalised_device_coordinates);
    return position.z;
}

glm::dvec3 Window::position([[maybe_unused]] const glm::dvec2& normalised_device_coordinates)
{
    // If we read position directly no reconstruction is necessary
    // glm::dvec3 reconstructed = m_camera.position() + m_camera.ray_direction(normalised_device_coordinates) * (double)depth(normalised_device_coordinates);
    auto position = synchronous_position_readback(normalised_device_coordinates);
    return m_camera.position() + glm::dvec3(position.x, position.y, position.z);
}

void Window::destroy() { }

// Return this object as the depth tester
nucleus::camera::AbstractDepthTester* Window::depth_tester() { return this; }

nucleus::utils::ColourTexture::Format Window::ortho_tile_compression_algorithm() const
{
    // TODO use compressed textures in the future
    return nucleus::utils::ColourTexture::Format::Uncompressed_RGBA;
}

void Window::update_camera([[maybe_unused]] const nucleus::camera::Definition& new_definition)
{
    // NOTE: Could also just be done on camera or viewport change!
    uboCameraConfig* cc = &m_camera_config_ubo->data;
    cc->position = glm::vec4(new_definition.position(), 1.0);
    cc->view_matrix = new_definition.local_view_matrix();
    cc->proj_matrix = new_definition.projection_matrix();
    cc->view_proj_matrix = cc->proj_matrix * cc->view_matrix;
    cc->inv_view_proj_matrix = glm::inverse(cc->view_proj_matrix);
    cc->inv_view_matrix = glm::inverse(cc->view_matrix);
    cc->inv_proj_matrix = glm::inverse(cc->proj_matrix);
    cc->viewport_size = new_definition.viewport_size();
    cc->distance_scaling_factor = new_definition.distance_scale_factor();
    m_camera_config_ubo->update_gpu_data(m_context->webgpu_ctx().queue());
    m_camera = new_definition;

    emit update_requested();
}

void Window::request_redraw() { m_needs_redraw = true; }

void Window::ready() { m_context->overlay_renderer()->ready(m_context->webgpu_ctx()); }

void Window::reload_shaders()
{
    m_context->webgpu_ctx().resource_registry().recreate_all(m_context->webgpu_ctx().device());
    recreate_compose_bind_group();
    qDebug() << "reloading shaders done";
    request_redraw();
}

void Window::create_buffers()
{
    m_shared_config_ubo
        = std::make_unique<webgpu::Buffer<uboSharedConfig>>(m_context->webgpu_ctx().device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    m_camera_config_ubo
        = std::make_unique<webgpu::Buffer<uboCameraConfig>>(m_context->webgpu_ctx().device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    m_position_readback_buffer = std::make_unique<webgpu::raii::RawBuffer<glm::vec4>>(
        m_context->webgpu_ctx().device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, 256 / sizeof(glm::vec4), "position readback buffer");
}

void Window::create_bind_groups()
{
    m_shared_config_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_context->webgpu_ctx().device(),
        m_context->webgpu_ctx().resource_registry().bind_group_layout("shared_config"),
        std::initializer_list<WGPUBindGroupEntry> { m_shared_config_ubo->raw_buffer().create_bind_group_entry(0) });

    m_camera_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_context->webgpu_ctx().device(),
        m_context->webgpu_ctx().resource_registry().bind_group_layout("camera"),
        std::initializer_list<WGPUBindGroupEntry> { m_camera_config_ubo->raw_buffer().create_bind_group_entry(0) });
}

void Window::recreate_compose_bind_group()
{
    for (int i = 0; i < 2; ++i) {
        m_compose_bind_groups[i] = std::make_unique<webgpu::raii::BindGroup>(m_context->webgpu_ctx().device(),
            m_context->webgpu_ctx().resource_registry().bind_group_layout("compose"),
            std::initializer_list<WGPUBindGroupEntry> {
                m_gbuffer->color_texture_view(0).create_bind_group_entry(0), // albedo texture
                m_gbuffer->color_texture_view(1).create_bind_group_entry(1), // position texture
                m_gbuffer->color_texture_view(2).create_bind_group_entry(2), // normal texture
                m_context->atmosphere_renderer()->result_view()->create_bind_group_entry(3), // atmosphere texture
                m_gbuffer->color_texture_view(3).create_bind_group_entry(4), // overlay texture
                m_context->cloud_renderer()->result_color_view(i)->create_bind_group_entry(5),
                m_context->cloud_renderer()->result_depth_view()->create_bind_group_entry(6),
                m_shadow_texture->texture_view().create_bind_group_entry(7),
                m_shadow_texture->sampler().create_bind_group_entry(8),
                m_gbuffer->depth_texture_view().create_bind_group_entry(9),
                m_context->overlay_renderer()->result_post_view()->create_bind_group_entry(10), // overlay post-shading output
                m_context->overlay_renderer()->result_pre_view()->create_bind_group_entry(11), // overlay pre-shading output
            });
    }
}

void Window::update_required_gpu_limits(WGPULimits& limits, const WGPULimits& supported_limits)
{
    const uint32_t max_required_bind_groups = 4u;
    const uint32_t min_recommended_max_texture_array_layers = 1024u;
    const uint32_t min_required_max_color_attachment_bytes_per_sample = 32u;
    const uint64_t min_required_max_storage_buffer_binding_size = 268435456u;

    if (supported_limits.maxColorAttachmentBytesPerSample < min_required_max_color_attachment_bytes_per_sample) {
        qFatal() << "Minimum supported maxColorAttachmentBytesPerSample needs to be >=" << min_required_max_color_attachment_bytes_per_sample;
    }
    if (supported_limits.maxTextureArrayLayers < min_recommended_max_texture_array_layers) {
        qWarning() << "Minimum supported maxTextureArrayLayers is " << supported_limits.maxTextureArrayLayers << " ("
                   << min_recommended_max_texture_array_layers << " recommended)!";
    }
    if (supported_limits.maxBindGroups < max_required_bind_groups) {
        qFatal() << "Maximum supported number of bind groups is " << supported_limits.maxBindGroups << " and " << max_required_bind_groups << " are required";
    }
    if (supported_limits.maxStorageBufferBindingSize < min_required_max_storage_buffer_binding_size) {
        qFatal() << "Maximum supported storage buffer binding size is " << supported_limits.maxStorageBufferBindingSize << " and "
                 << min_required_max_storage_buffer_binding_size << " is required";
    }
    limits.maxBindGroups = std::max(limits.maxBindGroups, max_required_bind_groups);
    limits.maxColorAttachmentBytesPerSample = std::max(limits.maxColorAttachmentBytesPerSample, min_required_max_color_attachment_bytes_per_sample);
    limits.maxTextureArrayLayers
        = std::min(std::max(limits.maxTextureArrayLayers, min_recommended_max_texture_array_layers), supported_limits.maxTextureArrayLayers);
    limits.maxStorageBufferBindingSize = std::max(limits.maxStorageBufferBindingSize, supported_limits.maxStorageBufferBindingSize);
}

} // namespace webgpu_engine
