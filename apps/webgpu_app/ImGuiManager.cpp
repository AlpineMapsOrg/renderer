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
#include "App.h"
#include "RenderingContext.h"

#include <webgpu/base/Framebuffer.h>
#include <webgpu/base/raii/BindGroup.h>
#include <webgpu/base/util/VertexBufferInfo.h>

#include "ui/ImGuiPanel.h"

#ifdef __EMSCRIPTEN__
#include "util/WebInterop.h"
#else
#include <ImGuiFileDialog.h>
#include <filesystem>
#endif

#include "atmosphere/AtmospherePanel.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_wgpu.h"
#include "cloud/CloudPanel.h"
#include "overlay/OverlaysPanel.h"
#include "track/TrackPanel.h"
#include "ui/AboutPanel.h"
#include "ui/AppPanel.h"
#include "ui/CameraPanel.h"
#include "ui/CompassPanel.h"
#include "ui/LogoPanel.h"
#include "ui/RenderGraphPanel.h"
#include "ui/SearchPanel.h"
#include "ui/ShadingPanel.h"
#include "ui/TimingPanel.h"
#ifdef ALP_WEBGPU_APP_ENABLE_COMPUTE
#include "compute/NodeGraphPanel.h"
#endif
#include <IconsFontAwesome5.h>
#include <imgui_internal.h>
#include <imnodes.h>

#include "util/dark_mode.h"
#include <QDebug>
#include <QFile>

namespace webgpu_app {

    //TODO: move
webgpu::rg::RenderGraph* g_render_graph = nullptr;

ImGuiManager::ImGuiManager(App* terrain_renderer)
    : m_terrain_renderer(terrain_renderer)
{
}

void ImGuiManager::init(
    SDL_Window* window, WGPUDevice device, WGPUTextureFormat swapchainFormat, [[maybe_unused]] WGPUTextureFormat depthTextureFormat)
{
    qDebug() << "Setup ImGuiManager...";
    m_window = window;
    m_device = device;
    m_queue = wgpuDeviceGetQueue(m_device);

    create_blit_pipeline(swapchainFormat);

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
    m_panels.push_back(std::make_unique<TrackPanel>(engine_ctx, m_terrain_renderer));
#ifdef ALP_WEBGPU_APP_ENABLE_COMPUTE
    m_panels.push_back(std::make_unique<NodeGraphPanel>(engine_ctx));
#endif
    m_panels.push_back(std::make_unique<OverlaysPanel>(engine_ctx));

    m_panels.push_back(std::make_unique<RenderGraphPanel>());

    connect(&search_panel, &SearchPanel::search_requested, rc->search_service(), &SearchService::search);
    connect(&search_panel,
        &SearchPanel::search_result_selected,
        m_terrain_renderer->get_camera_controller(),
        &nucleus::camera::Controller::fly_to_latitude_longitude);
    connect(rc->search_service(), &SearchService::search_results_arrived, &search_panel, &SearchPanel::display_search_results);

#ifdef __EMSCRIPTEN__
    connect(&WebInterop::instance(), &WebInterop::file_uploaded, this, &ImGuiManager::on_file_uploaded);
#endif
}

void ImGuiManager::ready()
{
    for (auto& panel : m_panels)
        panel->ready();
}

void ImGuiManager::install_fonts()
{
    ImGuiIO& io = ImGui::GetIO();

    float baseFontSize = 16.0f;
    float iconFontSize = 14.0f;
    float smallFontSize = 14.0f;
    float smallIconFontSize = 12.0f;

    QByteArray robotoData;
    {
        QFile file(":/fonts/Roboto-Regular.ttf");
        if (!file.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Failed to open Main Font.");
        }
        robotoData = file.readAll();
        file.close();
    }

    QByteArray faData;
    {
        QFile file(":/fonts/fa5-solid-900.ttf");
        if (!file.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Failed to open glyph font.");
        }
        faData = file.readAll();
        file.close();
    }

    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };

    // Default UI font (16 px Roboto + FA icons)
    {
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(robotoData.data(), robotoData.size(), baseFontSize, &font_cfg);

        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = iconFontSize;
        icons_config.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(faData.data(), faData.size(), iconFontSize, &icons_config, icons_ranges);
    }

    // Small font (14 px Roboto + FA icons)
    {
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        s_node_font = io.Fonts->AddFontFromMemoryTTF(robotoData.data(), robotoData.size(), smallFontSize, &font_cfg);

        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = smallIconFontSize;
        icons_config.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(faData.data(), faData.size(), smallIconFontSize, &icons_config, icons_ranges);
    }
}

void ImGuiManager::create_blit_pipeline(WGPUTextureFormat target_format)
{
    qDebug() << "Create GUI blit pipeline...";

    webgpu::FramebufferFormat format {};
    format.color_formats.emplace_back(target_format);

    WGPUBindGroupLayoutEntry backbuffer_texture_entry {};
    backbuffer_texture_entry.binding = 0;
    backbuffer_texture_entry.visibility = WGPUShaderStage_Fragment;
    backbuffer_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    backbuffer_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry gui_ubo_entry = {};
    gui_ubo_entry.binding = 1;
    gui_ubo_entry.visibility = WGPUShaderStage_Fragment;
    gui_ubo_entry.buffer.type = WGPUBufferBindingType_Uniform;
    gui_ubo_entry.buffer.minBindingSize = sizeof(GuiPipelineUBO);

    m_blit_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(
        m_device, std::vector<WGPUBindGroupLayoutEntry> { backbuffer_texture_entry, gui_ubo_entry }, "gui bind group layout");

    m_blit_ubo = std::make_unique<webgpu::raii::RawBuffer<GuiPipelineUBO>>(m_device, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, 1, "gui ubo");
    GuiPipelineUBO initial { glm::vec2(1280.0f, 1024.0f) };
    m_blit_ubo->write(m_queue, &initial);

    const char preprocessed_code[] = R"(
    @group(0) @binding(0) var backbuffer_texture : texture_2d<f32>;
    @group(0) @binding(1) var<uniform> gui_ubo : vec2f;

    struct VertexOut {
        @builtin(position) position : vec4f,
        @location(0) texcoords : vec2f
    }

    @vertex
    fn vertexMain(@builtin(vertex_index) vertex_index : u32) -> VertexOut {
        const VERTICES = array(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
        var vertex_out : VertexOut;
        vertex_out.position = vec4(VERTICES[vertex_index], 0.0, 1.0);
        vertex_out.texcoords = vec2(0.5, -0.5) * vertex_out.position.xy + vec2(0.5);
        return vertex_out;
    }

    @fragment
    fn fragmentMain(vertex_out : VertexOut) -> @location(0) vec4f {
        let tci : vec2<u32> = vec2u(vertex_out.texcoords * gui_ubo);
        var backbuffer_color = textureLoad(backbuffer_texture, tci, 0);
        return backbuffer_color;
    }
    )";

    WGPUShaderSourceWGSL wgsl_desc {};
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_desc.code = WGPUStringView {
        .data = preprocessed_code,
        .length = WGPU_STRLEN,
    };
    WGPUShaderModuleDescriptor shader_module_desc {};
    shader_module_desc.label = WGPUStringView { .data = "Gui Shader Module", .length = WGPU_STRLEN };
    shader_module_desc.nextInChain = &wgsl_desc.chain;
    auto shader_module = std::make_unique<webgpu::raii::ShaderModule>(m_device, shader_module_desc);

    m_blit_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(m_device,
        *shader_module,
        *shader_module,
        std::vector<webgpu::util::SingleVertexBufferInfo> {},
        format,
        std::vector<const webgpu::raii::BindGroupLayout*> { m_blit_bind_group_layout.get() });
}

void ImGuiManager::set_viewport_size(glm::vec2 resolution)
{
    if (!m_blit_ubo)
        return;
    GuiPipelineUBO data { resolution };
    m_blit_ubo->write(m_queue, &data);
}

void ImGuiManager::render(WGPUCommandEncoder encoder, WGPUTextureView target_view, WGPUBindGroupEntry blit_texture)
{
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    draw();

    ImGui::Render();

    webgpu::raii::BindGroup blit_bind_group(m_device, *m_blit_bind_group_layout, { blit_texture, m_blit_ubo->create_bind_group_entry(1) }, "gui blit");

    WGPURenderPassColorAttachment color_attachment {};
    color_attachment.view = target_view;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDescriptor pass_desc {};
    pass_desc.label = WGPUStringView { .data = "gui pass", .length = WGPU_STRLEN };
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);

    // fullscreen blit of the scene target, then the gui draws on top
    wgpuRenderPassEncoderSetPipeline(pass, m_blit_pipeline->pipeline().handle());
    wgpuRenderPassEncoderSetBindGroup(pass, 0, blit_bind_group.handle(), 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);

    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
}

void ImGuiManager::shutdown()
{
    qDebug() << "Releasing ImGuiManager...";
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    if (m_queue)
        wgpuQueueRelease(m_queue);
}

bool ImGuiManager::want_capture_keyboard() { return ImGui::GetIO().WantCaptureKeyboard; }

bool ImGuiManager::want_capture_mouse() { return ImGui::GetIO().WantCaptureMouse; }

void ImGuiManager::on_sdl_event(SDL_Event& event) { ImGui_ImplSDL2_ProcessEvent(&event); }

void ImGuiManager::set_gui_visibility(bool visible) { m_gui_visible = visible; }

bool ImGuiManager::get_gui_visibility() const { return m_gui_visible; }

float ImGuiManager::s_tool_button_y = 0.0f;
ImFont* ImGuiManager::s_node_font = nullptr;
std::unordered_map<std::string, ImGuiManager::FilePickerState> ImGuiManager::s_picker_states;

#ifdef __EMSCRIPTEN__
void ImGuiManager::on_file_uploaded(const std::string& filename, const std::string& tag)
{
    auto it = s_picker_states.find(tag);
    if (it != s_picker_states.end() && it->second.is_open)
        it->second.pending.push_back(filename);
}
#endif

bool ImGuiManager::FilePicker(const char* dialog_id,
    const char* title,
    const char* filters,
    bool wants_open,
    std::vector<std::string>& out_paths,
    bool allow_multiple,
    const char* initial_path,
    FilePickerMode mode,
    const char* default_filename)
{
#ifdef __EMSCRIPTEN__
    if (mode == FilePickerMode::Save) {
        // Web: no file-system dialog for saves; return a synthetic MEMFS path immediately.
        if (wants_open) {
            out_paths.push_back(std::string("/download/") + default_filename);
            return true;
        }
        return false;
    }
    auto& state = s_picker_states[dialog_id];
    if (wants_open) {
        state.is_open = true;
        state.pending.clear();
        WebInterop::instance().open_file_dialog(filters, dialog_id, allow_multiple);
    }
    if (state.is_open && !state.pending.empty()) {
        out_paths = std::move(state.pending);
        state.pending.clear();
        state.is_open = false;
        return true;
    }
    return false;
#else
    if (wants_open) {
        IGFD::FileDialogConfig config;
        config.path = initial_path;
        config.flags = ImGuiFileDialogFlags_Modal;
        if (mode == FilePickerMode::Save) {
            config.countSelectionMax = 1;
            config.flags |= ImGuiFileDialogFlags_ConfirmOverwrite;
            config.fileName = default_filename;
        } else {
            config.countSelectionMax = allow_multiple ? 0 : 1;
        }
        ImGuiFileDialog::Instance()->OpenDialog(dialog_id, title, filters, config);
    }
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    const ImVec2 dialog_size(vp.x < 1000.0f ? vp.x * 0.9f : vp.x * 0.5f, vp.y < 1000.0f ? vp.y * 0.9f : vp.y * 0.5f);
    if (ImGuiFileDialog::Instance()->Display(dialog_id, ImGuiWindowFlags_NoCollapse, dialog_size, dialog_size)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            if (mode == FilePickerMode::Save) {
                out_paths.push_back(ImGuiFileDialog::Instance()->GetFilePathName());
            } else {
                for (auto& [name, path] : ImGuiFileDialog::Instance()->GetSelection())
                    out_paths.push_back(path);
            }
        }
        ImGuiFileDialog::Instance()->Close();
        return !out_paths.empty();
    }
    return false;
#endif
}

void ImGuiManager::finalize_save(const std::string& path)
{
#ifdef __EMSCRIPTEN__
    WebInterop::download_file(path);
#else
    (void)path;
#endif
}

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

void ImGuiManager::draw()
{
    if (!m_gui_visible)
        return;

    // Reset the floating tool-button stack for this frame (bottom-left, stacking upward).
    s_tool_button_y = ImGui::GetIO().DisplaySize.y - 48.0f - 40.0f;

    // Main sidebar window with CollapsingHeader sections.
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 430, 0)); // Set position to top-right corner
    ImGui::SetNextWindowSize(ImVec2(430, ImGui::GetIO().DisplaySize.y)); // Set height to full screen height, width as desired
    ImGui::Begin("weBIGeo", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    for (auto& panel : m_panels)
        panel->draw_panel();

    ImGui::End();

    for (auto& panel : m_panels)
        panel->draw();
}

} // namespace webgpu_app
