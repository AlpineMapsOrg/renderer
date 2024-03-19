/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "webgpu.hpp"
#include <webgpu/webgpu_interface.hpp>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include <imgui.h>
#include <imnodes.h>
#include "backends/imgui_impl_wgpu.h"
#endif

namespace webgpu_engine {

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
Window::Window(ObtainWebGpuSurfaceFunc obtain_webgpu_surface_func, ImGuiWindowImplInitFunc imgui_window_init_func,
    ImGuiWindowImplNewFrameFunc imgui_window_new_frame_func, ImGuiWindowImplShutdownFunc imgui_window_shutdown_func)
    : m_obtain_webgpu_surface_func { obtain_webgpu_surface_func }
    , m_imgui_window_init_func { imgui_window_init_func }
    , m_imgui_window_new_frame_func { imgui_window_new_frame_func }
    , m_imgui_window_shutdown_func { imgui_window_shutdown_func }
{
    // Constructor initialization logic here
}
#else
Window::Window(ObtainWebGpuSurfaceFunc obtain_webgpu_surface_func)
    : m_obtain_webgpu_surface_func { obtain_webgpu_surface_func }
{}
#endif

Window::~Window() {
    // Destructor cleanup logic here
}

void Window::initialise_gpu() {
    // GPU initialization logic here
    webgpuPlatformInit(); // platform dependent initialization code
    create_instance();
    init_surface();
    request_adapter();
    request_device();
    init_queue();

    create_buffers();
    create_bind_group_info();

    m_shader_manager = std::make_unique<ShaderModuleManager>(m_device);
    m_shader_manager->create_shader_modules();
    m_pipeline_manager = std::make_unique<PipelineManager>(m_device, *m_shader_manager);

    m_stopwatch.restart();
    emit gpu_ready_changed(true);
}

void Window::resize_framebuffer(int w, int h) {
    //TODO check we can do it without completely recreating swapchain and pipeline

    if (m_pipeline_manager->pipelines_created()) {
        m_pipeline_manager->release_pipelines();
    }

    if (m_swapchain != nullptr) {
        m_swapchain.release();
    }

    create_swapchain(w, h);
    m_pipeline_manager->create_pipelines(m_swapchain_format, *m_bind_group_info);

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    if (ImGui::GetCurrentContext() != nullptr) {
        // already initialized
        return;
    }

    if (!init_gui()) {
        std::cerr << "Could not initialize GUI!" << std::endl;
        throw std::runtime_error("could not initialize GUI");
    }
#endif
}

void Window::paint([[maybe_unused]] QOpenGLFramebufferObject* framebuffer) {
    // Painting logic here, using the optional framebuffer parameter which is currently unused

    wgpu::TextureView next_texture = m_swapchain.getCurrentTextureView();
    if (!next_texture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        throw std::runtime_error("Cannot acquire next swap chain texture");
    }

    emit update_camera_requested();

    //TODO remove, debugging
    const auto elapsed = static_cast<double>(m_stopwatch.total().count() / 1000.0f);
    uboSharedConfig* sc = &m_shared_config_ubo->data;
    sc->m_sun_light = QVector4D(0.0f, 1.0f, 1.0f, 1.0f);
    sc->m_sun_light_dir = QVector4D(elapsed, 1.0f, 1.0f, 1.0f);
    m_shared_config_ubo->update_gpu_data(m_queue);

    wgpu::CommandEncoderDescriptor command_encoder_desc {};
    command_encoder_desc.label = "Command Encoder";
    wgpu::CommandEncoder encoder = m_device.createCommandEncoder(command_encoder_desc);

    wgpu::RenderPassDescriptor render_pass_desc {};

    wgpu::RenderPassColorAttachment render_pass_color_attachment {};
    render_pass_color_attachment.view = next_texture;
    render_pass_color_attachment.resolveTarget = nullptr;
    render_pass_color_attachment.loadOp = wgpu::LoadOp::Clear;
    render_pass_color_attachment.storeOp = wgpu::StoreOp::Store;
    render_pass_color_attachment.clearValue = wgpu::Color { 0.9, 0.1, 0.2, 1.0 };

    // depthSlice field for RenderPassColorAttachment (https://github.com/gpuweb/gpuweb/issues/4251)
    // this field specifies the slice to render to when rendering to a 3d texture (view)
    // passing a valid index but referencing a non-3d texture leads to an error
    // TODO use some constant that represents "undefined" for this value (I couldn't find a constant for this?)
    //     (I just guessed -1 (max unsigned int value) and it worked)
    render_pass_color_attachment.depthSlice = -1;

    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &render_pass_color_attachment;

    render_pass_desc.depthStencilAttachment = nullptr;
    render_pass_desc.timestampWrites = nullptr;
    wgpu::RenderPassEncoder render_pass = encoder.beginRenderPass(render_pass_desc);

    m_bind_group_info->bind(render_pass, 0);
    render_pass.setPipeline(m_pipeline_manager->debug_config_and_camera_pipeline());
    render_pass.draw(36, 1, 0, 0);

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    // We add the GUI drawing commands to the render pass
    update_gui(render_pass);
#endif

    render_pass.end();
    render_pass.release();

    next_texture.release();

    wgpu::CommandBufferDescriptor cmd_buffer_descriptor {};
    cmd_buffer_descriptor.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmd_buffer_descriptor);
    encoder.release();
    m_queue.submit(command);
    command.release();

#ifndef __EMSCRIPTEN__
    // Swapchain in the WEB is handled by the browser!
    m_swapchain.present();
    m_instance.processEvents();
#endif
}

float Window::depth([[maybe_unused]] const glm::dvec2& normalised_device_coordinates) {
    // Implementation for calculating depth, parameters currently unused
    return 0.0f;
}

glm::dvec3 Window::position([[maybe_unused]] const glm::dvec2& normalised_device_coordinates) {
    // Calculate and return position from normalized device coordinates, parameters currently unused
    return glm::dvec3();
}

void Window::deinit_gpu() {
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    terminate_gui();
#endif

    m_pipeline_manager->release_pipelines();
    m_shader_manager->release_shader_modules();
    m_swapchain.release();
    m_queue.release();
    m_surface.release();
    //TODO triggers warning No Dawn device lost callback was set, but this is actually expected behavior
    m_device.release();
    m_adapter.release();
    m_instance.release();

    emit gpu_ready_changed(false);
}

void Window::set_aabb_decorator([[maybe_unused]] const nucleus::tile_scheduler::utils::AabbDecoratorPtr&) {
    // Logic for setting AABB decorator, parameter currently unused
}

void Window::remove_tile([[maybe_unused]] const tile::Id&) {
    // Logic to remove a tile, parameter currently unused
}

nucleus::camera::AbstractDepthTester* Window::depth_tester() {
    // Return this object as the depth tester
    return this;
}

void Window::set_permissible_screen_space_error([[maybe_unused]] float new_error) {
    // Logic for setting permissible screen space error, parameter currently unused
}

void Window::update_camera([[maybe_unused]] const nucleus::camera::Definition& new_definition) {
    // Logic for updating camera, parameter currently unused

    // UPDATE CAMERA UNIFORM BUFFER
    // NOTE: Could also just be done on camera or viewport change!
    const auto pos = new_definition.position();
    std::cout << "UBO update, new pos: " << pos.x << ", " << pos.y << ", " << pos.z << std::endl;
    uboCameraConfig* cc = &m_camera_config_ubo->data;
    cc->position = glm::vec4(new_definition.position(), 1.0);
    cc->view_matrix = new_definition.camera_matrix(); //TODO only for debug, actual line for terrain is below
    //cc->view_matrix = new_definition.local_view_matrix();
    cc->proj_matrix = new_definition.projection_matrix();
    cc->view_proj_matrix = cc->proj_matrix * cc->view_matrix;
    cc->inv_view_proj_matrix = glm::inverse(cc->view_proj_matrix);
    cc->inv_view_matrix = glm::inverse(cc->view_matrix);
    cc->inv_proj_matrix = glm::inverse(cc->proj_matrix);
    cc->viewport_size = new_definition.viewport_size();
    cc->distance_scaling_factor = new_definition.distance_scale_factor();
    m_camera_config_ubo->update_gpu_data(m_queue);
}

void Window::update_debug_scheduler_stats([[maybe_unused]] const QString& stats) {
    // Logic for updating debug scheduler stats, parameter currently unused
}

void Window::update_gpu_quads([[maybe_unused]] const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, [[maybe_unused]] const std::vector<tile::Id>& deleted_quads) {
    // Logic for updating GPU quads, parameters currently unused

    std::cout << "received " << new_quads.size() << " new quads, should delete " << deleted_quads.size() << " quads" << std::endl;
}

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
bool Window::init_gui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup ImNodes
    ImNodes::CreateContext();

    //ImGui::GetIO();

    // Setup Platform/Renderer backends
    m_imgui_window_init_func();
    ImGui_ImplWGPU_InitInfo init_info = {};
    init_info.Device = m_device;
    init_info.RenderTargetFormat = (WGPUTextureFormat)m_swapchain_format;
    init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    init_info.NumFramesInFlight = 3;
    ImGui_ImplWGPU_Init(&init_info);

    ImGui::StyleColorsLight();
    // set background of windows to 90% transparent
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.9f, 0.9f, 0.9f, 0.9f);
    ImNodes::StyleColorsLight();

    return true;
}

void Window::terminate_gui()
{
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    m_imgui_window_shutdown_func();
    ImGui_ImplWGPU_Shutdown();
}

void Window::update_gui(wgpu::RenderPassEncoder render_pass)
{
    ImGui_ImplWGPU_NewFrame();
    m_imgui_window_new_frame_func();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    static float frame_time = 0.0f;
    static std::vector<std::pair<int, int>> links;
    static bool show_node_editor = false;
    static bool first_frame = true;

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 300, 0)); // Set position to top-left corner
    ImGui::SetNextWindowSize(ImVec2(300, ImGui::GetIO().DisplaySize.y)); // Set height to full screen height, width as desired

    ImGui::Begin("weBIGeo", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    // FPS counter variables
    static float fpsValues[90] = {}; // Array to store FPS values for the graph, adjust size as needed for the time window
    static int fpsIndex = 0; // Current index in FPS values array
    static float lastTime = 0.0f; // Last time FPS was updated

    // Calculate delta time and FPS
    float currentTime = ImGui::GetTime();
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;
    float fps = 1.0f / deltaTime;

    // Store the current FPS value in the array
    fpsValues[fpsIndex] = fps;
    fpsIndex = (fpsIndex + 1) % IM_ARRAYSIZE(fpsValues); // Loop around the array


    frame_time = frame_time * 0.95f + (1000.0f / io.Framerate) * 0.05f;
    ImGui::PlotLines("", fpsValues, IM_ARRAYSIZE(fpsValues), fpsIndex, nullptr, 0.0f, 80.0f, ImVec2(280,100));
    ImGui::Text("Average: %.3f ms/frame (%.1f FPS)", frame_time, io.Framerate);

    ImGui::Separator();

    if (ImGui::Button(!show_node_editor ? "Show Node Editor" : "Hide Node Editor", ImVec2(280, 20))) {
        show_node_editor = !show_node_editor;
    }

    ImGui::End();

    if (first_frame) {
        ImNodes::SetNodeScreenSpacePos(1, ImVec2(50, 50));
        ImNodes::SetNodeScreenSpacePos(2, ImVec2(400, 50));
    }

    if (show_node_editor) {
        // ========== BEGIN NODE WINDOW ===========
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 300, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always);
        ImGui::Begin("Node Editor", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        // BEGINN NODE EDITOR
        ImNodes::BeginNodeEditor();

        // DRAW NODE 1
        ImNodes::BeginNode(1);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("input node");
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginOutputAttribute(2);
        ImGui::Text("data");
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();

        // DRAW NODE 2
        ImNodes::BeginNode(2);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("output node");
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(3);
        ImGui::Text("data");
        ImNodes::EndInputAttribute();

        ImNodes::BeginInputAttribute(4, ImNodesPinShape_Triangle);
        ImGui::Text("overlay");
        ImNodes::EndInputAttribute();

        ImNodes::EndNode();

        // IMNODES - DRAW LINKS
        int id = 0;
        for (const auto& p : links)
        {
            ImNodes::Link(id++, p.first, p.second);
        }

        // IMNODES - MINIMAP
        ImNodes::MiniMap(0.1f, ImNodesMiniMapLocation_BottomRight);

        ImNodes::EndNodeEditor();

        int start_attr, end_attr;
        if (ImNodes::IsLinkCreated(&start_attr, &end_attr))
        {
            links.push_back(std::make_pair(start_attr, end_attr));
        }



        ImGui::End();
    }



    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), render_pass);
    first_frame = false;
}
#endif


void Window::create_instance() {
    std::cout << "Creating WebGPU instance..." << std::endl;

#ifdef __EMSCRIPTEN__
    // instance = wgpu::createInstance(wgpu::InstanceDescriptor{}); would throw nullptr exception
    // but using vanilla webgpu with nullptr as descriptor seems to work.
    m_instance = wgpuCreateInstance(nullptr);
#else
    m_instance = wgpu::createInstance(wgpu::InstanceDescriptor{});
#endif
    if (!m_instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        throw std::runtime_error("Could not initialize WebGPU");
    }
    std::cout << "Got instance: " << m_instance << std::endl;
}

void Window::init_surface() { m_surface = m_obtain_webgpu_surface_func(m_instance); }

void Window::request_adapter()
{
    std::cout << "Requesting adapter..." << std::endl;
    wgpu::RequestAdapterOptions adapter_opts {};
    adapter_opts.compatibleSurface = m_surface;
    m_adapter = m_instance.requestAdapter(adapter_opts);
    std::cout << "Got adapter: " << m_adapter << std::endl;
}

void Window::request_device()
{
    std::cout << "Requesting device..." << std::endl;
    wgpu::DeviceDescriptor device_desc {};
    device_desc.label = "My Device";
    device_desc.requiredFeatureCount = 0;
    device_desc.requiredLimits = nullptr;
    device_desc.defaultQueue.label = "The default queue";
    m_device = m_adapter.requestDevice(device_desc);
    std::cout << "Got device: " << m_device << std::endl;

    auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
        std::cout << "Uncaptured device error: type " << type;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr /* pUserData */);
}

void Window::init_queue() { m_queue = m_device.getQueue(); }

void Window::create_buffers()
{
    m_shared_config_ubo = std::make_unique<UniformBuffer<uboSharedConfig>>();
    m_shared_config_ubo->init(m_device);

    m_camera_config_ubo = std::make_unique<UniformBuffer<uboCameraConfig>>();
    m_camera_config_ubo->init(m_device);
}

void Window::create_swapchain(uint32_t width, uint32_t height)
{
    std::cout << "Creating swapchain device..." << std::endl;
    // from Learn WebGPU C++ tutorial
#ifdef WEBGPU_BACKEND_WGPU
    m_swapchain_format = surface.getPreferredFormat(m_adapter);
#else
    m_swapchain_format = wgpu::TextureFormat::BGRA8Unorm;
#endif
    wgpu::SwapChainDescriptor swapchain_desc = {};
    swapchain_desc.width = width;
    swapchain_desc.height = height;
    swapchain_desc.usage = wgpu::TextureUsage::RenderAttachment;
    swapchain_desc.format = m_swapchain_format;
    swapchain_desc.presentMode = wgpu::PresentMode::Fifo;
    m_swapchain = m_device.createSwapChain(m_surface, swapchain_desc);
    std::cout << "Swapchain: " << m_swapchain << std::endl;
}

void Window::create_bind_group_info()
{
    m_bind_group_info = std::make_unique<BindGroupInfo>();
    m_bind_group_info->add_entry(0, *m_shared_config_ubo, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
    m_bind_group_info->add_entry(1, *m_camera_config_ubo, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
    m_bind_group_info->init(m_device);
}


}
