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

#pragma once

#include "nucleus/AbstractRenderWindow.h"
#include "nucleus/Controller.h"
#include "nucleus/camera/Controller.h"
#include <GLFW/glfw3.h>
#include <memory>

class TerrainRenderer : public QObject {
    Q_OBJECT

public:
    TerrainRenderer();
    ~TerrainRenderer() = default;

    void init_window();
    void start();
    void render();
    void on_window_resize(int width, int height);
    void on_key_callback(int key, int scancode, int action, int mods);
    void on_cursor_position_callback(double x_pos, double y_pos);
    void on_mouse_button_callback(int button, int action, int mods);
    void on_scroll_callback(double x_offset, double y_offset);

signals:
    void key_pressed(const QKeyCombination&);
    void key_released(const QKeyCombination&);
    void mouse_pressed(const nucleus::event_parameter::Mouse&);
    void mouse_moved(const nucleus::event_parameter::Mouse&);
    void wheel_turned(const nucleus::event_parameter::Wheel&);
    void update_camera_requested();

private slots:
    void set_glfw_window_size(int width, int height);

private:
    GLFWwindow* m_window;
    std::unique_ptr<nucleus::AbstractRenderWindow> m_webgpu_window;
    std::unique_ptr<nucleus::Controller> m_controller;

    uint32_t m_width = 1280;
    uint32_t m_height = 1024;
    bool m_initialized = false;

    nucleus::event_parameter::Mouse m_mouse;
};
