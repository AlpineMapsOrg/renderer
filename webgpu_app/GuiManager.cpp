/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Markus Rampp
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

#include "GuiManager.h"
#include "TerrainRenderer.h"

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include <imgui_internal.h>
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_wgpu.h"
#include <IconsFontAwesome5.h>
#include <imnodes.h>
#endif
#include "nucleus/utils/image_loader.h"
#include "util/dark_mode.h"
#include "webgpu_engine/Window.h"
#include <QDebug>
#include <QFile>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/tile/Scheduler.h>
#include <glm/gtc/type_ptr.hpp>

namespace webgpu_app {

GuiManager::GuiManager(TerrainRenderer* terrain_renderer)
    : m_terrain_renderer(terrain_renderer)
{
    // Lets build a vector of std::string...
    const auto position_storage = nucleus::camera::PositionStorage::instance();
    const QList<QString> position_storage_list = position_storage->getPositionList();
    for (const auto& position : position_storage_list) {
        m_camera_preset_names.push_back(position.toStdString());
    }
}

void GuiManager::init(
    SDL_Window* window, WGPUDevice device, [[maybe_unused]] WGPUTextureFormat swapchainFormat, [[maybe_unused]] WGPUTextureFormat depthTextureFormat)
{
    qDebug() << "Setup GuiManager...";
    m_window = window;
    m_device = device;
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup ImNodes
    ImNodes::CreateContext();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOther(m_window);
    ImGui_ImplWGPU_InitInfo init_info = {};
    init_info.Device = m_device;
    init_info.RenderTargetFormat = swapchainFormat;
    init_info.DepthStencilFormat = depthTextureFormat;
    init_info.NumFramesInFlight = 3;
    ImGui_ImplWGPU_Init(&init_info);

    webgpu_app::util::setup_darkmode_imgui_style();

    this->install_fonts();

    nucleus::Raster<glm::u8vec4> logo = nucleus::utils::image_loader::rgba8(":/gfx/sujet_shadow.png").value();

    m_webigeo_logo_size = ImVec2(logo.width(), logo.height());

    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "webigeo logo texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { uint32_t(logo.width()), uint32_t(logo.height()), uint32_t(1) };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm;
    texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    m_webigeo_logo = std::make_unique<webgpu::raii::Texture>(m_device, texture_desc);
    auto queue = wgpuDeviceGetQueue(m_device);
    m_webigeo_logo->write(queue, logo);
    m_webigeo_logo_view = m_webigeo_logo->create_view();

#endif
}

void GuiManager::install_fonts()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    ImGuiIO& io = ImGui::GetIO();

    float baseFontSize = 16.0f;
    float iconFontSize = 14.0f;

    {
        QFile file(":/fonts/Roboto-Regular.ttf");
        if (!file.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Failed to open Main Font.");
        }
        QByteArray byteArray = file.readAll();
        file.close();

        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(byteArray.data(), byteArray.size(), baseFontSize, &font_cfg);
    }

    {
        QFile file(":/fonts/fa5-solid-900.ttf");
        if (!file.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Failed to open glyph font.");
        }
        QByteArray byteArray = file.readAll();
        file.close();

        // merge in icons from Font Awesome
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = iconFontSize;
        icons_config.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(byteArray.data(), byteArray.size(), iconFontSize, &icons_config, icons_ranges);
    }
#endif
}

void GuiManager::render([[maybe_unused]] WGPURenderPassEncoder renderPass)
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    draw();

    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
#endif
}

void GuiManager::shutdown()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    qDebug() << "Releasing GuiManager...";
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
#endif
}

bool GuiManager::want_capture_keyboard()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    return ImGui::GetIO().WantCaptureKeyboard;
#else
    return false;
#endif
}

bool GuiManager::want_capture_mouse()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    return ImGui::GetIO().WantCaptureMouse;
#else
    return false;
#endif
}

void GuiManager::on_sdl_event(SDL_Event& event)
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    ImGui_ImplSDL2_ProcessEvent(&event);
#endif
}

void GuiManager::set_gui_visibility(bool visible) { m_gui_visible = visible; }

bool GuiManager::get_gui_visibility() const { return m_gui_visible; }

void GuiManager::toggle_timer(uint32_t timer_id)
{
    if (is_timer_selected(timer_id)) {
        m_selected_timer.erase(timer_id);
    } else {
        m_selected_timer.clear(); // Remove if multiple selection possible
        m_selected_timer.insert(timer_id);
    }
}

bool GuiManager::is_timer_selected(uint32_t timer_id) { return m_selected_timer.find(timer_id) != m_selected_timer.end(); }

void GuiManager::before_first_frame()
{
    // Init m_max_zoom level
    m_terrain_renderer->get_webgpu_window()->set_max_zoom_level(m_max_zoom_level);
    m_terrain_renderer->get_camera_controller()->update();
}

void GuiManager::draw()
{
    if (m_first_frame) {
        before_first_frame();
    }
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI

    if (!m_gui_visible)
        return;

#ifndef QT_DEBUG
    if (m_first_frame) {
        ImGui::OpenPopup("research_preview");
    }
#endif

    draw_disclaimer_popup();

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 430, 0)); // Set position to top-left corner
    ImGui::SetNextWindowSize(ImVec2(430, ImGui::GetIO().DisplaySize.y)); // Set height to full screen height, width as desired

    ImGui::Begin("weBIGeo", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    if (ImGui::CollapsingHeader(ICON_FA_STOPWATCH "  Timing")) {

        const webgpu::timing::GuiTimerWrapper* selected_timer = nullptr;
        if (!m_selected_timer.empty()) {
            uint32_t first_selected_timer_id = *m_selected_timer.begin();
            selected_timer = m_terrain_renderer->get_timer_manager()->get_timer_by_id(first_selected_timer_id);
        }
        if (selected_timer) {
            const auto* tmr = selected_timer->timer.get();
            if (tmr->get_sample_count() > 2) {
                ImVec4 timer_color = *(ImVec4*)(void*)&selected_timer->color;
                ImGui::PushStyleColor(ImGuiCol_PlotLines, timer_color);
                ImGui::PlotLines("##SelTimerGraph", &tmr->get_results()[0], (int)tmr->get_sample_count(), 0, nullptr, 0.0f, tmr->get_max(), ImVec2(380, 80));
                ImGui::PopStyleColor();
            }
        }

        auto group_list = m_terrain_renderer->get_timer_manager()->get_groups();
        for (const auto& group : group_list) {
            bool showGroup = true;
            if (group.name != "") {
                ImGui::Indent();
                showGroup = ImGui::CollapsingHeader(group.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            }
            if (showGroup) {
                for (const auto& tmr : group.timers) {
                    const uint32_t tmr_id = tmr.timer->get_id();
                    ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
                    if (is_timer_selected(tmr_id)) {
                        color = *(ImVec4*)(void*)&tmr.color;
                    }

                    if (ImGui::ColorButton(
                            ("##t" + std::to_string(tmr_id)).c_str(), color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(10, 10))) {
                        toggle_timer(tmr_id);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s: %s ±%s [%zu]", tmr.name.c_str(), webgpu::timing::format_time(tmr.timer->get_average()).c_str(),
                        webgpu::timing::format_time(tmr.timer->get_standard_deviation()).c_str(), tmr.timer->get_sample_count());
                }
            }
            if (group.name != "")
                ImGui::Unindent();
        }
        if (ImGui::Button("Reset All Timers")) {
            for (const auto& group : group_list)
                for (const auto& tmr : group.timers)
                    tmr.timer->clear_results();
        }
    }

    if (ImGui::CollapsingHeader(ICON_FA_CAMERA " Camera")) {
        if (ImGui::BeginCombo("Preset", m_camera_preset_names[m_selected_camera_preset].c_str())) {
            for (size_t n = 0; n < m_camera_preset_names.size(); n++) {
                bool is_selected = (size_t(m_selected_camera_preset) == n);
                if (ImGui::Selectable(m_camera_preset_names[n].c_str(), is_selected)) {
                    m_selected_camera_preset = int(n);

                    const auto position_storage = nucleus::camera::PositionStorage::instance();
                    const auto camera_controller = m_terrain_renderer->get_camera_controller();
                    auto new_definition = position_storage->get_by_index(m_selected_camera_preset);
                    auto old_vp_size = camera_controller->definition().viewport_size();
                    new_definition.set_viewport_size(old_vp_size);
                    camera_controller->set_model_matrix(new_definition);
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        {
            auto& camera = m_terrain_renderer->get_camera_controller()->definition();
            auto pos = camera.position();
            glm::vec3 posf = pos;
            ImGui::InputFloat3("Position", glm::value_ptr(posf), "%.2f", ImGuiInputTextFlags_ReadOnly);
            glm::vec3 coords = nucleus::srs::world_to_lat_long_alt(pos);
            ImGui::InputFloat3("Coords", glm::value_ptr(coords), "%.6f", ImGuiInputTextFlags_ReadOnly);
            float fov = camera.field_of_view();
            if (ImGui::SliderFloat("FoV", &fov, 1.0f, 179.0f)) {
                m_terrain_renderer->get_camera_controller()->set_field_of_view(fov);
            }
        }
    }

    if (ImGui::CollapsingHeader(ICON_FA_COG "  App Settings")) {
        m_terrain_renderer->render_gui();
        static float render_quality = 0.5f;
        if (ImGui::SliderFloat("Level of Detail", &render_quality, 0.1f, 2.0f)) {
            const auto permissible_error = 1.0f / render_quality;
            m_terrain_renderer->get_camera_controller()->set_pixel_error_threshold(permissible_error);
            m_terrain_renderer->update_camera();
            qDebug() << "Setting permissible error to " << permissible_error;
        }

        const uint32_t min_max_zoom_lvl = 1;
        const uint32_t max_max_zoom_lvl = 18;
        if (ImGui::SliderScalar("Max zoom level", ImGuiDataType_U32, &m_max_zoom_level, &min_max_zoom_lvl, &max_max_zoom_lvl, "%u")) {
            m_terrain_renderer->get_webgpu_window()->set_max_zoom_level(m_max_zoom_level);
            m_terrain_renderer->get_camera_controller()->update();
        }

        static int geometry_tile_source_index = 0; // 0 ... DSM, 1 ... DTM
        if (ImGui::Combo("Geometry Tiles", &geometry_tile_source_index, "AlpineMaps DSM\0AlpineMaps DTM\0")) {
            auto geometry_load_service = m_terrain_renderer->get_rendering_context()->geometry_tile_load_service();
            if (geometry_tile_source_index == 0) {
                geometry_load_service->set_base_url("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/");
            } else if (geometry_tile_source_index == 1) {
                geometry_load_service->set_base_url("https://alpinemaps.cg.tuwien.ac.at/tiles/at_dtm_alpinemaps/");
            }

            m_terrain_renderer->get_rendering_context()->geometry_scheduler()->clear_full_cache();
            m_terrain_renderer->get_camera_controller()->update();
        }

        static int ortho_tile_source_index = 0;
        if (ImGui::Combo("Ortho Tiles", &ortho_tile_source_index, "Gataki Ortho\0Basemap Ortho\0Basemap Gelände\0Basemap Oberfläche\0")) {
            auto ortho_load_service = m_terrain_renderer->get_rendering_context()->ortho_tile_load_service();
            if (ortho_tile_source_index == 0) {
                ortho_load_service->set_base_url("https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/");
            } else if (ortho_tile_source_index == 1) {
                ortho_load_service->set_base_url("https://mapsneu.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/");
            } else if (ortho_tile_source_index == 2) {
                ortho_load_service->set_base_url("https://mapsneu.wien.gv.at/basemap/bmapgelaende/grau/google3857/");
            } else if (ortho_tile_source_index == 3) {
                ortho_load_service->set_base_url("https://mapsneu.wien.gv.at/basemap/bmapoberflaeche/grau/google3857/");
            }
            m_terrain_renderer->get_rendering_context()->ortho_scheduler()->clear_full_cache();
            m_terrain_renderer->get_camera_controller()->update();
        }
    }

    {
        auto rendering_context = m_terrain_renderer->get_rendering_context();
        auto clouds_manger = rendering_context->clouds_manager();
        const auto& times = clouds_manger->get_slots();
        auto manifest_status = clouds_manger->get_manifest_status();

        auto selected_slot = clouds_manger->selected_time_slot();

        if (ImGui::CollapsingHeader(ICON_FA_CLOUD "  Clouds")) {

            ImGui::SeparatorText("Data");

            if (times.empty()) {
                if (manifest_status == clouds::ManifestStatus::Pending) {
                    ImGui::Text("Loading cloud data...");
                } else {
                    if (manifest_status == clouds::ManifestStatus::Error) {
                        ImGui::Text("Failed to fetch cloud data.");
                    } else {
                        ImGui::Text("No cloud data available.");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_SYNC "##reload_clouds")) {
                        clouds_manger->refresh_manifest();
                    }
                }
            } else {
                std::string preview_str = "Select time";
                if (!selected_slot.id.isEmpty()) {
                    preview_str = selected_slot.format_string() + " (" + clouds::to_string(selected_slot.status) + ")";
                }
                if (ImGui::BeginCombo("(UTC)", preview_str.c_str())) {
                    for (int n = 0; n < (int)times.size(); n++) {
                        auto slot = times[n];
                        ImGui::PushID(slot.id.toStdString().c_str());
                        const bool is_selected = slot.id == selected_slot.id;
                        std::string label = slot.format_string() + " (" + clouds::to_string(slot.status) + ")";
                        if (ImGui::Selectable(label.c_str(), is_selected)) {
                            clouds_manger->select_time_slot(times[n]);
                        }

                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                if (manifest_status == clouds::ManifestStatus::Pending) ImGui::BeginDisabled();
                if (ImGui::Button(ICON_FA_SYNC "##reload_clouds")) {
                    clouds_manger->refresh_manifest();
                }
                if (manifest_status == clouds::ManifestStatus::Pending) ImGui::EndDisabled();

                bool can_generate = selected_slot.is_generation_requestable();
                if (!can_generate) ImGui::BeginDisabled();
                if (ImGui::Button("Generate")) {
                    clouds_manger->generate_selected_slot();
                }
                if (!can_generate) ImGui::EndDisabled();
                if (selected_slot.status == clouds::SlotStatus::Pending) {
                    ImGui::SameLine();
                    ImGui::Text("Progress: %s %d%%", selected_slot.progress.stage.toStdString().c_str(), selected_slot.progress.percent);
                }
            }

            ImGui::SeparatorText("Shading");
            auto& shader_params = m_terrain_renderer->get_rendering_context()->engine_context()->cloud_geometry()->shader_params;
            ImGui::Text("Step Size");
            ImGui::Indent();
            ImGui::DragFloat("Minimum", &shader_params.step_size_min, 1.0, 0.0, 10000.0);
            float inv_dist_fact = 1.0 / shader_params.step_size_distance_factor;
            if (ImGui::DragFloat("Distance Factor", &inv_dist_fact, 1.0, 0.0, 10000.0)) {
                shader_params.step_size_distance_factor = 1.0 / inv_dist_fact;
            }
            ImGui::DragFloat("Horizon Factor", &shader_params.step_size_horizon_factor, 1.0, 0.0, 10000.0);
            ImGui::Unindent();
            ImGui::Text("Scattering");
            ImGui::Indent();
            ImGui::SliderFloat("Scattering Coeff", &shader_params.scattering_coeff, -1, 1);
            ImGui::SliderFloat("Extinction Coeff", &shader_params.extinction_coeff, 0, 1, "%.5f");
            ImGui::SliderFloat("Albedo", &shader_params.albedo, 0, 1);
            ImGui::Unindent();
            ImGui::Text("Lighting");
            ImGui::Indent();
            ImGui::DragFloat("Sun Light Scale", &shader_params.sun_light_scale, 1.0, 0.0, 10000.0);
            ImGui::DragFloat("Ambient Light Scale", &shader_params.ambient_light_scale, 0.01f, 0.0f, 10000.0f);
            ImGui::DragFloat("Atmospheric Light Scale", &shader_params.atmospheric_light_scale, 0.01f, 0.0f, 10000.0f);
            ImGui::DragFloat("Shadow Extinction Scale", &shader_params.shadow_extinction_scale, 0.01f, 0.0f, 10000.0f);
            ImGui::SliderFloat("Powder Effect Scale", &shader_params.powder_scale, 0, 1);
            ImGui::Unindent();
            ImGui::Text("Visibility");
            ImGui::Indent();
            ImGui::SliderFloat("Fade", &shader_params.fade_factor, 0.001f, 1.0f);
            ImGui::Unindent();
            ImGui::Text("Accumulation");
            ImGui::Indent();
            ImGui::SliderInt("Stable Frames Limit", &shader_params.stable_frames_limit, 1, 256);
            ImGui::Unindent();
        }
    }

    if (ImGui::CollapsingHeader(ICON_FA_COGS "  Engine Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto webgpu_window = m_terrain_renderer->get_webgpu_window();
        if (webgpu_window) {
            webgpu_window->paint_gui();
        }
    }

    ImGui::End();

    {
        // === ROTATE NORTH BUTTON ===
        // Offset position from the bottom-left corner by 32 pixels, then position the button
        ImVec2 button_pos(10, ImGui::GetIO().DisplaySize.y - 48 - 40);
        ImGui::SetNextWindowPos(button_pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f); // Semi-transparent background
        ImGui::SetNextWindowSize(ImVec2(48, 48)); // Set button size

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // No padding for better icon alignment
        ImGui::Begin("RotateNorthButton", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // fully transparent
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.2f)); // black with alpha 0.2
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.2f)); // same for active

        auto camController = m_terrain_renderer->get_camera_controller();

        if (ImGui::Button("###RotateNorthButton", ImVec2(48, 48))) {
            camController->rotate_north();
        }

        ImGui::PopStyleColor(3);

        // Drawing the arrow icon manually with rotation
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const auto rectMin = ImGui::GetItemRectMin();

        auto cameraFrontAxis = camController->definition().z_axis();
        auto degFromNorth = glm::degrees(glm::acos(glm::dot(glm::normalize(glm::dvec3(cameraFrontAxis.x, cameraFrontAxis.y, 0)), glm::dvec3(0, -1, 0))));
        float cameraAngle = cameraFrontAxis.x > 0 ? degFromNorth : -degFromNorth;

        ImVec2 center = ImVec2(rectMin.x + 24, rectMin.y + 24); // Center of the button
        float rotation_angle = cameraAngle * (M_PI / 180.0f);

        // Define arrow vertices relative to the center
        float arrow_length = 16.0f; // Size of the arrow
        ImVec2 points[3] = {
            ImVec2(0.0f, -arrow_length), // Arrow tip
            ImVec2(-arrow_length * 0.5f, arrow_length * 0.5f), // Left base
            ImVec2(arrow_length * 0.5f, arrow_length * 0.5f), // Right base
        };

        // Rotate and translate arrow vertices to draw at the specified angle
        for (int i = 0; i < 3; ++i) {
            float rotated_x = cos(rotation_angle) * points[i].x - sin(rotation_angle) * points[i].y;
            float rotated_y = sin(rotation_angle) * points[i].x + cos(rotation_angle) * points[i].y;
            points[i] = ImVec2(center.x + rotated_x, center.y + rotated_y);
        }

        // Draw the rotated arrow
        draw_list->AddTriangleFilled(points[0], points[1], points[2], IM_COL32(255, 255, 255, 255)); // White color for the arrow

        ImGui::End();
        ImGui::PopStyleVar(); // Restore padding
    }

    { // weBIGeo LOGO
        ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
        float viewportWidth = viewportSize.x;
        const float minWidth = 800.0f;
        const float maxWidth = 1920.0f;

        float scaleFactor = 1.0f;
        if (viewportWidth <= minWidth) {
            scaleFactor = 0.5f;
        } else if (viewportWidth >= maxWidth) {
            scaleFactor = 1.0f;
        } else {
            scaleFactor = 0.5f + 0.5f * ((viewportWidth - minWidth) / (maxWidth - minWidth));
        }
        ImVec2 scaledSize = ImVec2(m_webigeo_logo_size.x * scaleFactor, m_webigeo_logo_size.y * scaleFactor);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("weBIGeo-Logo",
            nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                | ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Image((ImTextureID)m_webigeo_logo_view->handle(), scaledSize);
        ImGui::End();
    }

    { // Draw the copyright box
        // Position the white box in the bottom-left corner
        ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 30), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f); // Semi-transparent background
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4)); // Reduce padding
        // Set border color to transparent
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent border
        ImGui::Begin("CopyrightBox", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        // Set up a button with no hover effect by temporarily changing colors
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.0f)); // Transparent background
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 1.0f, 0.2f)); // No hover effect
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 1.0f, 0.1f)); // No active effect

        if (ImGui::Button("About")) {
            m_show_about_popup = true; // necessary due to ImGui-Window-Scope
        }

        ImGui::PopStyleColor(3);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(); // Restore padding
    }

    draw_about_popup();

    m_first_frame = false;
#endif
}

void GuiManager::draw_disclaimer_popup()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(365, 200));

    if (ImGui::BeginPopupModal("research_preview", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        const char* text = "WARNING: RESEARCH PREVIEW";
        auto windowWidth = ImGui::GetWindowSize().x;
        auto textWidth = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::Text("%s", text);

        ImGui::Spacing();
        ImGui::TextWrapped(
            "The avalanche simulation and visualization is part of a research project and should not be used as a basis for decision-making during "
            "actual route planning.");
        ImGui::Spacing();
        ImGui::TextWrapped("Simulations always contain uncertainty and their results\n"
                           "may differ drastically from reality in some cases.");
        ImGui::Spacing();
        ImGui::TextWrapped("We exclude liability for any accidents or damages in connection to this service.");
        ImGui::Spacing();
        ImGui::SetCursorPosX(205);
        if (ImGui::Button("OK", ImVec2(150, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
#endif
}

void GuiManager::draw_about_popup()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    if (m_show_about_popup) {
        m_show_about_popup = false;
        ImGui::OpenPopup("about_webigeo");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({450,470});

    if (ImGui::BeginPopupModal("about_webigeo", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        const char* title = "About weBIGeo";
        float windowWidth = ImGui::GetWindowSize().x;
        float textWidth   = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::Text("%s", title);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextWrapped("weBIGeo is a research project to show that processing and "
                           "visualizing of large datasets is possible in the browser near real-time.");

        ImGui::Spacing();
        ImGui::Text("This project is based on "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("AlpineMaps.org", "https://github.com/AlpineMapsOrg/renderer");

        ImGui::Spacing();
        ImGui::Text("It is licensed under the "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("GPLv3", "https://www.gnu.org/licenses/gpl-3.0.html#license-text");

        ImGui::Spacing();
        ImGui::TextLinkOpenURL("GitHub repository", "https://github.com/weBIGeo/webigeo");
        ImGui::TextLinkOpenURL("netidee project page", "https://www.netidee.at/webigeo");

        ImGui::Spacing(); ImGui::Spacing();
        ImGui::Text("Authors:");
        ImGui::Text(" - "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("Adam Celarek", "https://github.com/adam-ce"); ImGui::SameLine();
        ImGui::Text("(2022-2025)");
        ImGui::Text(" - "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("Gerald Kimmersdorfer", "https://github.com/GeraldKimmersdorfer"); ImGui::SameLine();
        ImGui::Text("(2023-2025)");
        ImGui::Text(" - "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("Jakob Lindner", "https://github.com/JakobLindner"); ImGui::SameLine();
        ImGui::Text("(2023)");
        ImGui::Text(" - "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("Patrick Komon", "https://github.com/pkomon"); ImGui::SameLine();
        ImGui::Text("(2024-2025)");
        ImGui::Text(" - "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("Jakob Maier", "https://github.com/Gro2mi"); ImGui::SameLine();
        ImGui::Text("(2024)");
        ImGui::Text(" - "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("Markus Rampp", "https://github.com/gue-ni"); ImGui::SameLine();
        ImGui::Text("(2025)");
        ImGui::Text(" - "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("Wendelin Muth", "https://github.com/Qendolin"); ImGui::SameLine();
        ImGui::Text("(2026)");

        ImGui::Spacing(); ImGui::Spacing();
        ImGui::Text("Height and ortho data is provided by "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("basemap.at", "https://basemap.at/");

        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextWrapped("If you have feedback or ideas for collaboration, contact us!");
        ImGui::Text("E-Mail: "); ImGui::SameLine();
        ImGui::TextLinkOpenURL("alpinemaps@cg.tuwien.ac.at", "mailto:alpinemaps@cg.tuwien.ac.at");
        ImGui::TextLinkOpenURL("Join us on Discord", "https://discord.gg/j4MRrrbh");

        ImGui::Spacing(); ImGui::Spacing();

        ImGui::SetCursorPosX(windowWidth - 200 - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Close", {200, 0})) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
#endif
}



} // namespace webgpu_app
