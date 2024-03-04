//#include <webgpu/webgpu_cpp.h>
#include "dawn_setup.h"
#include "webgpu.hpp"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#endif

#include <iostream>
#include "webgpu_engine/Test.h"

#include <GLFW/glfw3.h>

#include <QCoreApplication>
#include <QImage>

GLFWwindow* window;

const uint32_t kWidth = 1024;
const uint32_t kHeight = 800;

wgpu::Instance instance = nullptr;
wgpu::Device device = nullptr;
wgpu::Adapter adapter = nullptr;
wgpu::Surface surface = nullptr;
wgpu::SwapChain swapchain = nullptr;
wgpu::TextureFormat swapchainFormat = wgpu::TextureFormat::Undefined;
wgpu::RenderPipeline pipeline = nullptr;
wgpu::Queue queue = nullptr;


#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
bool initGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_InitInfo init_info = {};
    init_info.Device = device;
    init_info.RenderTargetFormat = (WGPUTextureFormat)swapchainFormat;
    init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    init_info.NumFramesInFlight = 3;
    ImGui_ImplWGPU_Init(&init_info);
    return true;
}

void terminateGui() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}

void updateGui(wgpu::RenderPassEncoder renderPass) {
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
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

void createWindow() {
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        throw std::runtime_error("Could not initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(kWidth, kHeight, "Learn WebGPU", NULL, NULL);
    if (!window) {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        throw std::runtime_error("Could not open window");
    }

#ifndef __EMSCRIPTEN__
    // Load Icon for Window
    QImage icon(":/icons/icon_32.png"); // Note: When we remove QGUI as dependency this has to go
    if (!icon.isNull()) {
        QImage formattedIcon = icon.convertToFormat(QImage::Format_RGBA8888);

        GLFWimage images[1];
        images[0].width = formattedIcon.width();
        images[0].height = formattedIcon.height();
        images[0].pixels = formattedIcon.bits();
        glfwSetWindowIcon(window, 1, images);
    } else {
        std::cerr << "Could not load icon image!" << std::endl;
    }
#endif
}

void createInstance() {
    instance = createInstance(wgpu::InstanceDescriptor{});
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        throw std::runtime_error("Could not initialize WebGPU");
    }
}

void initSurface() {
    surface = glfwGetWGPUSurface(instance, window);
}

void requestAdapter() {
    std::cout << "Requesting adapter..." << std::endl;
    wgpu::RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = surface;
    adapter = instance.requestAdapter(adapterOpts);
    std::cout << "Got adapter: " << adapter << std::endl;
}

void requestDevice() {
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

void initQueue() {
    queue = device.getQueue();
}

void createSwapchain() {
    std::cout << "Creating swapchain device..." << std::endl;
#ifdef WEBGPU_BACKEND_WGPU
    swapchainFormat = surface.getPreferredFormat(adapter);
#else
    swapchainFormat = wgpu::TextureFormat::BGRA8Unorm;
#endif
    wgpu::SwapChainDescriptor swapChainDesc = {};
    swapChainDesc.width = kWidth;
    swapChainDesc.height = kHeight;
    swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapChainDesc.format = swapchainFormat;
    swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
    swapchain = device.createSwapChain(surface, swapChainDesc);
    std::cout << "Swapchain: " << swapchain << std::endl;
}

void createPipeline() {
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

void initGraphics() {
    createInstance();
    initSurface();
    requestAdapter();
    requestDevice();
    initQueue();
    createSwapchain();
    createPipeline();
}

void render() {
    wgpu::TextureView nextTexture = swapchain.getCurrentTextureView();
    if (!nextTexture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        throw std::runtime_error("Cannot acquire next swap chain texture");
    }

    wgpu::CommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.label = "Command Encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(commandEncoderDesc);

    wgpu::RenderPassDescriptor renderPassDesc{};

    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
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
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    using namespace wgpu;

    // Init QCoreApplication seems to be necessary as it declares the current thread
    // as a Qt-Thread. Otherwise functionalities like QTimers wouldnt work.
    // It basically initializes the Qt environment
    QCoreApplication app(argc, argv);

    setup_dawn_proctable();

    createWindow();
    initGraphics();

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    if (!initGui()) {
        std::cerr << "Could not initialize GUI!" << std::endl;
        return 1;
    }
#endif

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(Render, 0, false);
#else
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        render();
        swapchain.present();
        instance.processEvents();
    }
#endif

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    terminateGui();
#endif

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
