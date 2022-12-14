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
    m_framebuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None, std::vector({ Framebuffer::ColourFormat::RGBA8 }));
}

void Atmosphere::resize(const glm::uvec2& new_size)
{
    m_framebuffer->resize({ 1, new_size.y });
}

void Atmosphere::draw(ShaderProgram* atmosphere_program, const camera::Definition& camera, ShaderProgram* copy_program, Framebuffer* out)
{
    atmosphere_program->set_uniform("inversed_projection_matrix", glm::inverse(camera.projectionMatrix()));
    atmosphere_program->set_uniform("inversed_view_matrix", glm::translate(-camera.position()) * camera.camera_space_to_world_matrix());
    atmosphere_program->set_uniform("camera_position", glm::vec3(camera.position()));
    m_framebuffer->bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glDepthFunc(GL_ALWAYS);
    m_screen_quad_geometry.draw();
    m_framebuffer->unbind();

    out->bind();
    f->glDepthFunc(GL_EQUAL);
    m_framebuffer->bind_colour_texture(0);
    copy_program->bind();
    m_screen_quad_geometry.draw_with_depth_test();
}
