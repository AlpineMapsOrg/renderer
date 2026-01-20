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

#include "TerrainRenderer.h"

#include "webgpu_engine/Window.h"
#include <QCoreApplication>
#include <QFile>
#include <cassert>
#include <qthread.h> //TODO maybe only for threading enabled?
#include <webgpu/webgpu_interface.hpp>

#ifdef __EMSCRIPTEN__
#include "WebInterop.h"
#include <emscripten/emscripten.h>
#else
#include "nucleus/utils/image_loader.h"
#pragma comment(lib, "dwmapi.lib")
#endif

#include "util/dark_mode.h"

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include "imgui.h"
#endif
#include "util/error_logging.h"

#include <nucleus/DataQuerier.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/tile/SchedulerDirector.h>
#include <nucleus/tile/setup.h>
#include <nucleus/timing/CpuTimer.h>
#include <webgpu_engine/Context.h>
#include <webgpu_engine/TileGeometry.h>

namespace webgpu_app {

TerrainRenderer::TerrainRenderer()
{
#ifdef __EMSCRIPTEN__
    // execute on window resize when canvas size changes
    QObject::connect(&WebInterop::instance(), &WebInterop::body_size_changed, this, &TerrainRenderer::set_window_size);

    m_surface_presentmode = WGPUPresentMode_Fifo; // chrome does not want other present modes
#endif
}

void TerrainRenderer::init_window() {
    // Initializes SDL2 video subsystem
    SDL_SetMainReady();
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        qFatal("Could not initialize SDL2 video subsystem! SDL_Error: %s", SDL_GetError());
    }

#ifdef __EMSCRIPTEN__
    // Fetch size of the webpage
    m_viewport_size = WebInterop::instance().get_body_size();
#endif
    m_sdl_window = SDL_CreateWindow("weBIGeo - Geospatial Visualization Tool", // Window title
        SDL_WINDOWPOS_CENTERED, // Window position x
        SDL_WINDOWPOS_CENTERED, // Window position y
        m_viewport_size.x, // Window width
        m_viewport_size.y, // Window height
        SDL_WINDOW_RESIZABLE); // SDL_WINDOW_VULKAN

    if (!m_sdl_window) {
        SDL_Quit();
        qFatal("Could not create SDL window! SDL_Error: %s", SDL_GetError());
    }

    util::enable_darkmode_on_windows(m_sdl_window);

#ifndef __EMSCRIPTEN__
    // Load icon using the existing image loader
    auto icon = nucleus::utils::image_loader::rgba8(":/icons/logo32.png").value();
    // Create SDL_Surface from the raw image data
    SDL_Surface* iconSurface = SDL_CreateRGBSurfaceFrom((void*)icon.bytes(), // Pixel data
        icon.width(), // Image width
        icon.height(), // Image height
        32, // Bits per pixel (RGBA = 32 bits)
        icon.width() * 4, // Pitch (width * 4 bytes per pixel)
        0x000000ff, // Red mask
        0x0000ff00, // Green mask
        0x00ff0000, // Blue mask
        0xff000000 // Alpha mask
    );

    if (iconSurface) {
        SDL_SetWindowIcon(m_sdl_window, iconSurface); // Set the window icon
        SDL_FreeSurface(iconSurface); // Free the surface after setting the icon
    } else {
        qWarning("Could not create SDL surface for window icon. SDL_Error: %s", SDL_GetError());
    }
#endif
}

void TerrainRenderer::render_gui()
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    static bool vsync_enabled = (m_surface_presentmode == WGPUPresentMode::WGPUPresentMode_Fifo);
    if (ImGui::Checkbox("VSync", &vsync_enabled)) {
        m_surface_presentmode = vsync_enabled ? WGPUPresentMode::WGPUPresentMode_Fifo : WGPUPresentMode::WGPUPresentMode_Immediate;
        // Reconfigure surface
        m_force_repaint_once = true;
        this->on_window_resize(m_viewport_size.x, m_viewport_size.y);
    }
    ImGui::Checkbox("Repaint each frame", &m_force_repaint);
    ImGui::Text("Repaint-Counter: %d", m_repaint_count);

    if (ImGui::Button("Reload shaders [F5]", ImVec2(350, 20))) {
        m_webgpu_window->reload_shaders();
    }
#endif
}

void TerrainRenderer::poll_events()
{
    // TODO hack, makes animations work
    m_camera_controller->advance_camera();

    // NOTE: The following line is not strictly necessary, we discovered that SDL somehow
    // triggers the processing of qt events. On the web we assume that Qt attaches itself to
    // the emscripten event loop.
    QCoreApplication::processEvents();
    // Poll SDL events and handle them.
    // (contrary to GLFW, close event is not automatically managed, and there
    // is no callback mechanism by default.)
    static SDL_Event events[15]; // Only allocate memory once (11 is the max events at once i witnessed)
    bool events_contain_touch = false;
    int event_count = 0;
    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        m_gui_manager->on_sdl_event(event);
        if (event.type == SDL_QUIT) {
            m_window_open = false;
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                on_window_resize(event.window.data1, event.window.data2);
            }
        } else {
            events[event_count++] = event;
            if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP || event.type == SDL_FINGERMOTION) {
                events_contain_touch = true;
            }
        }
    }

    // IMPORTANT: SDL seems to emulate touch events as mouse events aswell. In order to avoid this
    // we need to filter out the mouse events if there are touch events. Meaning we priortize touch over mouse.
    for (int i = 0; i < event_count; i++) {
        if (events_contain_touch && (events[i].type == SDL_MOUSEMOTION || events[i].type == SDL_MOUSEBUTTONDOWN || events[i].type == SDL_MOUSEBUTTONUP)) {
            continue;
        }
        m_input_mapper->on_sdl_event(events[i]);
    }

    wgpuInstanceProcessEvents(m_instance);
}

void TerrainRenderer::render() {
    // Do nothing, this checks for ongoing asynchronous operations and call their callbacks

    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(m_surface, &surface_texture);

    m_cputimer->start();

    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
        && surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        // skip frame (?)
        qDebug() << "Could not get current surface texture: surface_texture.status=" << surface_texture.status;
        return;
    }

    WGPUTextureViewDescriptor viewDescriptor {};
    viewDescriptor.nextInChain = nullptr;
    viewDescriptor.label = WGPUStringView { .data = "Surface texture view", .length = WGPU_STRLEN };
    viewDescriptor.format = wgpuTextureGetFormat(surface_texture.texture);
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = WGPUTextureAspect_All;
    WGPUTextureView surface_texture_view = wgpuTextureCreateView(surface_texture.texture, &viewDescriptor);

    if (!surface_texture_view) {
        qFatal("Cannot acquire next surface texture");
    }

    WGPUCommandEncoderDescriptor command_encoder_desc {};
    command_encoder_desc.label = WGPUStringView { .data = "Command Encoder", .length = WGPU_STRLEN };
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &command_encoder_desc);

    if (webgpu::isTimingSupported())
        m_gputimer->start(encoder);

    m_frame_count++;
    if (m_webgpu_window->needs_redraw() || m_force_repaint || m_force_repaint_once) {
        m_webgpu_window->paint(m_framebuffer.get(), encoder);
        m_repaint_count++;
        m_force_repaint_once = false;
    }

    {
        webgpu::raii::RenderPassEncoder render_pass(encoder, surface_texture_view, nullptr);
        wgpuRenderPassEncoderSetPipeline(render_pass.handle(), m_gui_pipeline.get()->pipeline().handle());
        wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 0, m_gui_bind_group->handle(), 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass.handle(), 3, 1, 0, 0);

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
        // We add the GUI drawing commands to the render pass
        m_gui_manager->render(render_pass.handle());
#endif
    }

    if (webgpu::isTimingSupported())
        m_gputimer->stop(encoder);

    wgpuTextureViewRelease(surface_texture_view);

    WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
    cmd_buffer_descriptor.label = WGPUStringView { .data = "Command buffer", .length = WGPU_STRLEN };
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmd_buffer_descriptor);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);

    if (webgpu::isTimingSupported())
        m_gputimer->resolve();

    m_cputimer->stop();

#ifndef __EMSCRIPTEN__
    // Surface present in the WEB is handled by the browser!
    wgpuSurfacePresent(m_surface);
    wgpuDeviceTick(m_device);
#endif
}

void TerrainRenderer::start() {
    init_window();

    webgpu_create_context();

    m_context = std::make_unique<RenderingContext>();
    m_context->initialize(m_instance, m_device);

    m_camera_controller = std::make_unique<nucleus::camera::Controller>(nucleus::camera::PositionStorage::instance()->get("grossglockner"), m_webgpu_window.get(), m_context->data_querier());

    // clang-format off
    // NOTICE ME!!!! READ THIS, IF YOU HAVE TROUBLES WITH SIGNALS NOT REACHING THE QML RENDERING THREAD!!!!111elevenone
    // In Qt/QML the rendering thread goes to sleep (at least until Qt 6.5, See RenderThreadNotifier).
    // At the time of writing, an additional connection from tile_ready and tile_expired to the notifier is made.
    // this only works if ALP_ENABLE_THREADING is on, i.e., the tile scheduler is on an extra thread. -> potential issue on webassembly
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_context->geometry_scheduler(), &nucleus::tile::Scheduler::update_camera);
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_context->ortho_scheduler(),    &nucleus::tile::Scheduler::update_camera);
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_webgpu_window.get(),           &webgpu_engine::Window::update_camera);
    
    connect(m_context->geometry_scheduler(), &nucleus::tile::GeometryScheduler::gpu_tiles_updated, m_webgpu_window.get(), &webgpu_engine::Window::update_requested);
    connect(m_context->ortho_scheduler(),    &nucleus::tile::TextureScheduler::gpu_tiles_updated,  m_webgpu_window.get(), &webgpu_engine::Window::update_requested);
    // clang-format on

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    m_gui_manager = std::make_unique<GuiManager>(this);
#endif

    m_input_mapper = std::make_unique<InputMapper>(this, m_camera_controller.get(), m_gui_manager.get(), [this]() { return m_viewport_size; });

    // TODO connect this (is used from GuiManager to update camera when settings are changed)
    //  connect(this, &TerrainRenderer::update_camera_requested, camera_controller, &nucleus::camera::Controller::update_camera_request);
    connect(m_webgpu_window.get(),
        &webgpu_engine::Window::set_camera_definition_requested,
        m_camera_controller.get(),
        &nucleus::camera::Controller::set_model_matrix);

    connect(m_webgpu_window.get(), &nucleus::AbstractRenderWindow::update_requested, this, &TerrainRenderer::schedule_update);
    connect(m_input_mapper.get(), &InputMapper::key_pressed, this, &TerrainRenderer::handle_shortcuts);

    m_webgpu_window->set_wgpu_context(m_instance, m_device, m_adapter, m_surface, m_queue, m_context->engine_context());
    m_webgpu_window->initialise_gpu();

    // Configures surface
    this->on_window_resize(m_viewport_size.x, m_viewport_size.y);

    { // load first camera definition without changing preset in nucleus
        auto new_definition = nucleus::camera::stored_positions::grossglockner();
        new_definition.set_viewport_size(m_viewport_size);
        m_camera_controller->set_model_matrix(new_definition);
    }

    qDebug() << "Create GUI Pipeline...";
    m_gui_ubo
        = std::make_unique<webgpu::raii::RawBuffer<TerrainRenderer::GuiPipelineUBO>>(m_device, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, 1, "gui ubo");
    m_gui_ubo->write(m_queue, &m_gui_ubo_data);

    webgpu::FramebufferFormat format {};
    format.color_formats.emplace_back(m_surface_texture_format);

    WGPUBindGroupLayoutEntry backbuffer_texture_entry {};
    backbuffer_texture_entry.binding = 0;
    backbuffer_texture_entry.visibility = WGPUShaderStage_Fragment;
    backbuffer_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    backbuffer_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry gui_ubo_entry = {};
    gui_ubo_entry.binding = 1;
    gui_ubo_entry.visibility = WGPUShaderStage_Fragment;
    gui_ubo_entry.buffer.type = WGPUBufferBindingType_Uniform;
    gui_ubo_entry.buffer.minBindingSize = sizeof(TerrainRenderer::GuiPipelineUBO);

    m_gui_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(
        m_device, std::vector<WGPUBindGroupLayoutEntry> { backbuffer_texture_entry, gui_ubo_entry }, "gui bind group layout");

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

    m_gui_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(m_device, *shader_module, *shader_module,
        std::vector<webgpu::util::SingleVertexBufferInfo> {}, format, std::vector<const webgpu::raii::BindGroupLayout*> { m_gui_bind_group_layout.get() });

    m_gui_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_device, *m_gui_bind_group_layout.get(),
        std::initializer_list<WGPUBindGroupEntry> { m_framebuffer->color_texture_view(0).create_bind_group_entry(0), m_gui_ubo->create_bind_group_entry(1) });

    m_timer_manager = std::make_unique<webgpu::timing::GuiTimerManager>();
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    m_gui_manager->init(m_sdl_window, m_device, m_surface_texture_format, WGPUTextureFormat_Undefined);
#endif

    m_cputimer = std::make_shared<webgpu::timing::CpuTimer>(120);
    m_timer_manager->add_timer(m_cputimer, "CPU Timer", "Renderer");
    if (webgpu::isTimingSupported()) {
        m_gputimer = std::make_shared<webgpu::timing::WebGpuTimer>(m_device, 3, 120);
        m_timer_manager->add_timer(m_gputimer, "GPU Timer", "Renderer");
    }

    this->on_window_resize(m_viewport_size.x, m_viewport_size.y);
    m_initialized = true;

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(
        [](void* userData) {
            TerrainRenderer& renderer = *reinterpret_cast<TerrainRenderer*>(userData);
            renderer.poll_events();
            renderer.render();
        },
        (void*)this, 0, true);
#else
    while (m_window_open) {
        poll_events();
        render();
    }
#endif

    // NOTE: Ressources are freed by the browser when the page is closed. Also keep in mind
    // that this part of code will be executed immediately since the main loop is not blocking.
#ifndef __EMSCRIPTEN__
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    m_gui_manager->shutdown();
#endif
    webgpu_release_context();
    m_webgpu_window->destroy();
    m_context->destroy();

    SDL_DestroyWindow(m_sdl_window);
    SDL_Quit();
    m_initialized = false;
#endif
}

void TerrainRenderer::set_window_size(glm::uvec2 size)
{
    if (m_viewport_size == size)
        return;
    m_viewport_size = size;
    if (m_initialized) {
        SDL_SetWindowSize(m_sdl_window, size.x, size.y);
        on_window_resize(size.x, size.y);
    }
}

void TerrainRenderer::handle_shortcuts(QKeyCombination key)
{
    if (key.key() == Qt::Key_F5) {
        m_webgpu_window->reload_shaders();
    } else if (key.key() == Qt::Key_H) {
        m_gui_manager->set_gui_visibility(!m_gui_manager->get_gui_visibility());
    }
}

void TerrainRenderer::schedule_update() { m_force_repaint_once = true; }

void TerrainRenderer::create_framebuffer(uint32_t width, uint32_t height)
{
    qDebug() << "creating framebuffer textures for size " << width << "x" << height;

    webgpu::FramebufferFormat format { .size = { width, height }, .depth_format = m_depth_texture_format, .color_formats = { m_surface_texture_format } };
    m_framebuffer = std::make_unique<webgpu::Framebuffer>(m_device, format);

    if (m_gui_bind_group) {
        m_gui_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_device, *m_gui_bind_group_layout.get(),
            std::initializer_list<WGPUBindGroupEntry> {
                m_framebuffer->color_texture_view(0).create_bind_group_entry(0), m_gui_ubo->create_bind_group_entry(1) });
    }

    if (m_gui_ubo) {
        m_gui_ubo_data.resolution = glm::vec2(m_viewport_size);
        m_gui_ubo->write(m_queue, &m_gui_ubo_data);
    }
}

void TerrainRenderer::configure_surface(uint32_t width, uint32_t height)
{
    qDebug() << "configuring surface...";

    // from Learn WebGPU C++ tutorial

    WGPUSurfaceCapabilities surface_capabilities {};
    wgpuSurfaceGetCapabilities(m_surface, m_adapter, &surface_capabilities);
    if (surface_capabilities.formatCount < 1) {
        qFatal() << "WebGPU surface formatCount is 0 - must support at least one format";
    }

    m_surface_texture_format = surface_capabilities.formats[0];
    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.width = width;
    config.height = height;
    config.format = m_surface_texture_format;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.device = m_device;
    config.presentMode = m_surface_presentmode;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    qInfo() << "trying to configure surface with size " << width << "x" << height << "alpha mode=" << config.alphaMode
            << ", present mode=" << m_surface_presentmode;
    wgpuSurfaceConfigure(m_surface, &config);
    qInfo() << "configured surface with size " << width << "x" << height << ", present mode=" << m_surface_presentmode;
}

void TerrainRenderer::update_camera() { emit update_camera_requested(); }

void TerrainRenderer::on_window_resize(int width, int height) {
    m_viewport_size = { width, height };

    configure_surface(width, height);
    create_framebuffer(width, height);

    m_webgpu_window->resize_framebuffer(m_viewport_size.x, m_viewport_size.y);
    m_camera_controller->set_viewport(m_viewport_size);
}

void TerrainRenderer::webgpu_create_context()
{
    qDebug() << "Creating WebGPU instance...";
    m_instance_desc = {};
    m_instance_desc.nextInChain = nullptr;

#ifndef __EMSCRIPTEN__
    WGPUDawnTogglesDescriptor dawnToggles;
    dawnToggles.chain.next = nullptr;
    dawnToggles.chain.sType = WGPUSType_DawnTogglesDescriptor;

    std::vector<const char*> enabledToggles = { "allow_unsafe_apis" };
#if defined(QT_DEBUG)
    // TODO: Figure out why this doesnt work
    enabledToggles.push_back("use_user_defined_labels_in_backend");
    enabledToggles.push_back("enable_vulkan_validation");
    enabledToggles.push_back("disable_symbol_renaming");
#endif

    QStringList toggleList;
    for (const auto& toggle : enabledToggles)
        toggleList << QString::fromStdString(toggle);
    qDebug() << "Dawn toggles:" << toggleList.join(", ");

    dawnToggles.enabledToggles = enabledToggles.data();
    dawnToggles.enabledToggleCount = enabledToggles.size();
    dawnToggles.disabledToggleCount = 0;

    m_instance_desc.nextInChain = &dawnToggles.chain;
#endif

    const auto timed_wait_feature = WGPUInstanceFeatureName_TimedWaitAny;
    m_instance_desc.requiredFeatureCount = 1;
    m_instance_desc.requiredFeatures = &timed_wait_feature;

    m_instance = wgpuCreateInstance(&m_instance_desc);

    if (!m_instance) {
        qFatal("Could not initialize WebGPU Instance!");
    }
    qInfo() << "Got instance: " << m_instance;

    qDebug() << "Requesting surface...";
    m_surface = SDL_GetWGPUSurface(m_instance, m_sdl_window);
    if (!m_surface) {
        qFatal("Could not create surface!");
    }
    qInfo() << "Got surface: " << m_surface;

    qDebug() << "Requesting adapter...";
    WGPURequestAdapterOptions adapter_opts {};
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;
    adapter_opts.compatibleSurface = m_surface;
    m_adapter = webgpu::requestAdapterSync(m_instance, adapter_opts);
    if (!m_adapter) {
        qFatal("Could not get adapter!");
    }
    qInfo() << "Got adapter: " << m_adapter;

    m_webgpu_window = std::make_unique<webgpu_engine::Window>();

    qDebug() << "Requesting device...";
    WGPULimits required_limits {};
    WGPULimits supported_limits {};
    wgpuAdapterGetLimits(m_adapter, &supported_limits);

    // irrelevant for us, but needs to be set
    required_limits.minStorageBufferOffsetAlignment = supported_limits.minStorageBufferOffsetAlignment;
    required_limits.minUniformBufferOffsetAlignment = supported_limits.minUniformBufferOffsetAlignment;
    required_limits.maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED; // required for current version of  Chrome Canary (2025-04-03)

    // Let the engine change the required limits
    m_webgpu_window->update_required_gpu_limits(required_limits, supported_limits);

    std::vector<WGPUFeatureName> requiredFeatures;
    requiredFeatures.push_back(WGPUFeatureName_TimestampQuery);

    WGPUDeviceDescriptor device_desc {};
    device_desc.label = WGPUStringView { .data = "webigeo device", .length = WGPU_STRLEN };
    device_desc.requiredFeatures = requiredFeatures.data();
    device_desc.requiredFeatureCount = (uint32_t)requiredFeatures.size();
    device_desc.requiredLimits = &required_limits;
    device_desc.defaultQueue.label = WGPUStringView { .data = "webigeo queue", .length = WGPU_STRLEN };
    device_desc.uncapturedErrorCallbackInfo = WGPUUncapturedErrorCallbackInfo {
        .nextInChain = nullptr,
        .callback = webgpu_device_error_callback,
        .userdata1 = nullptr,
        .userdata2 = nullptr,
    };
    device_desc.deviceLostCallbackInfo = WGPUDeviceLostCallbackInfo {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = webgpu_device_lost_callback,
        .userdata1 = nullptr,
        .userdata2 = nullptr,
    };

    m_device = webgpu::requestDeviceSync(m_instance, m_adapter, device_desc);
    if (!m_device) {
        qFatal("Could not get device!");
    }
    qInfo() << "Got device: " << m_device;

    webgpu::checkForTimingSupport(m_adapter, m_device);

    qDebug() << "Requesting queue...";
    m_queue = wgpuDeviceGetQueue(m_device);
    if (!m_queue) {
        qFatal("Could not get queue!");
    }
    qInfo() << "Got queue: " << m_queue;
}

void TerrainRenderer::webgpu_release_context()
{
    qDebug() << "Releasing WebGPU context...";
    wgpuSurfaceUnconfigure(m_surface);
    wgpuQueueRelease(m_queue);
    wgpuSurfaceRelease(m_surface);
    wgpuDeviceRelease(m_device);
    wgpuAdapterRelease(m_adapter);
    wgpuInstanceRelease(m_instance);
}

} // namespace webgpu_app
