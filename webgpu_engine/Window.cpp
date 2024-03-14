#include "Window.h"

#include "webgpu.hpp"
#include <webgpu/webgpu_interface.hpp>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include <imgui.h>
#include "backends/imgui_impl_wgpu.h"
#endif

namespace webgpu_engine {

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
Window::Window(ObtainWebGpuSurfaceFunc obtainWebGpuSurfaceFunc, ImGuiWindowImplInitFunc imguiWindowInitFunc,
    ImGuiWindowImplNewFrameFunc imguiWindowNewFrameFunc, ImGuiWindowImplShutdownFunc imguiWindowShutdownFunc)
    :
    obtainWebGpuSurfaceFunc{obtainWebGpuSurfaceFunc},
    imguiWindowInitFunc{imguiWindowInitFunc},
    imguiWindowNewFrameFunc{imguiWindowNewFrameFunc},
    imguiWindowShutdownFunc{imguiWindowShutdownFunc}
{
    // Constructor initialization logic here
}
#else
Window::Window(ObtainWebGpuSurfaceFunc obtainWebGpuSurfaceFunc):
    obtainWebGpuSurfaceFunc{obtainWebGpuSurfaceFunc}
{}
#endif

Window::~Window() {
    // Destructor cleanup logic here
}

void Window::initialise_gpu() {
    // GPU initialization logic here
    webgpuPlatformInit(); // platform dependent initialization code
    createInstance();
    initSurface();
    requestAdapter();
    requestDevice();
    initQueue();

    create_buffers();
    create_bind_group_info();

    m_shader_manager = std::make_unique<ShaderModuleManager>(device);
    m_shader_manager->create_shader_modules();
    m_pipeline_manager = std::make_unique<PipelineManager>(device, *m_shader_manager);

    m_stopwatch.restart();
}

void Window::resize_framebuffer(int w, int h) {
    //TODO check we can do it without completely recreating swapchain and pipeline

    if (m_pipeline_manager->pipelines_created()) {
        m_pipeline_manager->release_pipelines();
    }

    if (swapchain != nullptr) {
        swapchain.release();
    }

    createSwapchain(w, h);
    m_pipeline_manager->create_pipelines(swapchainFormat, *m_bind_group_info);

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    if (ImGui::GetCurrentContext() != nullptr) {
        // already initialized
        return;
    }

    if (!initGui()) {
        std::cerr << "Could not initialize GUI!" << std::endl;
        throw std::runtime_error("could not initialize GUI");
    }
#endif
}

void Window::paint([[maybe_unused]] QOpenGLFramebufferObject* framebuffer) {
    // Painting logic here, using the optional framebuffer parameter which is currently unused

    wgpu::TextureView nextTexture = swapchain.getCurrentTextureView();
    if (!nextTexture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        throw std::runtime_error("Cannot acquire next swap chain texture");
    }

    emit update_camera_requested();

    //TODO remove, debugging
    const auto elapsed = static_cast<double>(m_stopwatch.total().count() / 1000.0f);
    uboSharedConfig* sc = &m_shared_config_ubo->data;
    sc->m_sun_light = QVector4D(0.0f, 1.0f, 1.0f, 1.0f);
    sc->m_sun_light_dir = QVector4D(elapsed, 1.0f, 1.0f, 1.0f);
    m_shared_config_ubo->update_gpu_data(queue);

    wgpu::CommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.label = "Command Encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(commandEncoderDesc);

    wgpu::RenderPassDescriptor renderPassDesc{};

    wgpu::RenderPassColorAttachment renderPassColorAttachment{};
    renderPassColorAttachment.view = nextTexture;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = wgpu::Color{ 0.9, 0.1, 0.2, 1.0 };

    // depthSlice field for RenderPassColorAttachment (https://github.com/gpuweb/gpuweb/issues/4251)
    // this field specifies the slice to render to when rendering to a 3d texture (view)
    // passing a valid index but referencing a non-3d texture leads to an error
    // TODO use some constant that represents "undefined" for this value (I couldn't find a constant for this?)
    //     (I just guessed -1 (max unsigned int value) and it worked)
    renderPassColorAttachment.depthSlice = -1;

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;
    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

    m_bind_group_info->bind(renderPass, 0);
    renderPass.setPipeline(m_pipeline_manager->debug_config_and_camera_pipeline());
    renderPass.draw(36, 1, 0, 0);

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    // We add the GUI drawing commands to the render pass
    updateGui(renderPass);
#endif

    renderPass.end();
    renderPass.release();

    nextTexture.release();

    wgpu::CommandBufferDescriptor cmdBufferDescriptor{};
    cmdBufferDescriptor.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    encoder.release();
    queue.submit(command);
    command.release();

#ifndef __EMSCRIPTEN__
    // Swapchain in the WEB is handled by the browser!
    swapchain.present();
    instance.processEvents();
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
    terminateGui();
#endif

    m_pipeline_manager->release_pipelines();
    m_shader_manager->release_shader_modules();
    swapchain.release();
    queue.release();
    surface.release();
    //TODO triggers warning No Dawn device lost callback was set, but this is actually expected behavior
    device.release();
    adapter.release();
    instance.release();
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
    m_camera_config_ubo->update_gpu_data(queue);
}

void Window::update_debug_scheduler_stats([[maybe_unused]] const QString& stats) {
    // Logic for updating debug scheduler stats, parameter currently unused
}

void Window::update_gpu_quads([[maybe_unused]] const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, [[maybe_unused]] const std::vector<tile::Id>& deleted_quads) {
    // Logic for updating GPU quads, parameters currently unused
}



#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
bool Window::initGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    // Setup Platform/Renderer backends
    imguiWindowInitFunc();
    ImGui_ImplWGPU_InitInfo init_info = {};
    init_info.Device = device;
    init_info.RenderTargetFormat = (WGPUTextureFormat)swapchainFormat;
    init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    init_info.NumFramesInFlight = 3;
    ImGui_ImplWGPU_Init(&init_info);
    return true;
}

void Window::terminateGui() {
    imguiWindowShutdownFunc();
    ImGui_ImplWGPU_Shutdown();
}

void Window::updateGui(wgpu::RenderPassEncoder renderPass) {
    ImGui_ImplWGPU_NewFrame();
    imguiWindowNewFrameFunc();
    ImGui::NewFrame();

           // Build our UI
    static float f = 0.0f;
    static int counter = 0;
    static bool show_demo_window = true;
    static bool show_another_window = false;
    static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ImGui::Begin("Hello, world!");                                // Create a window called "Hello, world!" and append into it.

    ImGui::Text("This is some useful text.");                     // Display some text (you can use a format strings too)
    ImGui::Checkbox("Demo Window", &show_demo_window);            // Edit bools storing our window open/close state
    ImGui::Checkbox("Another Window", &show_another_window);

    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);                  // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::ColorEdit3("clear color", (float*)&clear_color);       // Edit 3 floats representing a color

    if (ImGui::Button("Button"))                                  // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}
#endif


void Window::createInstance() {
    std::cout << "Creating WebGPU instance..." << std::endl;

#ifdef __EMSCRIPTEN__
    // instance = wgpu::createInstance(wgpu::InstanceDescriptor{}); would throw nullptr exception
    // but using vanilla webgpu with nullptr as descriptor seems to work.
    instance = wgpuCreateInstance(nullptr);
#else
    instance = wgpu::createInstance(wgpu::InstanceDescriptor{});
#endif
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        throw std::runtime_error("Could not initialize WebGPU");
    }
    std::cout << "Got instance: " << instance << std::endl;
}

void Window::initSurface() {
    surface = obtainWebGpuSurfaceFunc(instance);
}

void Window::requestAdapter() {
    std::cout << "Requesting adapter..." << std::endl;
    wgpu::RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = surface;
    adapter = instance.requestAdapter(adapterOpts);
    std::cout << "Got adapter: " << adapter << std::endl;
}

void Window::requestDevice() {
    std::cout << "Requesting device..." << std::endl;
    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    device = adapter.requestDevice(deviceDesc);
    std::cout << "Got device: " << device << std::endl;

    auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
        std::cout << "Uncaptured device error: type " << type;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr /* pUserData */);
}

void Window::initQueue() {
    queue = device.getQueue();
}

void Window::create_buffers()
{
    m_shared_config_ubo = std::make_unique<UniformBuffer<uboSharedConfig>>();
    m_shared_config_ubo->init(device);

    m_camera_config_ubo = std::make_unique<UniformBuffer<uboCameraConfig>>();
    m_camera_config_ubo->init(device);
}

void Window::createSwapchain(uint32_t width, uint32_t height) {
    std::cout << "Creating swapchain device..." << std::endl;
    // from Learn WebGPU C++ tutorial
#ifdef WEBGPU_BACKEND_WGPU
    swapchainFormat = surface.getPreferredFormat(adapter);
#else
    swapchainFormat = wgpu::TextureFormat::BGRA8Unorm;
#endif
    wgpu::SwapChainDescriptor swapChainDesc = {};
    swapChainDesc.width = width;
    swapChainDesc.height = height;
    swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapChainDesc.format = swapchainFormat;
    swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
    swapchain = device.createSwapChain(surface, swapChainDesc);
    std::cout << "Swapchain: " << swapchain << std::endl;
}

void Window::create_bind_group_info()
{
    m_bind_group_info = std::make_unique<BindGroupInfo>();
    m_bind_group_info->add_entry(0, *m_shared_config_ubo, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
    m_bind_group_info->add_entry(1, *m_camera_config_ubo, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
    m_bind_group_info->init(device);
}


}
