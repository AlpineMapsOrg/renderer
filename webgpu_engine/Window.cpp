#include "Window.h"

#include "webgpu.hpp"
#include "dawn_setup.h"

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
    setup_dawn_proctable();
    createInstance();
    initSurface();
    requestAdapter();
    requestDevice();
    initQueue();
}

void Window::resize_framebuffer(int w, int h) {
    //TODO check we can do it without completely recreating swapchain and pipeline

    if (pipeline != nullptr) {
        pipeline.release();
    }

    if (swapchain != nullptr) {
        swapchain.release();
    }

    createSwapchain(w, h);
    createPipeline();

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
   //TODO use some constant that represents "undefined" for this value (I couldn't find a constant for this?)
   //     (I just guessed -1 (max unsigned int value) and it worked)
    renderPassColorAttachment.depthSlice = -1;

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;
    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.setPipeline(pipeline);
    renderPass.draw(3, 1, 0, 0);

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    // We add the GUI drawing commands to the render pass
    updateGui(renderPass);
#endif

    renderPass.end();

    nextTexture.release();

    wgpu::CommandBufferDescriptor cmdBufferDescriptor{};
    cmdBufferDescriptor.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    queue.submit(command);

    swapchain.present();
    instance.processEvents();
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

    pipeline.release();
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


const char shaderCode[] = R"(
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
      @builtin(position) vec4f {
        const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
        return vec4f(pos[i], 0, 1);
    }
    @fragment fn fragmentMain() -> @location(0) vec4f {
        return vec4f(0.0, 0.4, 1.0, 1.0);
    }
)";

void Window::createInstance() {
    instance = wgpu::createInstance(wgpu::InstanceDescriptor{});
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        throw std::runtime_error("Could not initialize WebGPU");
    }
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

void Window::createPipeline() {
    wgpu::ShaderModuleDescriptor shaderModuleDesc{};
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgslDesc.code = shaderCode;
    shaderModuleDesc.nextInChain = &wgslDesc.chain;
    wgpu::ShaderModule shaderModule = device.createShaderModule(shaderModuleDesc);

    wgpu::RenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vertexMain";
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

    wgpu::FragmentState fragmentState{};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fragmentMain";
    fragmentState.targetCount = 1;

    wgpu::ColorTargetState colorTargetState{};
    colorTargetState.format = swapchainFormat;
    fragmentState.targets = &colorTargetState;

    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colorTarget;
    colorTarget.format = swapchainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    pipeline = device.createRenderPipeline(pipelineDesc);
}


}
