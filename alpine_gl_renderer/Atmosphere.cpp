#include "Atmosphere.h"

#include <glm/gtx/transform.hpp>

#include "Framebuffer.h"
#include "GLHelpers.h"
#include "ShaderProgram.h"
#include "nucleus/camera/Definition.h"

Atmosphere::Atmosphere()
{
    m_screen_quad_geometry = gl_helpers::create_screen_quad_geometry();
    Framebuffer compute_buffer(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::Float32 });
    compute_buffer.resize({ 512, 512 });
    compute_buffer.bind();

    ShaderProgram compute_shader(ShaderProgram::Files({ "gl_shaders/screen_pass.vert" }), ShaderProgram::Files({ "gl_shaders/atmosphere_compute.frag" }));
    compute_shader.bind();

    m_screen_quad_geometry.draw();

    compute_buffer.read_colour_attachment(0).save("/home/madam/Documents/work/tuw/alpinemaps/test.png");

}

void Atmosphere::draw(ShaderProgram* shader_program, const camera::Definition& camera)
{
    shader_program->set_uniform("inversed_projection_matrix", glm::inverse(camera.projectionMatrix()));
    shader_program->set_uniform("inversed_view_matrix", glm::translate(-camera.position()) * camera.camera_space_to_world_matrix());

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glDepthFunc(GL_EQUAL);
    m_screen_quad_geometry.draw_with_depth_test();
}
