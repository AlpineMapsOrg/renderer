#pragma once

#include "helpers.h"

namespace nucleus::camera {
class Definition;
}

namespace gl_engine {
class ShaderProgram;
class Framebuffer;

class Atmosphere {
public:
    gl_engine::helpers::ScreenQuadGeometry m_screen_quad_geometry;
    std::unique_ptr<Framebuffer> m_framebuffer;

    Atmosphere();
    void resize(const glm::uvec2& new_size);
    void draw(ShaderProgram* atmosphere_program, const nucleus::camera::Definition& camera, ShaderProgram* copy_program, Framebuffer* out);
};
}
