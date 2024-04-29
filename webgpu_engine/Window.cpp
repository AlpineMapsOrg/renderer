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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_interface.hpp>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include "backends/imgui_impl_wgpu.h"
#include <imgui.h>
#include <imnodes.h>
#endif

#include "raii/Sampler.h"
#include <QImage>
#include <thread>

#include <glm/gtx/string_cast.hpp>

namespace webgpu_engine {

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
Window::Window(ObtainWebGpuSurfaceFunc obtain_webgpu_surface_func, ImGuiWindowImplInitFunc imgui_window_init_func,
    ImGuiWindowImplNewFrameFunc imgui_window_new_frame_func, ImGuiWindowImplShutdownFunc imgui_window_shutdown_func)
    : m_obtain_webgpu_surface_func { obtain_webgpu_surface_func }
    , m_imgui_window_init_func { imgui_window_init_func }
    , m_imgui_window_new_frame_func { imgui_window_new_frame_func }
    , m_imgui_window_shutdown_func { imgui_window_shutdown_func }
    , m_tile_manager { std::make_unique<TileManager>() }

{
  // Constructor initialization logic here
}
#else
Window::Window(ObtainWebGpuSurfaceFunc obtain_webgpu_surface_func)
    : m_obtain_webgpu_surface_func { obtain_webgpu_surface_func }
    , m_tile_manager { std::make_unique<TileManager>() }
{
}
#endif

Window::~Window()
{
  // Destructor cleanup logic here
}

void Window::initialise_gpu()
{
    // GPU initialization logic here
    webgpuPlatformInit(); // platform dependent initialization code
    create_instance();
    init_surface();
    request_adapter();
    request_device();
    init_queue();

    create_buffers();

    WGPUSamplerDescriptor compose_sampler_filtering_desc {};
    compose_sampler_filtering_desc.label = "compose sampler filtering";
    compose_sampler_filtering_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    compose_sampler_filtering_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    compose_sampler_filtering_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    compose_sampler_filtering_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    compose_sampler_filtering_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    compose_sampler_filtering_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Linear;
    compose_sampler_filtering_desc.lodMinClamp = 0.0f;
    compose_sampler_filtering_desc.lodMaxClamp = 1.0f;
    compose_sampler_filtering_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    compose_sampler_filtering_desc.maxAnisotropy = 1;
    m_compose_sampler_filtering = std::make_unique<raii::Sampler>(m_device, compose_sampler_filtering_desc);

    WGPUSamplerDescriptor compose_sampler_nonfiltering_desc {};
    compose_sampler_nonfiltering_desc.label = "compose sampler";
    compose_sampler_nonfiltering_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    compose_sampler_nonfiltering_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    compose_sampler_nonfiltering_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    compose_sampler_nonfiltering_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Undefined;
    compose_sampler_nonfiltering_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Undefined;
    compose_sampler_nonfiltering_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Undefined;
    compose_sampler_nonfiltering_desc.lodMinClamp = 0.0f;
    compose_sampler_nonfiltering_desc.lodMaxClamp = 1.0f;
    compose_sampler_nonfiltering_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    compose_sampler_nonfiltering_desc.maxAnisotropy = 1;
    m_compose_sampler_nonfiltering = std::make_unique<raii::Sampler>(m_device, compose_sampler_nonfiltering_desc);

    m_shader_manager = std::make_unique<ShaderModuleManager>(m_device);
    m_shader_manager->create_shader_modules();
    m_pipeline_manager = std::make_unique<PipelineManager>(m_device, *m_shader_manager);
    m_pipeline_manager->create_pipelines();
    create_bind_groups();

    m_tile_manager->init(m_device, m_queue, *m_pipeline_manager);

    std::cout << "webgpu_engine::Window emitting: gpu_ready_changed" << std::endl;
    emit gpu_ready_changed(true);
}

void Window::resize_framebuffer(int w, int h)
{
    // TODO check we can do it without completely recreating swapchain
    if (m_swapchain != nullptr) {
        wgpuSwapChainRelease(m_swapchain);
    }

    create_depth_texture(w, h);

    create_swapchain(w, h);

    m_gbuffer_format = FramebufferFormat(m_pipeline_manager->tile_pipeline().framebuffer_format());
    m_gbuffer_format.size = glm::uvec2 { w, h };
    m_gbuffer = std::make_unique<Framebuffer>(m_device, m_gbuffer_format);

    FramebufferFormat atmosphere_framebuffer_format(m_pipeline_manager->atmosphere_pipeline().framebuffer_format());
    atmosphere_framebuffer_format.size = glm::uvec2(1, h);
    m_atmosphere_framebuffer = std::make_unique<Framebuffer>(m_device, atmosphere_framebuffer_format);

    m_compose_bind_group = std::make_unique<raii::BindGroup>(m_device, m_pipeline_manager->compose_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> { m_gbuffer->color_texture_view(0).create_bind_group_entry(0), // albedo texture
            m_gbuffer->color_texture_view(1).create_bind_group_entry(1), // position texture
            m_gbuffer->color_texture_view(2).create_bind_group_entry(2), // normal texture
            m_atmosphere_framebuffer->color_texture_view(0).create_bind_group_entry(3), // atmosphere texture
            m_compose_sampler_filtering->create_bind_group_entry(4), m_compose_sampler_nonfiltering->create_bind_group_entry(5) });

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

std::unique_ptr<raii::RenderPassEncoder> begin_render_pass(WGPUCommandEncoder encoder, WGPUTextureView color_attachment, WGPUTextureView depth_attachment)
{
    WGPURenderPassColorAttachment render_pass_color_attachment {};
    render_pass_color_attachment.view = color_attachment;
    render_pass_color_attachment.resolveTarget = nullptr;
    render_pass_color_attachment.loadOp = WGPULoadOp::WGPULoadOp_Clear;
    render_pass_color_attachment.storeOp = WGPUStoreOp::WGPUStoreOp_Store;
    render_pass_color_attachment.clearValue = WGPUColor { 0.0, 0.0, 0.0, 0.0 };

           // depthSlice field for RenderPassColorAttachment (https://github.com/gpuweb/gpuweb/issues/4251)
           // this field specifies the slice to render to when rendering to a 3d texture (view)
           // passing a valid index but referencing a non-3d texture leads to an error
           // TODO use some constant that represents "undefined" for this value (I couldn't find a constant for this?)
           //     (I just guessed -1 (max unsigned int value) and it worked)
    render_pass_color_attachment.depthSlice = -1;

    WGPURenderPassDepthStencilAttachment depth_stencil_attachment {};
    depth_stencil_attachment.view = depth_attachment;
    depth_stencil_attachment.depthClearValue = 1.0f;
    depth_stencil_attachment.depthLoadOp = WGPULoadOp::WGPULoadOp_Clear;
    depth_stencil_attachment.depthStoreOp = WGPUStoreOp::WGPUStoreOp_Store;
    depth_stencil_attachment.depthReadOnly = false;
    depth_stencil_attachment.stencilClearValue = 0;
    depth_stencil_attachment.stencilLoadOp = WGPULoadOp::WGPULoadOp_Undefined;
    depth_stencil_attachment.stencilStoreOp = WGPUStoreOp::WGPUStoreOp_Undefined;
    depth_stencil_attachment.stencilReadOnly = true;

    WGPURenderPassDescriptor render_pass_desc {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &render_pass_color_attachment;
    render_pass_desc.depthStencilAttachment = &depth_stencil_attachment;
    render_pass_desc.timestampWrites = nullptr;
    return std::make_unique<raii::RenderPassEncoder>(encoder, render_pass_desc);
}

void Window::paint([[maybe_unused]] QOpenGLFramebufferObject* framebuffer)
{
    // Painting logic here, using the optional framebuffer parameter which is currently unused

    WGPUTextureView next_texture = wgpuSwapChainGetCurrentTextureView(m_swapchain);
    if (!next_texture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        throw std::runtime_error("Cannot acquire next swap chain texture");
    }

    emit update_camera_requested();

           // TODO remove, debugging
           // uboSharedConfig* sc = &m_shared_config_ubo->data;
           // sc->m_sun_light = QVector4D(0.0f, 1.0f, 1.0f, 1.0f);
           // sc->m_sun_light_dir = QVector4D(elapsed, 1.0f, 1.0f, 1.0f);
           // m_shared_config_ubo->update_gpu_data(m_queue);

    WGPUCommandEncoderDescriptor command_encoder_desc {};
    command_encoder_desc.label = "Command Encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &command_encoder_desc);

           // render atmosphere to color buffer
    {
        std::unique_ptr<raii::RenderPassEncoder> render_pass = m_atmosphere_framebuffer->begin_render_pass(encoder);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, m_camera_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(render_pass->handle(), m_pipeline_manager->atmosphere_pipeline().pipeline().handle());
        wgpuRenderPassEncoderDraw(render_pass->handle(), 3, 1, 0, 0);
    }

           // render tiles to geometry buffers
    {
        std::unique_ptr<raii::RenderPassEncoder> render_pass = m_gbuffer->begin_render_pass(encoder);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, m_shared_config_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 1, m_camera_bind_group->handle(), 0, nullptr);

        const auto tile_set = m_tile_manager->generate_tilelist(m_camera);
        m_tile_manager->draw(render_pass->handle(), m_camera, tile_set, true, m_camera.position());
    }

           // render geometry buffers to color buffer (the texture view obtained from the swapchain)
    {
        std::unique_ptr<raii::RenderPassEncoder> render_pass = begin_render_pass(encoder, next_texture, m_depth_texture_view->handle());
        wgpuRenderPassEncoderSetPipeline(render_pass->handle(), m_pipeline_manager->compose_pipeline().pipeline().handle());
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 0, m_shared_config_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 1, m_camera_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass->handle(), 2, m_compose_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass->handle(), 3, 1, 0, 0);

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
        // We add the GUI drawing commands to the render pass
        update_gui(render_pass->handle());
#endif
    }

    wgpuTextureViewRelease(next_texture);

    WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
    cmd_buffer_descriptor.label = "Command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmd_buffer_descriptor);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);

#ifndef __EMSCRIPTEN__
    // Swapchain in the WEB is handled by the browser!
    wgpuSwapChainPresent(m_swapchain);
    wgpuInstanceProcessEvents(m_instance);
    wgpuDeviceTick(m_device);
#endif
}


glm::vec4 Window::synchronous_position_readback(const glm::dvec2& ndc) {
    if (m_readback_in_progress) return {};

    // A little bit silly, but we have to transform it back to device coordinates
    glm::uvec2 device_coordinates = {
        (ndc.x + 1) * 0.5 * m_swapchain_size.x,
        (1 - (ndc.y + 1) * 0.5) * m_swapchain_size.y
    };

    // clamp device coordinates to the swapchain size
    device_coordinates = glm::clamp(device_coordinates, glm::uvec2(0), glm::uvec2(m_swapchain_size - glm::vec2(1.0)));

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, {});

           // readback depth buffer
    m_position_readback_buffer = std::make_unique<raii::RawBuffer<glm::vec4>>(m_device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, 256 / sizeof(glm::vec4), "readback buffer");

    const auto& src_texture = m_gbuffer->color_texture(1);
    // Define Source Texture
    WGPUImageCopyTexture image_copy_texture_source = {
        .nextInChain = nullptr,
        .texture = src_texture.handle(),
        .mipLevel = 0,
        .origin = {device_coordinates.x,device_coordinates.y,0},
        .aspect = {}
    };
    // Define destination buffer
    WGPUTextureDataLayout texture_data_layout = {
        .nextInChain = nullptr,
        .offset      = 0,
        .bytesPerRow = 256, // multiple of 256
        .rowsPerImage = 1,
    };
    WGPUImageCopyBuffer image_copy_buffer_destination = {
        .nextInChain = nullptr,
        .layout = texture_data_layout,
        .buffer = m_position_readback_buffer->handle(),
    };
    WGPUExtent3D image_copy_extent = {
        .width  = 1,
        .height = 1,
        .depthOrArrayLayers = 1,
    };
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &image_copy_texture_source, &image_copy_buffer_destination, &image_copy_extent);

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, {});
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);

    m_readback_in_progress = true;
    wgpuQueueOnSubmittedWorkDone(m_queue, []([[maybe_unused]]WGPUQueueWorkDoneStatus status, void* pUserData) {
            auto onBufferMapped = [](WGPUBufferMapAsyncStatus status, void* pUserData) {
                Window* _this = reinterpret_cast<Window*>(pUserData);

                if (status != WGPUBufferMapAsyncStatus_Success) {
                    std::cout << "Error in buffer mapping" << std::endl;
                    _this->m_readback_in_progress = false;
                };

                glm::vec4* bufferData = (glm::vec4*)wgpuBufferGetConstMappedRange(_this->m_position_readback_buffer->handle(), 0, sizeof(glm::vec4));
                _this->m_position_readback_result = bufferData[0];
                wgpuBufferUnmap(_this->m_position_readback_buffer->handle());
                _this->m_readback_in_progress = false;
            };
            Window* _this = reinterpret_cast<Window*>(pUserData);
            wgpuBufferMapAsync(_this->m_position_readback_buffer->handle(), WGPUMapMode_Read, 0, sizeof(glm::vec4), onBufferMapped, pUserData);
        }, this);

    int sleepcnt = 0;
    const int max_timeout = 1000;
    while (m_readback_in_progress) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(1);    // using asyncify to return to js event loop
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        wgpuDeviceTick(m_device); // polling events for DAWN
#endif
        wgpuInstanceProcessEvents(m_instance); // not sure if necessary

        sleepcnt++;
        if (sleepcnt > max_timeout) {
            std::cout << "Timeout in readback" << std::endl;
            break;
        }
    }

    std::cout << "Waited for " << sleepcnt << "ms" << std::endl;

    std::cout << "Position: " << glm::to_string(m_position_readback_result) << std::endl;
    return m_position_readback_result;
}


float Window::depth([[maybe_unused]] const glm::dvec2& normalised_device_coordinates)
{
    auto position = synchronous_position_readback(normalised_device_coordinates);
    return position.z;
}

glm::dvec3 Window::position([[maybe_unused]] const glm::dvec2& normalised_device_coordinates)
{
    // If we read position directly no reconstruction is necessary
    //glm::dvec3 reconstructed = m_camera.position() + m_camera.ray_direction(normalised_device_coordinates) * (double)depth(normalised_device_coordinates);
    auto position = synchronous_position_readback(normalised_device_coordinates);
    return m_camera.position() + glm::dvec3(position.x, position.y, position.z);
}

void Window::deinit_gpu()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    terminate_gui();
#endif

    m_pipeline_manager->release_pipelines();
    m_shader_manager->release_shader_modules();
    wgpuSwapChainRelease(m_swapchain);
    wgpuQueueRelease(m_queue);
    wgpuSurfaceRelease(m_surface);
    // TODO triggers warning No Dawn device lost callback was set, but this is actually expected behavior
    wgpuDeviceRelease(m_device);
    wgpuAdapterRelease(m_adapter);
    wgpuInstanceRelease(m_instance);

    emit gpu_ready_changed(false);
}

void Window::set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr& aabb_decorator) { m_tile_manager->set_aabb_decorator(aabb_decorator); }

void Window::remove_tile(const tile::Id& id) { return m_tile_manager->remove_tile(id); }

void Window::set_quad_limit(unsigned int new_limit) { m_tile_manager->set_quad_limit(new_limit); }

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

void Window::set_permissible_screen_space_error([[maybe_unused]] float new_error)
{
  // Logic for setting permissible screen space error, parameter currently unused
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
}

void Window::update_debug_scheduler_stats([[maybe_unused]] const QString& stats)
{
  // Logic for updating debug scheduler stats, parameter currently unused
}

void Window::update_gpu_quads([[maybe_unused]] const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads,
    [[maybe_unused]] const std::vector<tile::Id>& deleted_quads)
{
    // std::cout << "received " << new_quads.size() << " new quads, should delete " << deleted_quads.size() << " quads" << std::endl;
    m_tile_manager->update_gpu_quads(new_quads, deleted_quads);
}

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
bool Window::init_gui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

           // Setup ImNodes
    ImNodes::CreateContext();

           // ImGui::GetIO();

           // Setup Platform/Renderer backends
    m_imgui_window_init_func();
    ImGui_ImplWGPU_InitInfo init_info = {};
    init_info.Device = m_device;
    init_info.RenderTargetFormat = (WGPUTextureFormat)m_swapchain_format;
    init_info.DepthStencilFormat = m_depth_texture_format;
    // WebGPU may do frame buffering implicitly. (https://groups.google.com/g/dawn-graphics/c/OuEzF3SUo6Y)
    // I'm not sure wether it actually uses 3 frames in flight, but I guess better to have this value set to more than less?
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
    ImGui_ImplWGPU_Shutdown();
    m_imgui_window_shutdown_func();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
}

void Window::update_gui(WGPURenderPassEncoder render_pass)
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
    ImGui::PlotLines("", fpsValues, IM_ARRAYSIZE(fpsValues), fpsIndex, nullptr, 0.0f, 80.0f, ImVec2(280, 100));
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
        for (const auto& p : links) {
            ImNodes::Link(id++, p.first, p.second);
        }

               // IMNODES - MINIMAP
        ImNodes::MiniMap(0.1f, ImNodesMiniMapLocation_BottomRight);

        ImNodes::EndNodeEditor();

        int start_attr, end_attr;
        if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
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

void Window::create_instance()
{
    std::cout << "Creating WebGPU instance..." << std::endl;
    m_instance = wgpuCreateInstance(nullptr);
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
    WGPURequestAdapterOptions adapter_opts {};
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;
    adapter_opts.compatibleSurface = m_surface;
    m_adapter = requestAdapterSync(m_instance, adapter_opts);
    std::cout << "Got adapter: " << m_adapter << std::endl;
}

void Window::request_device()
{
    std::cout << "Requesting device..." << std::endl;

    const WGPURequiredLimits required_limits = required_gpu_limits();

    WGPUDeviceDescriptor device_desc {};
    device_desc.label = "My Device";
    device_desc.requiredFeatureCount = 0;
    device_desc.requiredLimits = &required_limits;
    device_desc.defaultQueue.label = "The default queue";
    m_device = requestDeviceSync(m_adapter, device_desc);
    std::cout << "Got device: " << m_device << std::endl;

    auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
        std::cout << "Uncaptured device error: type " << type;
        if (message)
            std::cout << " (" << message << ")";
        std::cout << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr /* pUserData */);
}

void Window::init_queue() { m_queue = wgpuDeviceGetQueue(m_device); }

void Window::create_buffers()
{
    m_shared_config_ubo
        = std::make_unique<raii::Buffer<uboSharedConfig>>(m_device, WGPUBufferUsage::WGPUBufferUsage_CopyDst | WGPUBufferUsage::WGPUBufferUsage_Uniform);
    m_camera_config_ubo
        = std::make_unique<raii::Buffer<uboCameraConfig>>(m_device, WGPUBufferUsage::WGPUBufferUsage_CopyDst | WGPUBufferUsage::WGPUBufferUsage_Uniform);
}

void Window::create_depth_texture(uint32_t width, uint32_t height)
{
    std::cout << "creating depth texture width=" << width << ", height=" << height << std::endl;
    WGPUTextureFormat format = m_depth_texture_format;
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = "depth texture";
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.format = m_depth_texture_format;
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.size = { width, height, 1 };
    texture_desc.usage = WGPUTextureUsage::WGPUTextureUsage_RenderAttachment;
    texture_desc.viewFormatCount = 1;
    texture_desc.viewFormats = &format;
    m_depth_texture = std::make_unique<raii::Texture>(m_device, texture_desc);

    WGPUTextureViewDescriptor view_desc {};
    view_desc.aspect = WGPUTextureAspect::WGPUTextureAspect_DepthOnly;
    view_desc.arrayLayerCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseMipLevel = 0;
    view_desc.dimension = WGPUTextureViewDimension::WGPUTextureViewDimension_2D;
    view_desc.format = texture_desc.format;
    m_depth_texture_view = m_depth_texture->create_view(view_desc);
}

void Window::create_swapchain(uint32_t width, uint32_t height)
{
    std::cout << "Creating swapchain device..." << std::endl;
    // from Learn WebGPU C++ tutorial
#ifdef WEBGPU_BACKEND_WGPU
    m_swapchain_format = surface.getPreferredFormat(m_adapter);
#else
    m_swapchain_format = WGPUTextureFormat::WGPUTextureFormat_BGRA8Unorm;
#endif
    WGPUSwapChainDescriptor swapchain_desc = {};
    swapchain_desc.width = width;
    swapchain_desc.height = height;
    swapchain_desc.usage = WGPUTextureUsage::WGPUTextureUsage_RenderAttachment;
    swapchain_desc.format = m_swapchain_format;
    swapchain_desc.presentMode = WGPUPresentMode::WGPUPresentMode_Fifo;
    m_swapchain = wgpuDeviceCreateSwapChain(m_device, m_surface, &swapchain_desc);
    m_swapchain_size = glm::vec2(width, height);
    std::cout << "Swapchain: " << m_swapchain << std::endl;
}

void Window::create_bind_groups()
{
    m_shared_config_bind_group = std::make_unique<raii::BindGroup>(m_device, m_pipeline_manager->shared_config_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> { m_shared_config_ubo->raw_buffer().create_bind_group_entry(0) });

    m_camera_bind_group = std::make_unique<raii::BindGroup>(m_device, m_pipeline_manager->camera_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> { m_camera_config_ubo->raw_buffer().create_bind_group_entry(0) });
}

WGPURequiredLimits Window::required_gpu_limits() const
{
    WGPURequiredLimits required_limits {};

           // irrelevant for us, but needs to be set
    required_limits.limits.minStorageBufferOffsetAlignment = std::numeric_limits<uint32_t>::max();
    required_limits.limits.minUniformBufferOffsetAlignment = std::numeric_limits<uint32_t>::max();

       // wgpuAdapterGetLimits is not supported in Chrome yet
#ifndef __EMSCRIPTEN__
    WGPUSupportedLimits supported_limits {};
    wgpuAdapterGetLimits(m_adapter, &supported_limits);
    required_limits.limits.maxTextureArrayLayers = supported_limits.limits.maxTextureArrayLayers;
#else
    // use 256 as it is supported on all devices according to https://web3dsurvey.com/webgpu/limits/maxTextureArrayLayers
    required_limits.limits.maxTextureArrayLayers = 256;
#endif

    required_limits.limits.maxColorAttachmentBytesPerSample
        = 64; // supported on 71% of devices (https://web3dsurvey.com/webgpu/limits/maxColorAttachmentBytesPerSample)

    return required_limits;
}

} // namespace webgpu_engine
