#include "SSAO.h"

#include <random>
#include <cmath>
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>
#include "Framebuffer.h"
#include "ShaderProgram.h"

namespace gl_engine {

SSAO::SSAO(std::shared_ptr<ShaderProgram> program)
    :m_ssao_program(program)
{
     m_f = QOpenGLContext::currentContext()->extraFunctions();

    // GENERATE SAMPLE KERNEL
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    for (unsigned int i = 0; i < 64; ++i)
    {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / 64.0f;

        // scale samples s.t. they're more aligned to center of kernel
        scale = std::lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        m_ssao_kernel.push_back(sample);
    }

    // GENERATE NOISE TEXTURE
    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++)
    {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f); // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }

    m_ssao_noise_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
    m_ssao_noise_texture->setFormat(QOpenGLTexture::TextureFormat::RGB32F);
    m_ssao_noise_texture->setSize(4,4);
    m_ssao_noise_texture->setAutoMipMapGenerationEnabled(false);
    m_ssao_noise_texture->setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
    m_ssao_noise_texture->setWrapMode(QOpenGLTexture::WrapMode::Repeat);
    m_ssao_noise_texture->allocateStorage();
    m_ssao_noise_texture->setData(QOpenGLTexture::RGB, QOpenGLTexture::Float32, &ssaoNoise[0]);

    // GENERATE FRAMEBUFFER
    m_ssaobuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None, std::vector({Framebuffer::ColourFormat::R8}));
}

SSAO::~SSAO() {

}

void SSAO::draw(Framebuffer* gbuffer, helpers::ScreenQuadGeometry* geometry, const nucleus::camera::Definition& camera) {
    m_ssaobuffer->bind();
    m_f->glClear(GL_COLOR_BUFFER_BIT);
    auto p = m_ssao_program.get();
    p->bind();
    p->set_uniform("texin_position", 0);
    gbuffer->bind_colour_texture(3,0);
    p->set_uniform("texin_normal", 1);
    gbuffer->bind_colour_texture(2,1);
    p->set_uniform("texin_noise", 2);
    m_ssao_noise_texture->bind(2);
    p->set_uniform_array("samples", this->m_ssao_kernel);
    geometry->draw();
    m_ssaobuffer->unbind();

}

void SSAO::resize(glm::uvec2 vp_size) {
    m_ssaobuffer->resize(vp_size);
}

void SSAO::bind_ssao_texture(unsigned int location) {
    m_ssaobuffer->bind_colour_texture(0, location);
}

}
