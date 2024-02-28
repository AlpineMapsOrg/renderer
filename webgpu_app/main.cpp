
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#endif

#include <iostream>
#include "webgpu_engine/Test.h"

#include <GLFW/glfw3.h>

#include <QCoreApplication>
#include <QImage>

wgpu::Instance instance;
wgpu::Device device;
wgpu::RenderPipeline pipeline;

wgpu::SwapChain swapChain;
const uint32_t kWidth = 1024;
const uint32_t kHeight = 800;

void SetupSwapChain(wgpu::Surface surface) {
    wgpu::SwapChainDescriptor scDesc{
        .usage = wgpu::TextureUsage::RenderAttachment,
        .format = wgpu::TextureFormat::BGRA8Unorm,
        .width = kWidth,
        .height = kHeight,
        .presentMode = wgpu::PresentMode::Fifo};
    swapChain = device.CreateSwapChain(surface, &scDesc);
}

void GetDevice(void (*callback)(wgpu::Device)) {
    instance.RequestAdapter(
        nullptr,
        // TODO(https://bugs.chromium.org/p/dawn/issues/detail?id=1892): Use
        // wgpu::RequestAdapterStatus, wgpu::Adapter, and wgpu::Device.
        [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter,
            [[maybe_unused]]const char* message, void* userdata) {
            if (status != WGPURequestAdapterStatus_Success) {
                exit(0);
            }
            wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
            adapter.RequestDevice(
                nullptr,
                []([[maybe_unused]]WGPURequestDeviceStatus status, WGPUDevice cDevice,
                    [[maybe_unused]]const char* message, void* userdata) {
                    wgpu::Device device = wgpu::Device::Acquire(cDevice);
                    device.SetUncapturedErrorCallback(
                        [](WGPUErrorType type, const char* message, [[maybe_unused]]void* userdata) {
                            std::cout << "Error: " << type << " - message: " << message;
                        },
                        nullptr);
                    reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
                },
                userdata);
        },
        reinterpret_cast<void*>(callback));
}

const char shaderCode[] = R"(
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
      @builtin(position) vec4f {
        const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
        return vec4f(pos[i], 0, 1);
    }
    @fragment fn fragmentMain() -> @location(0) vec4f {
        return vec4f(1, 0, 0, 1);
    }
)";

void CreateRenderPipeline() {
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = shaderCode;

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &wgslDesc};
    wgpu::ShaderModule shaderModule =
        device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{
        .format = wgpu::TextureFormat::BGRA8Unorm};

    wgpu::FragmentState fragmentState{.module = shaderModule,
        .targetCount = 1,
        .targets = &colorTargetState};

    wgpu::RenderPipelineDescriptor descriptor{
        .vertex = {.module = shaderModule},
        .fragment = &fragmentState};
    pipeline = device.CreateRenderPipeline(&descriptor);
}

void Render() {
    wgpu::RenderPassColorAttachment attachment{
        .view = swapChain.GetCurrentTextureView(),
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store};

    wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = 1,
        .colorAttachments = &attachment};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    pass.SetPipeline(pipeline);
    pass.Draw(3);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}

void InitGraphics(wgpu::Surface surface) {
    SetupSwapChain(surface);
    CreateRenderPipeline();
}

void Start() {
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
        glfwCreateWindow(kWidth, kHeight, "weBIGeo", nullptr, nullptr);

    if (!window) {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        return;
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

#if defined(__EMSCRIPTEN__)
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.selector = "#canvas";

    wgpu::SurfaceDescriptor surfaceDesc{.nextInChain = &canvasDesc};
    wgpu::Surface surface = instance.CreateSurface(&surfaceDesc);
#else
    wgpu::Surface surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
#endif

    InitGraphics(surface);

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(Render, 0, false);
#else
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        Render();
        swapChain.Present();
        instance.ProcessEvents();
    }
#endif

    // At the end of the program, destroy the window
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    // Init QCoreApplication seems to be necessary as it declares the current thread
    // as a Qt-Thread. Otherwise functionalities like QTimers wouldnt work.
    // It basically initializes the Qt environment
    QCoreApplication app(argc, argv);

    instance = wgpu::CreateInstance();
    GetDevice([](wgpu::Device dev) {
        device = dev;
        Start();
    });

    return 0;
}
