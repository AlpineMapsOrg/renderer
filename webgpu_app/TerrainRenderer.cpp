#include "TerrainRenderer.h"

#include <QImage>
#include "glfw3webgpu.h"
#include <iostream>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#endif

#include "webgpu_engine/Window.h"

void windowResizeCallback(GLFWwindow* window, int width, int height) {
    auto terrainRenderer = static_cast<TerrainRenderer*>(glfwGetWindowUserPointer(window));
    terrainRenderer->onWindowResize(width, height);
}

TerrainRenderer::TerrainRenderer() { }

void TerrainRenderer::initWindow() {
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        throw std::runtime_error("Could not initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(m_width, m_height, "Learn WebGPU", NULL, NULL);
    if (!m_window) {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        throw std::runtime_error("Could not open window");
    }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetWindowSizeCallback(m_window, windowResizeCallback);

#ifndef __EMSCRIPTEN__
    // Load Icon for Window
    QImage icon(":/icons/icon_32.png"); // Note: When we remove QGUI as dependency this has to go
    if (!icon.isNull()) {
        QImage formattedIcon = icon.convertToFormat(QImage::Format_RGBA8888);

        GLFWimage images[1];
        images[0].width = formattedIcon.width();
        images[0].height = formattedIcon.height();
        images[0].pixels = formattedIcon.bits();
        glfwSetWindowIcon(m_window, 1, images);
    } else {
        std::cerr << "Could not load icon image!" << std::endl;
    }
#endif
}

void TerrainRenderer::start() {
    initWindow();

    auto glfwGetWGPUSurfaceFunctor = [this] (wgpu::Instance instance) {
        return glfwGetWGPUSurface(instance, m_window);
    };

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    m_webgpu_window = std::make_unique<webgpu_engine::Window>(glfwGetWGPUSurfaceFunctor,
        [this] () { ImGui_ImplGlfw_InitForOther(m_window, true); },
        ImGui_ImplGlfw_NewFrame,
        ImGui_ImplGlfw_Shutdown
    );
#else
    m_webgpu_window = std::make_unique<webgpu_engine::Window>(glfwGetWGPUSurfaceFunctor);
#endif

    m_webgpu_window->initialise_gpu();
    m_webgpu_window->resize_framebuffer(m_width, m_height);

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(Render, 0, false);
#else
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        m_webgpu_window->paint(nullptr);
    }
#endif

    m_webgpu_window->deinit_gpu();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void TerrainRenderer::onWindowResize(int width, int height) {
    std::cout << "window resized to " << width << "x" << height << std::endl;
    m_webgpu_window->resize_framebuffer(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

