#include "TerrainRenderer.h"

#include <QImage>
#include <map>
#include "glfw3webgpu.h"
#include <iostream>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#endif

#include "webgpu_engine/Window.h"

static void windowResizeCallback(GLFWwindow* window, int width, int height) {
    auto terrainRenderer = static_cast<TerrainRenderer*>(glfwGetWindowUserPointer(window));
    terrainRenderer->on_window_resize(width, height);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto renderer = static_cast<TerrainRenderer*>(glfwGetWindowUserPointer(window));
    renderer->on_key_callback(key, scancode, action, mods);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto renderer = static_cast<TerrainRenderer*>(glfwGetWindowUserPointer(window));
    renderer->on_cursor_position_callback(xpos, ypos);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    auto renderer = static_cast<TerrainRenderer*>(glfwGetWindowUserPointer(window));
    renderer->on_mouse_button_callback(button, action, mods);
}

TerrainRenderer::TerrainRenderer() {}

void TerrainRenderer::init_window() {
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
    glfwSetKeyCallback(m_window, key_callback);
    glfwSetCursorPosCallback(m_window, cursor_position_callback);
    glfwSetMouseButtonCallback(m_window, mouse_button_callback);

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
    init_window();

    auto glfw_GetWGPUSurface_func = [this] (wgpu::Instance instance) {
        return glfwGetWGPUSurface(instance, m_window);
    };

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    m_webgpu_window = std::make_unique<webgpu_engine::Window>(glfw_GetWGPUSurface_func,
        [this] () { ImGui_ImplGlfw_InitForOther(m_window, true); },
        ImGui_ImplGlfw_NewFrame,
        ImGui_ImplGlfw_Shutdown
    );
#else
    m_webgpu_window = std::make_unique<webgpu_engine::Window>(glfwGetWGPUSurfaceFunctor);
#endif

    nucleus::camera::Definition camera;
    camera.look_at(glm::dvec3{0.0f, 0.0f, 0.0f}, glm::dvec3{0.0, -1.0, 0.0});
    m_camera_controller = std::make_unique<nucleus::camera::Controller>(camera, dynamic_cast<nucleus::camera::AbstractDepthTester*>(m_webgpu_window.get()), nullptr);
    m_camera_controller->set_viewport({m_width, m_height}); //TODO also when resizing window

    connect(this, &TerrainRenderer::key_pressed, m_camera_controller.get(), &nucleus::camera::Controller::key_press);
    connect(this, &TerrainRenderer::key_released, m_camera_controller.get(), &nucleus::camera::Controller::key_release);
    connect(this, &TerrainRenderer::mouse_moved, m_camera_controller.get(), &nucleus::camera::Controller::mouse_move);
    connect(this, &TerrainRenderer::mouse_pressed, m_camera_controller.get(), &nucleus::camera::Controller::mouse_press);

    connect(m_webgpu_window.get(), &nucleus::AbstractRenderWindow::update_camera_requested, m_camera_controller.get(), &nucleus::camera::Controller::update_camera_request);
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_webgpu_window.get(), &nucleus::AbstractRenderWindow::update_camera);

    m_webgpu_window->initialise_gpu();
    m_webgpu_window->resize_framebuffer(m_width, m_height);
    m_camera_controller->update();

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

void TerrainRenderer::on_window_resize(int width, int height) {
    std::cout << "window resized to " << width << "x" << height << std::endl;
    m_webgpu_window->resize_framebuffer(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    m_camera_controller->set_viewport({width, height});
    m_camera_controller->update();
}

void TerrainRenderer::on_key_callback(int key, int scancode, int action, int mods)
{
    //TODO modifiers; more keys if needed
    const std::map<int, Qt::Key> key_map = {
        {GLFW_KEY_W, Qt::Key_W},
        {GLFW_KEY_S, Qt::Key_S},
        {GLFW_KEY_A, Qt::Key_A},
        {GLFW_KEY_D, Qt::Key_D},
        {GLFW_KEY_Q, Qt::Key_Q},
        {GLFW_KEY_E, Qt::Key_E},
        {GLFW_KEY_1, Qt::Key_1},
        {GLFW_KEY_2, Qt::Key_2},
        {GLFW_KEY_3, Qt::Key_3}

    };

    const auto found_it = key_map.find(key);
    if (found_it == key_map.end()) {
        std::cout << "key not mapped" << std::endl;
        return;
    }

    QKeyCombination combination(found_it->second);
    if (action == GLFW_PRESS) {
        std::cout << "pressed " << found_it->second << std::endl;
        emit key_pressed(combination);
    } else if (action == GLFW_RELEASE) {
        std::cout << "released " << found_it->second << std::endl;
        emit key_released(combination);
    }
}

void TerrainRenderer::on_cursor_position_callback(double x_pos, double y_pos)
{
    m_mouse.last_position = m_mouse.position;
    m_mouse.position.x = x_pos;
    m_mouse.position.y = y_pos;

    std::cout << "mouse moved, x=" << x_pos << ", y=" << y_pos << std::endl;

    emit mouse_moved(m_mouse);
}

void TerrainRenderer::on_mouse_button_callback(int button, int action, int mods)
{
    //TODO modifiers if needed
    const std::map<int, Qt::MouseButton> button_map = {
        {GLFW_MOUSE_BUTTON_LEFT, Qt::LeftButton},
        {GLFW_MOUSE_BUTTON_RIGHT, Qt::RightButton},
        {GLFW_MOUSE_BUTTON_MIDDLE, Qt::MiddleButton}
    };

    const auto found_it = button_map.find(button);
    if (found_it == button_map.end()) {
        std::cout << "mouse button not mapped" << std::endl;
        return;
    }

    if (action == GLFW_RELEASE) {
        m_mouse.buttons &= ~found_it->second;
        std::cout << "mouse button released" << std::endl;
    } else if (action == GLFW_PRESS) {
        m_mouse.buttons |= found_it->second;
        std::cout << "mouse button pressed " << found_it->second << std::endl;
    }

    emit mouse_pressed(m_mouse);
}

