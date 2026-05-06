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
#include "compute/nodes/BufferToTextureNode.h"
#include "compute/nodes/ComputeAvalancheTrajectoriesNode.h"
#include "compute/nodes/ComputeReleasePointsNode.h"
#include "compute/nodes/ComputeSnowNode.h"
#include "compute/nodes/LoadRegionAabbNode.h"
#include "compute/nodes/SelectTilesNode.h"
#include "gpu_utils.h"
#include "nucleus/tile/drawing.h"
#include "nucleus/track/GPX.h"
#include "nucleus/utils/image_loader.h"
#include "webgpu/raii/RenderPassEncoder.h"
#include "webgpu_engine/Context.h"
#include <ktx.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <webgpu_app/WebInterop.h>
#endif
#include <webgpu/webgpu.h>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include "imgui.h"
#include "imgui_internal.h"
#ifndef __EMSCRIPTEN__
#include "ImGuiFileDialog.h"
#endif
// TODO: Remove ImGuiFileDialog dependency on Web-build

#include <IconsFontAwesome5.h>
#endif

namespace webgpu_engine {

Window::Window()
{
#ifdef __EMSCRIPTEN__
    connect(&WebInterop::instance(), &WebInterop::file_uploaded, this, &Window::file_upload_handler);
#endif
}

Window::~Window()
{
    // Destructor cleanup logic here
}

void Window::set_wgpu_context(WGPUInstance instance, WGPUDevice device, WGPUAdapter adapter, WGPUSurface surface, WGPUQueue queue, Context* context)
{
    m_instance = instance;
    m_device = device;
    m_adapter = adapter;
    m_surface = surface;
    m_queue = queue;
    m_context = context;
    connect(m_context, &Context::redraw_requested, this, &Window::request_redraw);
}

void Window::initialise_gpu()
{
    assert(m_device != nullptr); // just make sure that wgpu context is set

    create_buffers();
    create_bind_groups();

    m_track_renderer = std::make_unique<TrackRenderer>(m_device, *m_context->pipeline_manager());

    m_image_overlay_settings_uniform_buffer = std::make_unique<Buffer<ImageOverlaySettings>>(m_device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    m_image_overlay_settings_uniform_buffer->data.aabb_min = glm::fvec2(0);
    m_image_overlay_settings_uniform_buffer->data.aabb_max = glm::fvec2(0);
    m_image_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
    m_image_overlay_texture = create_overlay_texture(1, 1);

    m_compute_overlay_settings_uniform_buffer = std::make_unique<Buffer<ImageOverlaySettings>>(m_device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    m_compute_overlay_settings_uniform_buffer->data.aabb_min = glm::fvec2(0);
    m_compute_overlay_settings_uniform_buffer->data.aabb_max = glm::fvec2(0);
    m_compute_overlay_settings_uniform_buffer->data.mode = 0u;
    m_compute_overlay_settings_uniform_buffer->data.alpha = 1.0f;
    m_compute_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
    m_compute_overlay_dummy_texture = create_overlay_texture(1, 1);

    m_shadow_texture = create_shadow_texture(1, 1, 1);

    create_and_set_compute_pipeline(ComputePipelineType::AVALANCHE_TRAJECTORIES, false);

    qInfo() << "gpu_ready_changed";
    // emit gpu_ready_changed(true); //TODO remove/find replacement
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

    return std::make_unique<webgpu::raii::TextureWithSampler>(m_device, texture_desc, sampler_desc);
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
    wgpuQueueWriteTexture(m_queue, &image_copy_texture, byte_span.data(), byte_span.size_bytes(), &texture_data_layout, &copy_extent);

    ktxTexture_Destroy(ktx_texture);

    recreate_compose_bind_group();
}

void Window::resize_framebuffer(int w, int h)
{
    m_swapchain_size = glm::vec2(w, h);

    m_gbuffer_format = webgpu::FramebufferFormat(m_context->pipeline_manager()->render_tiles_pipeline().framebuffer_format());
    m_gbuffer_format.size = glm::uvec2 { w, h };
    m_gbuffer = std::make_unique<webgpu::Framebuffer>(m_device, m_gbuffer_format);

    webgpu::FramebufferFormat atmosphere_framebuffer_format(m_context->pipeline_manager()->render_atmosphere_pipeline().framebuffer_format());
    atmosphere_framebuffer_format.size = glm::uvec2(1, h);
    m_atmosphere_framebuffer = std::make_unique<webgpu::Framebuffer>(m_device, atmosphere_framebuffer_format);

    m_depth_texture_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_device,
        m_context->pipeline_manager()->depth_texture_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> {
            m_gbuffer->depth_texture_view().create_bind_group_entry(0), // depth
        });

    m_context->cloud_geometry()->resize(w, h);

    // Do late
    recreate_compose_bind_group();
}

std::unique_ptr<webgpu::raii::RenderPassEncoder> begin_render_pass(
    WGPUCommandEncoder encoder, WGPUTextureView color_attachment, WGPUTextureView depth_attachment)
{
    return std::make_unique<webgpu::raii::RenderPassEncoder>(encoder, color_attachment, depth_attachment);
}

void Window::paint(webgpu::Framebuffer* framebuffer, WGPUCommandEncoder command_encoder)
{
    m_needs_redraw = false;

    // ToDo only update on change?
    m_shared_config_ubo->data = m_context->shared_config();
    m_shared_config_ubo->update_gpu_data(m_queue);

    // render atmosphere to color buffer
    {
        std::unique_ptr<webgpu::raii::RenderPassEncoder> render_pass = m_atmosphere_framebuffer->begin_render_pass(command_encoder);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, m_camera_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(render_pass->handle(), m_context->pipeline_manager()->render_atmosphere_pipeline().pipeline().handle());
        wgpuRenderPassEncoderDraw(render_pass->handle(), 3, 1, 0, 0);
    }

    // render tiles to geometry buffers
    {
        std::unique_ptr<webgpu::raii::RenderPassEncoder> render_pass = m_gbuffer->begin_render_pass(command_encoder);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, m_shared_config_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 1, m_camera_bind_group->handle(), 0, nullptr);

        using namespace nucleus::tile;
        const auto draw_list = drawing::compute_bounds(
            drawing::limit(drawing::generate_list(m_camera, m_context->aabb_decorator(), m_max_zoom_level), 1024), m_context->aabb_decorator());
        const auto culled_draw_list = drawing::sort(drawing::cull(draw_list, m_camera), m_camera.position());

        m_context->tile_geometry()->draw(render_pass->handle(), m_camera, culled_draw_list);
    }

    // render clouds
    if (m_context->shared_config().m_clouds_enabled) {
        m_context->cloud_geometry()->draw(
            command_encoder, m_depth_texture_bind_group->handle(), m_shared_config_bind_group->handle(), m_camera, m_paint_number);
        m_needs_redraw |= m_context->cloud_geometry()->needs_redraw(); // Repaint for TAAU
    }

    // render geometry buffers to target framebuffer
    {
        std::unique_ptr<webgpu::raii::RenderPassEncoder> render_pass = framebuffer->begin_render_pass(command_encoder);
        wgpuRenderPassEncoderSetPipeline(render_pass->handle(), m_context->pipeline_manager()->compose_pipeline().pipeline().handle());
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, m_shared_config_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 1, m_camera_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 2, m_compose_bind_groups[m_paint_number % 2]->handle(), 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass->handle(), 3, 1, 0, 0);
    }

    // render lines to color buffer
    if (m_context->shared_config().m_track_render_mode > 0) {
        m_track_renderer->render(
            command_encoder, *m_shared_config_bind_group, *m_camera_bind_group, *m_depth_texture_bind_group, framebuffer->color_texture_view(0));
    }

    if (m_first_paint) {
        after_first_frame();
    }
    m_first_paint = false;
    m_paint_number++;
}

void Window::paint_gui()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI

    if (ImGui::Combo("Normal Mode", (int*)&m_context->shared_config().m_normal_mode, "None\0Flat\0Smooth\0\0")) {
        m_needs_redraw = true;
    }
    {
        static int currentItem = 0;
        static const std::vector<std::pair<std::string, int>> overlays = { { "None", 0 },
            { "Normals", 1 },
            { "Tiles", 2 },
            { "Zoomlevel", 3 },
            { "Vertex-ID", 4 },
            { "Vertex Height-Sample", 5 },
            { "Decoded Normals", 100 },
            { "Steepness", 101 },
            { "SSAO Buffer", 102 },
            { "Shadow Cascades", 103 } };
        const char* currentItemLabel = overlays[currentItem].first.c_str();
        if (ImGui::BeginCombo("Overlay", currentItemLabel)) {
            for (size_t i = 0; i < overlays.size(); i++) {
                bool isSelected = ((size_t)currentItem == i);
                if (ImGui::Selectable(overlays[i].first.c_str(), isSelected)) {
                    currentItem = int(i);
                    m_needs_redraw = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        m_context->shared_config().m_overlay_mode = overlays[currentItem].second;
        if (m_context->shared_config().m_overlay_mode > 0) {
            m_needs_redraw |= ImGui::SliderFloat("Overlay Strength", &m_context->shared_config().m_overlay_strength, 0.0f, 1.0f);
        }

        m_needs_redraw |= ImGui::Checkbox("Overlay Post Shading", (bool*)&m_context->shared_config().m_overlay_postshading_enabled);

        ImGui::Separator();

        m_needs_redraw |= ImGui::Checkbox("Phong Shading", (bool*)&m_context->shared_config().m_phong_enabled);
        m_needs_redraw |= ImGui::Checkbox("Atmosphere", (bool*)&m_context->shared_config().m_atmosphere_enabled);
        m_needs_redraw |= ImGui::Checkbox("Clouds", (bool*)&m_context->shared_config().m_clouds_enabled);

        bool snow_on = (m_context->shared_config().m_snow_settings_angle.x == 1.0f);
        if (ImGui::Checkbox("Snow", &snow_on)) {
            m_needs_redraw = true;
            m_context->shared_config().m_snow_settings_angle.x = (snow_on ? 1.0f : 0.0f);
        }
        if (snow_on) {
            ImGui::SameLine();
            if (ImGui::CollapsingHeader("###Snow Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& snow_settings_angle = m_context->shared_config().m_snow_settings_angle;
                bool changed = ImGui::DragFloatRange2(
                    "Angle limit", &snow_settings_angle.y, &snow_settings_angle.z, 0.1f, 0.0f, 90.0f, "Min: %.1f°", "Max: %.1f°", ImGuiSliderFlags_AlwaysClamp);
                changed |= ImGui::SliderFloat("Angle blend", &snow_settings_angle.w, 0.0f, 90.0f, "%.1f°");
                changed |= ImGui::SliderFloat("Altitude limit", &m_context->shared_config().m_snow_settings_alt.x, 0.0f, 4000.0f, "%.1fm");
                changed |= ImGui::SliderFloat("Altitude variation", &m_context->shared_config().m_snow_settings_alt.y, 0.0f, 1000.0f, "%.1f°");
                changed |= ImGui::SliderFloat("Altitude blend", &m_context->shared_config().m_snow_settings_alt.z, 0.0f, 1000.0f);
                changed |= ImGui::SliderFloat("Specular", &m_context->shared_config().m_snow_settings_alt.w, 0.0f, 5.0f);

                if (changed) {
                    m_needs_redraw = true;
                    update_compute_pipeline_settings();
                }
            }
        }

        m_needs_redraw |= ImGui::Checkbox("Heightlines", (bool*)&m_context->shared_config().m_height_lines_enabled);
        if (m_context->shared_config().m_height_lines_enabled) {
            ImGui::SameLine();
            if (ImGui::CollapsingHeader("###Height Lines", ImGuiTreeNodeFlags_DefaultOpen)) {
                float& primary = m_context->shared_config().m_height_lines_settings.x;
                float& secondary = m_context->shared_config().m_height_lines_settings.y;
                float ratio = primary / secondary;
                if (ImGui::DragFloat("Primary Interval", &primary, 1.0f, 5.0f, 1000.0f, "%.2f m")) {
                    m_needs_redraw = true;
                    secondary = primary / ratio;
                }
                m_needs_redraw |= ImGui::DragFloat("Secondary Interval", &secondary, 1.0f, 1.0f, 1000.0f, "%.2f m");
                m_needs_redraw |= ImGui::DragFloat("Base Line Width", &m_context->shared_config().m_height_lines_settings.z, 0.01f, 0.1f, 5.0f, "%.2f");
                m_needs_redraw |= ImGui::DragFloat("Darkening Factor", &m_context->shared_config().m_height_lines_settings.w, 0.01f, 0.0f, 1.0f, "%.2f");
            }
        }
    }

    if (ImGui::CollapsingHeader("Image overlay")) {
        if (ImGui::Button("Open overlay image file ...", ImVec2(350, 0))) {
#ifdef __EMSCRIPTEN__
            WebInterop::instance().open_file_dialog(".png", "overlay_png");
#else
            IGFD::FileDialogConfig config;
            config.path = m_last_dialog_directory;
            ImGuiFileDialog::Instance()->OpenDialog("OverlayImageFileDialog", "Choose File", ".png,.*", config);
#endif
        }

#ifndef __EMSCRIPTEN__
        if (ImGuiFileDialog::Instance()->Display("OverlayImageFileDialog")) {
            if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK
                std::string filename_str = ImGuiFileDialog::Instance()->GetFilePathName();
                auto filename = std::filesystem::path(filename_str);

                // Construct the default AABB file path
                auto aabb_filepath = filename.parent_path() / (filename.stem().string() + "_aabb.txt");

                // If the default AABB file does not exist, try with the trackname
                if (!std::filesystem::exists(aabb_filepath)) {
                    // Extract trackname from the filename until the first '_'
                    std::string filename_stem = filename.stem().string();
                    size_t underscore_pos = filename_stem.find('_');
                    std::string trackname = (underscore_pos != std::string::npos) ? filename_stem.substr(0, underscore_pos) // Before the first '_'
                                                                                  : filename_stem; // Entire stem if no '_'

                    // Construct the new AABB file path using trackname
                    aabb_filepath = filename.parent_path() / (trackname + "_aabb.txt");
                }

                // if trackname aabb does not exist, try just aabb.txt
                if (!std::filesystem::exists(aabb_filepath)) {
                    aabb_filepath = filename.parent_path() / "aabb.txt";
                }

                // If the AABB file exists, call the appropriate functions
                if (std::filesystem::exists(aabb_filepath)) {
                    m_last_dialog_directory = filename.parent_path().string();
                    update_image_overlay_texture(filename_str);
                    update_image_overlay_aabb_and_focus(aabb_filepath.string());
                    m_needs_redraw = true;
                } else {
                    qCritical() << "No AABB file found for image overlay.";
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
#endif

#ifdef __EMSCRIPTEN__
        // NOTE: In the web we can't check the filesystem for the aabb file so the user has to open it separately
        if (ImGui::Button("Open overlay aabb file ...", ImVec2(350, 0))) {
            WebInterop::instance().open_file_dialog(".txt", "overlay_aabb_txt");
        }
#endif
        if (ImGui::SliderFloat("Strength##image overlay", &m_image_overlay_settings_uniform_buffer->data.alpha, 0.0f, 1.0f, "%.2f")) {
            m_image_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
            m_needs_redraw = true;
        }

        if (ImGui::Combo("Mode##image overlay", (int*)&(m_image_overlay_settings_uniform_buffer->data.mode), "Alpha-Blend\0Encoded Float\0")) {
            m_image_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
            m_needs_redraw = true;
        }

        if (ImGui::Checkbox("Linear Interpolation##image overlay", &m_image_overlay_linear_interpolation)) {
            if (!m_image_overlay_texture_path.empty()) {
                update_image_overlay_texture(m_image_overlay_texture_path);
                m_image_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
            }
            m_needs_redraw = true;
        }

        if (m_image_overlay_settings_uniform_buffer->data.mode == 1) {
            if (ImGui::DragFloatRange2("Float Map Range",
                    &m_image_overlay_settings_uniform_buffer->data.float_decoding_lower_bound,
                    &m_image_overlay_settings_uniform_buffer->data.float_decoding_upper_bound,
                    1.0f,
                    -10000,
                    10000)) {
                m_image_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
                m_needs_redraw = true;
            }
        }
    }

    if (ImGui::CollapsingHeader("Track", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Open GPX file ...", ImVec2(250, 0))) {
#ifdef __EMSCRIPTEN__
            WebInterop::instance().open_file_dialog(".gpx", "track");
#else
            IGFD::FileDialogConfig config;
            config.path = m_last_dialog_directory;
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".gpx,.*", config);
#endif
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(106 / 255.0f, 112 / 255.0f, 115 / 255.0f, 1.00f));
        if (ImGui::Button("Open Preset ...", ImVec2(100, 0))) {
            load_track_and_focus(DEFAULT_GPX_TRACK_PATH);
        }
        ImGui::PopStyleColor(1);

        const char* items = "none\0without depth test\0with depth test\0semi-transparent\0";
        if (ImGui::Combo("Line render mode", (int*)&(m_context->shared_config().m_track_render_mode), items)) {
            m_needs_redraw = true;
        }
    }

#ifndef __EMSCRIPTEN__
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK
            std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            m_last_dialog_directory = std::filesystem::path(file_path).parent_path().string();
            load_track_and_focus(file_path);
        }
        ImGuiFileDialog::Instance()->Close();
    }
#endif

    paint_compute_pipeline_gui();

    if (m_gui_error_state.should_open_modal) {
        ImGui::OpenPopup("Error");
        m_gui_error_state.should_open_modal = false;
    }

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::PushTextWrapPos(30.0f * ImGui::GetFontSize());
        ImGui::Text("%s", m_gui_error_state.text.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

#endif
}

void Window::paint_compute_pipeline_gui()
{
#if ALP_WEBGPU_APP_ENABLE_IMGUI
    if (ImGui::CollapsingHeader("Compute pipeline", ImGuiTreeNodeFlags_DefaultOpen)) {

        if (ImGui::Button("Run", ImVec2(250, 0))) {
            if (m_is_region_selected) {
                update_settings_and_rerun_pipeline();
            } else {
                display_message("Cannot run pipeline - No region selected");
            }
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(150 / 255.0f, 10 / 255.0f, 10 / 255.0f, 1.00f));
        if (ImGui::Button("Clear", ImVec2(100, 0))) {
            create_and_set_compute_pipeline(m_active_compute_pipeline_type);
            m_needs_redraw = true;
        }
        ImGui::PopStyleColor(1);

        if (ImGui::SliderFloat("Strength##compute overlay", &m_compute_overlay_settings_uniform_buffer->data.alpha, 0.0f, 1.0f, "%.2f")) {
            m_compute_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
            m_needs_redraw = true;
        }

        const uint32_t min_zoomlevel = 1;
        const uint32_t max_zoomlevel = 18;
        ImGui::SliderScalar("Zoom level", ImGuiDataType_U32, &m_compute_pipeline_settings.zoomlevel, &min_zoomlevel, &max_zoomlevel, "%u");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            update_settings_and_rerun_pipeline();
        }

        static int overlays_current_item = 2;
        const std::vector<std::pair<std::string, ComputePipelineType>> overlays = {
            { "Normals", ComputePipelineType::NORMALS },
            { "Snow", ComputePipelineType::SNOW },
            { "Avalanche trajectories", ComputePipelineType::AVALANCHE_TRAJECTORIES },
            { "Release points", ComputePipelineType::RELEASE_POINTS },
            { "Iterative simulation (WIP)", ComputePipelineType::ITERATIVE_SIMULATION },
        };
        const char* current_item_label = overlays[overlays_current_item].first.c_str();
        if (ImGui::BeginCombo("Type", current_item_label)) {
            for (size_t i = 0; i < overlays.size(); i++) {
                bool is_selected = ((size_t)overlays_current_item == i);
                if (ImGui::Selectable(overlays[i].first.c_str(), is_selected)) {
                    overlays_current_item = int(i);
                    create_and_set_compute_pipeline(overlays[i].second);
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    {
        ImVec2 button_pos(10, ImGui::GetIO().DisplaySize.y - 48 * 2 - 40 - 10);
        ImGui::SetNextWindowPos(button_pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f);
        ImGui::SetNextWindowSize(ImVec2(48, 48));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("ToggleGraphRenderWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // fully transparent
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.2f)); // black with alpha 0.2
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.2f)); // same for active

        if (ImGui::Button(ICON_FA_NETWORK_WIRED "###ToggleGraphRenderer", ImVec2(48, 48))) {
            m_should_render_node_graph = !m_should_render_node_graph;
        }

        ImGui::PopStyleColor(3);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // render node graph
    if (m_should_render_node_graph) {
        m_node_graph_renderer->render();
    }

#endif
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
        src_texture.copy_to_buffer(m_device, *m_position_readback_buffer.get(), glm::uvec3(device_coordinates.x, device_coordinates.y, 0), glm::uvec2(16, 1));

        std::vector<glm::vec4> pos_buffer;
        WGPUMapAsyncStatus result = m_position_readback_buffer->read_back_sync(m_context->webgpu_instance(), m_device, pos_buffer);
        if (result == WGPUMapAsyncStatus_Success) {
            m_last_position_readback = pos_buffer[0];
        }
    } // else qDebug() << "Dropped position readback request, buffer still mapping.";

    // qDebug() << "Position:" << glm::to_string(m_last_position_readback);
    return m_last_position_readback;
}

void Window::create_and_set_compute_pipeline(ComputePipelineType pipeline_type, bool should_recreate_compose_bind_group)
{
    qDebug() << "setting new compute pipeline " << static_cast<int>(pipeline_type);
    m_active_compute_pipeline_type = pipeline_type;

    if (pipeline_type == ComputePipelineType::NORMALS) {
        m_compute_graph = compute::nodes::NodeGraph::create_normal_compute_graph(*m_context->pipeline_manager(), m_device);
    } else if (pipeline_type == ComputePipelineType::SNOW) {
        m_compute_graph = compute::nodes::NodeGraph::create_snow_compute_graph(*m_context->pipeline_manager(), m_device);
    } else if (pipeline_type == ComputePipelineType::AVALANCHE_TRAJECTORIES) {
        m_compute_graph = compute::nodes::NodeGraph::create_trajectories_with_export_compute_graph(*m_context->pipeline_manager(), m_device);
        m_compute_graph->set_enabled_for_nodes_with_name("export", false);
    } else if (pipeline_type == ComputePipelineType::RELEASE_POINTS) {
        m_compute_graph = compute::nodes::NodeGraph::create_release_points_compute_graph(*m_context->pipeline_manager(), m_device);
    } else if (pipeline_type == ComputePipelineType::ITERATIVE_SIMULATION) {
        m_compute_graph = compute::nodes::NodeGraph::create_iterative_simulation_compute_graph(*m_context->pipeline_manager(), m_device);
    }

    update_compute_pipeline_settings();

    connect(m_compute_graph.get(), &compute::nodes::NodeGraph::run_completed, this, [this](compute::GraphRunContext) { request_redraw(); });
    connect(m_compute_graph.get(), &compute::nodes::NodeGraph::run_completed, this, [this](compute::GraphRunContext) { on_pipeline_run_completed(); });

    connect(m_compute_graph.get(), &compute::nodes::NodeGraph::run_failed, this, [this](compute::nodes::GraphRunFailureInfo info) {
        qWarning() << "graph run failed. " << info.node_name() << ": " << info.node_run_failure_info().message();
        std::string message = "Execution of pipeline failed.\n\nNode \"" + info.node_name() + "\" reported \"" + info.node_run_failure_info().message() + "\"";
        this->display_message(message);
    });

    if (should_recreate_compose_bind_group) {
        // we usually need to recreate the compose bind group, because it might have now-outdated texture bindings from the last (now-destroyed) pipeline
        // however, we dont want this to happen when initializing, because at that point we dont have a gbuffer yet (which is required for creating the bind
        // group)
        clear_compute_overlay();
        recreate_compose_bind_group();
    }

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    m_node_graph_renderer = std::make_unique<compute::NodeGraphRenderer>();
    m_node_graph_renderer->init(*m_compute_graph.get());
#endif

    m_is_first_pipeline_run = true;
}

void Window::update_compute_pipeline_settings()
{
    if (m_active_compute_pipeline_type == ComputePipelineType::NORMALS || m_active_compute_pipeline_type == ComputePipelineType::RELEASE_POINTS) {
        m_compute_graph->get_node_as<compute::nodes::SelectTilesNode>("select_tiles_node")
            .select_tiles_in_world_aabb(m_compute_pipeline_settings.target_region, m_compute_pipeline_settings.zoomlevel);
    } else if (m_active_compute_pipeline_type == ComputePipelineType::SNOW) {
        m_compute_graph->get_node_as<compute::nodes::SelectTilesNode>("select_tiles_node")
            .select_tiles_in_world_aabb(m_compute_pipeline_settings.target_region, m_compute_pipeline_settings.zoomlevel);
    } else if (m_active_compute_pipeline_type == ComputePipelineType::AVALANCHE_TRAJECTORIES) {
        m_compute_graph->get_node_as<compute::nodes::SelectTilesNode>("select_tiles_node")
            .select_tiles_in_world_aabb(m_compute_pipeline_settings.target_region, m_compute_pipeline_settings.zoomlevel);
    } else if (m_active_compute_pipeline_type == ComputePipelineType::ITERATIVE_SIMULATION) {
        m_compute_graph->get_node_as<compute::nodes::SelectTilesNode>("select_tiles_node")
            .select_tiles_in_world_aabb(m_compute_pipeline_settings.target_region, m_compute_pipeline_settings.zoomlevel);
    }
}

void Window::update_settings_and_rerun_pipeline(const std::string& entry_node)
{
    update_compute_pipeline_settings();
    if (m_is_region_selected) {
        if (!entry_node.empty() && !m_is_first_pipeline_run) {
            if (m_compute_graph->exists_node(entry_node)) {
                m_compute_graph->get_node_as<compute::nodes::Node>(entry_node).rerun();
            } else {
                qCritical() << "Entry node" << entry_node << "does not exist.";
            }
        } else {
            m_is_first_pipeline_run = false;
            m_compute_graph->run();
        }
    } else {
        qWarning() << "No region selected. Please load track.";
    }
}

std::unique_ptr<webgpu::raii::TextureWithSampler> Window::create_overlay_texture(unsigned int width, unsigned int height, bool linear_interpolation)
{
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "image overlay texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { uint32_t(width), uint32_t(height), uint32_t(1) };
    texture_desc.mipLevelCount = webgpu::raii::Texture::max_mip_level_count(glm::uvec2(width, height));
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm;
    texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_StorageBinding;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "image overlay sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    const auto filter_mode = linear_interpolation ? WGPUFilterMode::WGPUFilterMode_Linear : WGPUFilterMode::WGPUFilterMode_Nearest;
    const auto mipmap_mode = linear_interpolation ? WGPUMipmapFilterMode::WGPUMipmapFilterMode_Linear : WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.magFilter = filter_mode;
    sampler_desc.minFilter = filter_mode;
    sampler_desc.mipmapFilter = mipmap_mode;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    auto texture = std::make_unique<webgpu::raii::TextureWithSampler>(m_device, texture_desc, sampler_desc);

    return texture;
}

void Window::set_max_zoom_level(uint32_t max_zoom_level) { m_max_zoom_level = max_zoom_level; }

void Window::display_message(const std::string& message)
{
    m_gui_error_state.text = message;
    m_gui_error_state.should_open_modal = true;
}

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

void Window::destroy()
{
    //  emit gpu_ready_changed(false); // TODO find replacement
}

nucleus::camera::AbstractDepthTester* Window::depth_tester()
{
    // Return this object as the depth tester
    return this;
}

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
    m_camera_config_ubo->update_gpu_data(m_queue);
    m_camera = new_definition;

    emit update_requested();
    // m_needs_redraw = true;
}

void Window::update_debug_scheduler_stats([[maybe_unused]] const QString& stats)
{
    // Logic for updating debug scheduler stats, parameter currently unused
}

void Window::pick_value([[maybe_unused]] const glm::dvec2& screen_space_coordinate)
{
    // Logic for picking (e.g. read back id buffer for label picking or sth)
}

void Window::request_redraw() { m_needs_redraw = true; }

void Window::file_upload_handler(const std::string& filename, const std::string& tag)
{
    if (tag == "track") {
        load_track_and_focus(filename);
    } else if (tag == "overlay_png") {
        update_image_overlay_texture(filename);
    } else if (tag == "overlay_aabb_txt") {
        update_image_overlay_aabb_and_focus(filename);
    } else {
        qWarning() << "Unknown file upload tag: " << QString::fromStdString(tag);
    }
}

void Window::load_track_and_focus(const std::string& path)
{
    std::vector<glm::dvec3> points;

    std::unique_ptr<nucleus::track::Gpx> gpx_track = nucleus::track::parse(QString::fromStdString(path));
    for (const auto& segment : gpx_track->track) {
        points.reserve(points.size() + segment.size());
        for (const auto& point : segment) {
            points.push_back({ point.latitude, point.longitude, point.elevation });
        }
    }
    m_track_renderer->add_track(points);

    const auto track_aabb = nucleus::track::compute_world_aabb(*gpx_track);
    focus_region_3d(track_aabb);

    if (m_context->shared_config().m_track_render_mode == 0) {
        m_context->shared_config().m_track_render_mode = 1;
    }

    m_needs_redraw = true;
}

void Window::focus_region_3d(const radix::geometry::Aabb3d& aabb)
{

    // add debug axis
    /*std::vector<glm::vec4> x_axis = { glm::vec4(aabb.min, 1), glm::vec4(aabb.max.x, aabb.min.y, aabb.min.z, 1) };
    std::vector<glm::vec4> y_axis = { glm::vec4(aabb.min, 1), glm::vec4(aabb.min.x, aabb.max.y, aabb.min.z, 1) };
    std::vector<glm::vec4> z_axis = { glm::vec4(aabb.min, 1), glm::vec4(aabb.min.x, aabb.min.y, aabb.max.z, 1) };
    m_track_renderer->add_world_positions(x_axis, { 1.0f, 0.0f, 0.0f, 1.0f });
    m_track_renderer->add_world_positions(y_axis, { 0.0f, 1.0f, 0.0f, 1.0f });
    m_track_renderer->add_world_positions(z_axis, { 0.0f, 0.0f, 1.0f, 1.0f });*/

    const auto aabb_size = aabb.size();

    nucleus::camera::Definition new_camera_definition = { aabb.centre() + glm::dvec3 { 0, 0, std::max(aabb_size.x, aabb_size.y) }, aabb.centre() };
    new_camera_definition.set_viewport_size(m_camera.viewport_size());

    // update pipeline settings
    m_is_region_selected = true;
    m_compute_pipeline_settings.target_region = aabb;
    update_compute_pipeline_settings();

    emit set_camera_definition_requested(new_camera_definition);
}

void Window::focus_region_2d(const radix::geometry::Aabb<2, double>& aabb)
{
    glm::dvec2 pos = glm::dvec2(aabb.min + aabb.max) / 2.0;
    auto size_x = aabb.max.x - aabb.min.x;
    auto size_y = aabb.max.y - aabb.min.y;
    nucleus::camera::Definition new_camera_definition = { glm::dvec3 { pos.x, pos.y, std::max(size_x, size_y) }, { pos.x, pos.y, 0 } };
    new_camera_definition.set_viewport_size(m_camera.viewport_size());
    emit set_camera_definition_requested(new_camera_definition);
}

void Window::update_image_overlay_texture(const std::string& image_file_path)
{
    m_image_overlay_texture_path = image_file_path;
    nucleus::Raster<glm::u8vec4> image = nucleus::utils::image_loader::rgba8(QString::fromStdString(image_file_path)).value();
    m_image_overlay_texture = create_overlay_texture(image.width(), image.height(), m_image_overlay_linear_interpolation);
    m_image_overlay_texture->texture().write(m_queue, image);
    m_image_overlay_settings_uniform_buffer->data.texture_size = glm::uvec2(image.width(), image.height());
    compute_mipmaps_for_texture(m_device, m_queue, *m_context->pipeline_manager(), &m_image_overlay_texture->texture());

    // Scan encoded float range across all non-zero pixels
    constexpr float ENCODING_MIN = -10000.0f;
    constexpr float ENCODING_MAX = 10000.0f;
    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();
    for (const glm::u8vec4& px : image) {
        uint32_t packed = (uint32_t(px.x) << 24) | (uint32_t(px.y) << 16) | (uint32_t(px.z) << 8) | uint32_t(px.w);
        if (packed == 0)
            continue;
        float value = ENCODING_MIN + (float(packed) / float(0xFFFFFFFFu)) * (ENCODING_MAX - ENCODING_MIN);
        min_val = std::min(min_val, value);
        max_val = std::max(max_val, value);
    }
    if (min_val <= max_val) {
        m_image_overlay_settings_uniform_buffer->data.float_decoding_lower_bound = min_val;
        m_image_overlay_settings_uniform_buffer->data.float_decoding_upper_bound = max_val;
        m_image_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
    }

    recreate_compose_bind_group();
}

bool Window::update_image_overlay_aabb(const radix::geometry::Aabb<2, double>& aabb)
{
    // Make sure the aabb actually changed
    glm::fvec2 new_min = glm::fvec2 { aabb.min.x, aabb.min.y };
    glm::fvec2 new_max = glm::fvec2 { aabb.max.x, aabb.max.y };
    if (new_min == m_image_overlay_settings_uniform_buffer->data.aabb_min && new_max == m_image_overlay_settings_uniform_buffer->data.aabb_max) {
        return false;
    }

    m_image_overlay_settings_uniform_buffer->data.aabb_min = new_min;
    m_image_overlay_settings_uniform_buffer->data.aabb_max = new_max;
    m_image_overlay_settings_uniform_buffer->update_gpu_data(m_queue);

    qDebug() << "updated image overlay aabb to [" << m_image_overlay_settings_uniform_buffer->data.aabb_min.x << ", "
             << m_image_overlay_settings_uniform_buffer->data.aabb_min.y << "] [" << m_image_overlay_settings_uniform_buffer->data.aabb_max.x << ", "
             << m_image_overlay_settings_uniform_buffer->data.aabb_max.y << "]";

    return true;
}

void Window::update_image_overlay_aabb_and_focus(const std::string& aabb_file_path)
{
    const auto aabb = compute::nodes::LoadRegionAabbNode::load_aabb_from_file(aabb_file_path).value();

    bool update_successful = update_image_overlay_aabb(aabb);
    if (!update_successful) {
        return;
    }

    focus_region_2d(aabb);
}

void Window::clear_compute_overlay()
{
    m_compute_overlay_texture_view = nullptr;
    m_compute_overlay_sampler = nullptr;
    recreate_compose_bind_group();
}

void Window::update_compute_overlay_texture(const webgpu::raii::TextureWithSampler& texture_with_sampler)
{
    m_compute_overlay_texture_view = &texture_with_sampler.texture_view();
    m_compute_overlay_sampler = &texture_with_sampler.sampler();
    m_compute_overlay_settings_uniform_buffer->data.texture_size = glm::uvec2(texture_with_sampler.texture().width(), texture_with_sampler.texture().height());

    compute_mipmaps_for_texture(m_device, m_queue, *m_context->pipeline_manager(), &texture_with_sampler.texture());
    // update in following update_compute_overlay_aabb
    recreate_compose_bind_group();
}

void Window::update_compute_overlay_aabb(const radix::geometry::Aabb<2, double>& aabb)
{
    m_compute_overlay_settings_uniform_buffer->data.aabb_min = glm::fvec2(aabb.min);
    m_compute_overlay_settings_uniform_buffer->data.aabb_max = glm::fvec2(aabb.max);
    m_compute_overlay_settings_uniform_buffer->update_gpu_data(m_queue);
}

void Window::after_first_frame()
{
#if defined(QT_DEBUG)
    load_track_and_focus(DEFAULT_GPX_TRACK_PATH);
    // m_compute_graph->run();
#endif
}

void Window::reload_shaders()
{
    m_context->shader_module_manager()->release_shader_modules();
    m_context->shader_module_manager()->create_shader_modules();
    m_context->pipeline_manager()->release_pipelines();
    m_context->pipeline_manager()->create_pipelines();
    qDebug() << "reloading shaders done";
    request_redraw();
}

void Window::on_pipeline_run_completed()
{
    // update compute overlay texture and aabb with compute pipeline outputs
    if (m_active_compute_pipeline_type == ComputePipelineType::NORMALS || m_active_compute_pipeline_type == ComputePipelineType::SNOW
        || m_active_compute_pipeline_type == ComputePipelineType::RELEASE_POINTS
        || m_active_compute_pipeline_type == ComputePipelineType::AVALANCHE_TRAJECTORIES
        || m_active_compute_pipeline_type == ComputePipelineType::ITERATIVE_SIMULATION) {

        const webgpu::raii::TextureWithSampler* texture = nullptr;
        if (m_active_compute_pipeline_type == ComputePipelineType::NORMALS) {
            texture = std::get<const webgpu::raii::TextureWithSampler*>(m_compute_graph->get_node("normals_node").output_socket("normal texture").get_data());
        } else if (m_active_compute_pipeline_type == ComputePipelineType::SNOW) {
            texture = std::get<const webgpu::raii::TextureWithSampler*>(m_compute_graph->get_node("snow_node").output_socket("snow texture").get_data());
        } else if (m_active_compute_pipeline_type == ComputePipelineType::RELEASE_POINTS) {
            texture = std::get<const webgpu::raii::TextureWithSampler*>(
                m_compute_graph->get_node("release_points_node").output_socket("release point texture").get_data());
        } else if (m_active_compute_pipeline_type == ComputePipelineType::AVALANCHE_TRAJECTORIES) {
            texture
                = std::get<const webgpu::raii::TextureWithSampler*>(m_compute_graph->get_node("buffer_to_texture_node").output_socket("texture").get_data());
        } else if (m_active_compute_pipeline_type == ComputePipelineType::ITERATIVE_SIMULATION) {
            texture = std::get<const webgpu::raii::TextureWithSampler*>(m_compute_graph->get_node("flowpy").output_socket("texture").get_data());
        }
        assert(texture != nullptr);
        update_compute_overlay_texture(*texture);

        auto& select_tiles_node = m_compute_graph->get_node_as<compute::nodes::SelectTilesNode&>("select_tiles_node");
        radix::geometry::Aabb<2, double> selected_aabb
            = *std::get<const radix::geometry::Aabb<2, double>*>(select_tiles_node.output_socket("region aabb").get_data());
        selected_aabb.max -= glm::dvec2(nucleus::srs::tile_width(18) / 65, nucleus::srs::tile_height(18) / 65); // stitch node ignores last col/row
        update_compute_overlay_aabb(selected_aabb);
    }
}

void Window::create_buffers()
{
    m_shared_config_ubo = std::make_unique<Buffer<uboSharedConfig>>(m_device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    m_camera_config_ubo = std::make_unique<Buffer<uboCameraConfig>>(m_device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);
    m_position_readback_buffer = std::make_unique<webgpu::raii::RawBuffer<glm::vec4>>(
        m_device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, 256 / sizeof(glm::vec4), "position readback buffer");
}

void Window::create_bind_groups()
{
    m_shared_config_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_device,
        m_context->pipeline_manager()->shared_config_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> { m_shared_config_ubo->raw_buffer().create_bind_group_entry(0) });

    m_camera_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_device,
        m_context->pipeline_manager()->camera_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> { m_camera_config_ubo->raw_buffer().create_bind_group_entry(0) });
}

void Window::recreate_compose_bind_group()
{
    // default bindings - we need to bind something, in case compute graph not finished yet (or has been cleared)
    const webgpu::raii::TextureView& compute_overlay_texture_view
        = m_compute_overlay_texture_view != nullptr ? *m_compute_overlay_texture_view : m_compute_overlay_dummy_texture->texture_view();
    const webgpu::raii::Sampler& compute_overlay_sampler
        = m_compute_overlay_sampler != nullptr ? *m_compute_overlay_sampler : m_compute_overlay_dummy_texture->sampler();
    WGPUBindGroupEntry compute_overlay_texture_entry = compute_overlay_texture_view.create_bind_group_entry(9);
    WGPUBindGroupEntry compute_overlay_sampler_entry = compute_overlay_sampler.create_bind_group_entry(10);

    for (int i = 0; i < 2; ++i) {
        m_compose_bind_groups[i] = std::make_unique<webgpu::raii::BindGroup>(m_device,
            m_context->pipeline_manager()->compose_bind_group_layout(),
            std::initializer_list<WGPUBindGroupEntry> {
                m_gbuffer->color_texture_view(0).create_bind_group_entry(0), // albedo texture
                m_gbuffer->color_texture_view(1).create_bind_group_entry(1), // position texture
                m_gbuffer->color_texture_view(2).create_bind_group_entry(2), // normal texture
                m_atmosphere_framebuffer->color_texture_view(0).create_bind_group_entry(3), // atmosphere texture
                m_gbuffer->color_texture_view(3).create_bind_group_entry(4), // overlay texture
                m_image_overlay_settings_uniform_buffer->raw_buffer().create_bind_group_entry(5), // image overlay aabb
                m_image_overlay_texture->texture_view().create_bind_group_entry(6), // image overlay texture (in uv space)
                m_image_overlay_texture->sampler().create_bind_group_entry(7), // image overlay sampler
                m_compute_overlay_settings_uniform_buffer->raw_buffer().create_bind_group_entry(8), // compute overlay aabb
                compute_overlay_texture_entry, // compute overlay texture (in uv space)
                compute_overlay_sampler_entry, // compute overlay sampler
                m_context->cloud_geometry()->result_color_view(i)->create_bind_group_entry(11),
                m_context->cloud_geometry()->result_depth_view()->create_bind_group_entry(12),
                m_shadow_texture->texture_view().create_bind_group_entry(13),
                m_shadow_texture->sampler().create_bind_group_entry(14),
                m_gbuffer->depth_texture_view().create_bind_group_entry(15),
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
