#pragma once

#include <memory>
#include <GLFW/glfw3.h>
#include "nucleus/camera/Controller.h"
#include "nucleus/AbstractRenderWindow.h"

class TerrainRenderer : public QObject {
    Q_OBJECT

public:
    TerrainRenderer();

    void init_window();
    void start();
    void on_window_resize(int width, int height);
    void on_key_callback(int key, int scancode, int action, int mods);
    void on_cursor_position_callback(double x_pos, double y_pos);
    void on_mouse_button_callback(int button, int action, int mods);

signals:
    void key_pressed(const QKeyCombination&);
    void key_released(const QKeyCombination&);
    void mouse_pressed(const nucleus::event_parameter::Mouse&);
    void mouse_moved(const nucleus::event_parameter::Mouse&);
    void update_camera_requested();

private:
    GLFWwindow* m_window;
    std::unique_ptr<nucleus::AbstractRenderWindow> m_webgpu_window;
    std::unique_ptr<nucleus::camera::Controller> m_camera_controller;

    uint32_t m_width = 1024;
    uint32_t m_height = 800;

    nucleus::event_parameter::Mouse m_mouse;
};
