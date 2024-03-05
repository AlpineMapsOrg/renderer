#pragma once

#include <memory>
#include <GLFW/glfw3.h>
#include "nucleus/AbstractRenderWindow.h"

class TerrainRenderer {

public:
    TerrainRenderer();

    void initWindow();
    void start();
    void onWindowResize(int width, int height);

private:
    GLFWwindow* m_window;
    std::unique_ptr<nucleus::AbstractRenderWindow> m_webgpu_window;

    uint32_t m_width = 1024;
    uint32_t m_height = 800;
};
