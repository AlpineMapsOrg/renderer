#pragma once

#include "alpine_gl_renderer/GLHelpers.h"

namespace camera {
class Definition;
}
class ShaderProgram;
class QOpenGLTexture;

class Atmosphere
{
    gl_helpers::ScreenQuadGeometry m_screen_quad_geometry;
    std::unique_ptr<QOpenGLTexture> m_lookup_table;
public:
    Atmosphere();
    void draw(ShaderProgram* shader_program, const camera::Definition& camera);
    void bind_lookup_table(int texture_unit);
};

