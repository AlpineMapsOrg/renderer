#pragma once

#include "alpine_gl_renderer/GLHelpers.h"

namespace camera {
class Definition;
}
class ShaderProgram;
class Framebuffer;

class Atmosphere
{
public:
    gl_helpers::ScreenQuadGeometry m_screen_quad_geometry;
    std::unique_ptr<Framebuffer> m_framebuffer;

    Atmosphere();
    void resize(const glm::uvec2& new_size);
    void draw(ShaderProgram* atmosphere_program, const camera::Definition& camera, ShaderProgram* copy_program, Framebuffer* out);
};

