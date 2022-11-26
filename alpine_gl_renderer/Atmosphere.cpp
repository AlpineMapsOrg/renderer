#include "Atmosphere.h"

#include <glm/gtx/transform.hpp>
#include <QOpenGLTexture>

#include "Framebuffer.h"
#include "GLHelpers.h"
#include "ShaderProgram.h"
#include "nucleus/camera/Definition.h"

Atmosphere::Atmosphere()
{
    m_screen_quad_geometry = gl_helpers::create_screen_quad_geometry();
}

void Atmosphere::draw(ShaderProgram* shader_program, const camera::Definition& camera)
{
    shader_program->set_uniform("inversed_projection_matrix", glm::inverse(camera.projectionMatrix()));
    shader_program->set_uniform("inversed_view_matrix", glm::translate(-camera.position()) * camera.camera_space_to_world_matrix());
    shader_program->set_uniform("camera_position", glm::vec3(camera.position()));

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glDepthFunc(GL_EQUAL);
    m_screen_quad_geometry.draw_with_depth_test();
}
