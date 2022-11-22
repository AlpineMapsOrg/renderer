#pragma once

#include "alpine_gl_renderer/GLHelpers.h"

namespace camera {
class Definition;
}
class ShaderProgram;

class Atmosphere
{
    gl_helpers::ScreenQuadGeometry m_screen_quad_geometry;
public:
    Atmosphere();
    void draw(ShaderProgram* shader_program, const camera::Definition& camera);
};

