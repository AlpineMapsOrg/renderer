#include "Atmosphere.h"

#include "Framebuffer.h"
#include "ShaderProgram.h"
#include "alpine_gl_renderer/GLHelpers.h"

Atmosphere::Atmosphere()
{
    Framebuffer compute_buffer(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::Float32 });
    compute_buffer.resize({ 512, 512 });
    compute_buffer.bind();

    ShaderProgram compute_shader(ShaderProgram::Files({ "gl_shaders/screen_pass.vert" }), ShaderProgram::Files({ "gl_shaders/atmosphere_compute.frag" }));
    compute_shader.bind();

    gl_helpers::create_screen_quad_geometry().draw();

    compute_buffer.read_colour_attachment(0).save("/home/madam/Documents/work/tuw/alpinemaps/test.png");

}
