/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
 * Copyright (C) 2025 Patrick Komon
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

#include "ImGuiManager.h"
#include "RenderingContext.h"
#include "TerrainRenderer.h"

#include "imgui/ImGuiPanel.h"

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui/AboutPanel.h"
#include "imgui/AppPanel.h"
#include "imgui/AtmospherePanel.h"
#include "imgui/CameraPanel.h"
#include "imgui/CloudPanel.h"
#include "imgui/CompassPanel.h"
#include "imgui/LogoPanel.h"
#include "imgui/NodeGraphPanel.h"
#include "imgui/OverlaysPanel.h"
#include "imgui/SearchPanel.h"
#include "imgui/ShadingPanel.h"
#include "imgui/TimingPanel.h"
#include "imgui/TrackPanel.h"
#include <IconsFontAwesome5.h>
#include <imgui_internal.h>
#include <imnodes.h>
#endif

#include "util/dark_mode.h"
#include <QDebug>
#include <QFile>

namespace webgpu_app {

ImGuiManager::ImGuiManager(TerrainRenderer* terrain_renderer)
    : m_terrain_renderer(terrain_renderer)
{
}

void ImGuiManager::init(
    SDL_Window* window, WGPUDevice device, [[maybe_unused]] WGPUTextureFormat swapchainFormat, [[maybe_unused]] WGPUTextureFormat depthTextureFormat)
{
    qDebug() << "Setup ImGuiManager...";
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
    install_fonts();

    auto* rc = m_terrain_renderer->get_rendering_context();
    auto* engine_ctx = rc->engine_context();

    m_panels.push_back(std::make_unique<LogoPanel>(m_device));
    m_panels.push_back(std::make_unique<CompassPanel>(m_terrain_renderer));
    m_panels.push_back(std::make_unique<AboutPanel>());
    m_panels.push_back(std::make_unique<SearchPanel>(m_terrain_renderer));
    SearchPanel& search_panel = static_cast<SearchPanel&>(**(m_panels.end() - 1));
    m_panels.push_back(std::make_unique<TimingPanel>(m_terrain_renderer));
    m_panels.push_back(std::make_unique<CameraPanel>(m_terrain_renderer));
    m_panels.push_back(std::make_unique<AppPanel>(m_terrain_renderer));
    m_panels.push_back(std::make_unique<CloudPanel>(engine_ctx, rc->clouds_manager(), engine_ctx->cloud_renderer()));
    m_panels.push_back(std::make_unique<AtmospherePanel>(engine_ctx));
    m_panels.push_back(std::make_unique<ShadingPanel>(engine_ctx));
    m_panels.push_back(std::make_unique<OverlaysPanel>(engine_ctx));
    m_panels.push_back(std::make_unique<TrackPanel>(engine_ctx, m_terrain_renderer));
    m_panels.push_back(std::make_unique<NodeGraphPanel>(engine_ctx));

    connect(&search_panel, &SearchPanel::search_requested, rc->search_service(), &SearchService::search);
    connect(&search_panel,
        &SearchPanel::search_result_selected,
        m_terrain_renderer->get_camera_controller(),
        &nucleus::camera::Controller::fly_to_latitude_longitude);
    connect(rc->search_service(), &SearchService::search_results_arrived, &search_panel, &SearchPanel::display_search_results);

#endif
}

void ImGuiManager::ready()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    for (auto& panel : m_panels)
        panel->ready();
#endif
}

void ImGuiManager::install_fonts()
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

void ImGuiManager::render([[maybe_unused]] WGPURenderPassEncoder renderPass)
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

void ImGuiManager::shutdown()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    qDebug() << "Releasing ImGuiManager...";
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
#endif
}

bool ImGuiManager::want_capture_keyboard()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    return ImGui::GetIO().WantCaptureKeyboard;
#else
    return false;
#endif
}

bool ImGuiManager::want_capture_mouse()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    return ImGui::GetIO().WantCaptureMouse;
#else
    return false;
#endif
}

void ImGuiManager::on_sdl_event(SDL_Event& event)
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    ImGui_ImplSDL2_ProcessEvent(&event);
#endif
}

void ImGuiManager::set_gui_visibility(bool visible) { m_gui_visible = visible; }

bool ImGuiManager::get_gui_visibility() const { return m_gui_visible; }

float ImGuiManager::s_tool_button_y = 0.0f;

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
bool ImGuiManager::FloatingToggleButton(const char* id, const char* icon, const char* tooltip, uint32_t* enabled)
{
    const bool on = *enabled != 0u;

    // Claim the next floating tool-button slot (bottom-left, stacking upward).
    ImVec2 button_pos(10, s_tool_button_y);
    s_tool_button_y -= 48 + 10;
    ImGui::SetNextWindowPos(button_pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(on ? 0.5f : 0.2f); // fade the background when disabled
    ImGui::SetNextWindowSize(ImVec2(48, 48));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // fully transparent
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_Text, on ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 0.5f)); // fade the icon when disabled

    const bool clicked = ImGui::Button(icon, ImVec2(48, 48));
    if (clicked)
        *enabled = on ? 0u : 1u;
    const bool hovered = ImGui::IsItemHovered();

    ImGui::PopStyleColor(4);
    ImGui::End();
    ImGui::PopStyleVar();

    if (hovered)
        ImGui::SetTooltip("%s", tooltip);
    return clicked;
}
#endif

void ImGuiManager::draw()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    if (!m_gui_visible)
        return;

    // Reset the floating tool-button stack for this frame (bottom-left, stacking upward).
    s_tool_button_y = ImGui::GetIO().DisplaySize.y - 48.0f - 40.0f;

    // Standalone windows, overlays, and modals
    for (auto& panel : m_panels)
        panel->draw();

    // Main sidebar window with CollapsingHeader sections
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 430, 0)); // Set position to top-right corner
    ImGui::SetNextWindowSize(ImVec2(430, ImGui::GetIO().DisplaySize.y)); // Set height to full screen height, width as desired
    ImGui::Begin("weBIGeo", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    for (auto& panel : m_panels)
        panel->draw_panel();

    ImGui::End();
#endif
}

} // namespace webgpu_app
