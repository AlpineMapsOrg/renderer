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

#include <QImage>
#include <map>
#include <webgpu/webgpu_interface.hpp>
#include <iostream>

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#endif

#include "nucleus/tile_scheduler/Scheduler.h"
#include "webgpu_engine/Window.h"

#ifdef __EMSCRIPTEN__
#include "WebInterop.h"
#include <emscripten/emscripten.h>
#endif

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

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto renderer = static_cast<TerrainRenderer*>(glfwGetWindowUserPointer(window));
    renderer->on_scroll_callback(xoffset, yoffset);
}

TerrainRenderer::TerrainRenderer() {
#ifdef __EMSCRIPTEN__
    // execute on window resize when canvas size changes
    QObject::connect(&WebInterop::instance(), &WebInterop::canvas_size_changed, this, &TerrainRenderer::set_glfw_window_size);
#endif
}

void TerrainRenderer::init_window() {

    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        //throw std::runtime_error("Could not initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(m_width, m_height, "weBIGeo - Geospatial Visualization Tool", NULL, NULL);
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
    glfwSetScrollCallback(m_window, scroll_callback);

#ifndef __EMSCRIPTEN__
    // Load Icon for Window
    QImage icon(":/icons/logo32.png"); // Note: When we remove QGUI as dependency this has to go
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

void TerrainRenderer::render() {
    glfwPollEvents();
    m_webgpu_window->paint(nullptr);
}

void TerrainRenderer::start() {
    std::cout << "before initWindow()" << std::endl;
    init_window();

    std::cout << "before initWebGPU()" << std::endl;
    auto glfwGetWGPUSurfaceFunctor = [this](WGPUInstance instance) { return glfwGetWGPUSurface(instance, m_window); };

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    m_webgpu_window = std::make_unique<webgpu_engine::Window>(glfwGetWGPUSurfaceFunctor,
        [this] () { ImGui_ImplGlfw_InitForOther(m_window, true); },
        ImGui_ImplGlfw_NewFrame,
        ImGui_ImplGlfw_Shutdown
    );
#else
    m_webgpu_window = std::make_unique<webgpu_engine::Window>(glfwGetWGPUSurfaceFunctor);
#endif

    m_controller = std::make_unique<nucleus::Controller>(m_webgpu_window.get());

    nucleus::camera::Controller* camera_controller = m_controller->camera_controller();
    connect(this, &TerrainRenderer::key_pressed, camera_controller, &nucleus::camera::Controller::key_press);
    connect(this, &TerrainRenderer::key_released, camera_controller, &nucleus::camera::Controller::key_release);
    connect(this, &TerrainRenderer::mouse_moved, camera_controller, &nucleus::camera::Controller::mouse_move);
    connect(this, &TerrainRenderer::mouse_pressed, camera_controller, &nucleus::camera::Controller::mouse_press);
    connect(this, &TerrainRenderer::wheel_turned, camera_controller, &nucleus::camera::Controller::wheel_turn);
    connect(this, &TerrainRenderer::update_camera_requested, camera_controller, &nucleus::camera::Controller::update_camera_request);

    m_webgpu_window->initialise_gpu();
    m_webgpu_window->resize_framebuffer(m_width, m_height);

    camera_controller->set_viewport({ m_width, m_height });
    camera_controller->update();

    glfwSetWindowSize(m_window, m_width, m_height);
    m_initialized = true;


#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(
        [](void *userData) {
            TerrainRenderer& renderer = *reinterpret_cast<TerrainRenderer*>(userData);
            renderer.render();
        },
        (void*)this,
        0, true
    );
#else
    while (!glfwWindowShouldClose(m_window)) {
        // Do nothing, this checks for ongoing asynchronous operations and call their callbacks

        glfwPollEvents();
        m_webgpu_window->paint(nullptr);
    }
#endif

    // NOTE: Ressources are freed by the browser when the page is closed. Also keep in mind
    // that this part of code will be executed immediately since the main loop is not blocking.
#ifndef __EMSCRIPTEN__
    m_webgpu_window->deinit_gpu();

    glfwDestroyWindow(m_window);
    glfwTerminate();
#endif
}

void TerrainRenderer::on_window_resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_webgpu_window->resize_framebuffer(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    m_controller->camera_controller()->set_viewport({ width, height });
    m_controller->camera_controller()->update();
}

void TerrainRenderer::on_key_callback(int key, [[maybe_unused]]int scancode, int action, [[maybe_unused]]int mods)
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
        {GLFW_KEY_3, Qt::Key_3},
        {GLFW_KEY_LEFT_CONTROL, Qt::Key_Control},
        {GLFW_KEY_LEFT_SHIFT, Qt::Key_Shift},
        {GLFW_KEY_LEFT_ALT, Qt::Key_Alt},

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
    m_mouse.position = { x_pos,  y_pos };
    //std::cout << "mouse moved, x=" << x_pos << ", y=" << y_pos << std::endl;
    emit mouse_moved(m_mouse);
}

void TerrainRenderer::set_glfw_window_size(int width, int height) {
    m_width = width;
    m_height = height;
    if (m_initialized) {
        glfwSetWindowSize(m_window, width, height);
    }
}

void TerrainRenderer::on_mouse_button_callback(int button, int action, [[maybe_unused]]int mods)
{
#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }
#endif

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
        //std::cout << "mouse button released" << std::endl;
    } else if (action == GLFW_PRESS) {
        m_mouse.buttons |= found_it->second;
        //std::cout << "mouse button pressed " << found_it->second << std::endl;
    }

    emit mouse_pressed(m_mouse);
}

void TerrainRenderer::on_scroll_callback(double x_offset, double y_offset)
{
    nucleus::event_parameter::Wheel wheel {};
    wheel.angle_delta = QPoint(static_cast<int>(x_offset), static_cast<int>(y_offset) * 50.0f);
    wheel.position = m_mouse.position;
    //std::cout << "wheel  turned, delta x=" << x_offset << ", y=" << y_offset << std::endl;
    emit wheel_turned(wheel);
}
