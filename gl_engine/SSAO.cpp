/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#include "SSAO.h"

#include <random>
#include <cmath>
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>
#include "Framebuffer.h"
#include "ShaderProgram.h"

namespace gl_engine {

SSAO::SSAO(std::shared_ptr<ShaderProgram> program, std::shared_ptr<ShaderProgram> blur_program)
    :m_ssao_program(program), m_ssao_blur_program(blur_program)
{
     m_f = QOpenGLContext::currentContext()->extraFunctions();

    // GENERATE SAMPLE KERNEL
    recreate_kernel();

    // GENERATE NOISE TEXTURE
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
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
    m_ssaobuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None, std::vector{ TextureDefinition{Framebuffer::ColourFormat::R8}});
    m_ssao_blurbuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None, std::vector{ TextureDefinition{Framebuffer::ColourFormat::R8}});
}

void SSAO::recreate_kernel(unsigned int size) {
    assert(size <= MAX_SSAO_KERNEL_SIZE);
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    m_ssao_kernel.clear();
    m_ssao_kernel.reserve(size);
    for (unsigned int i = 0; i < size; ++i)
    {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / size;

        // scale samples s.t. they're more aligned to center of kernel
        scale = std::lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        m_ssao_kernel.push_back(sample);
    }
}

SSAO::~SSAO() {

}

void SSAO::draw(Framebuffer* gbuffer, helpers::ScreenQuadGeometry* geometry,
    const nucleus::camera::Definition&, unsigned int kernel_size, unsigned int blur_level)
{
    m_ssaobuffer->bind();
    auto p = m_ssao_program.get();
    p->bind();
    p->set_uniform("texin_position", 0);
    gbuffer->bind_colour_texture(1,0);
    p->set_uniform("texin_normal", 1);
    gbuffer->bind_colour_texture(2,1);
    p->set_uniform("texin_noise", 2);
    m_ssao_noise_texture->bind(2);

    if (kernel_size != m_ssao_kernel.size()) recreate_kernel(kernel_size);
    p->set_uniform_array("samples", this->m_ssao_kernel);
    geometry->draw();
    m_ssaobuffer->unbind();
    p->release();

    if (blur_level > 0) {
        p = m_ssao_blur_program.get();
        p->bind();
        p->set_uniform("texin_ssao", 0);

        // BLUR HORIZONTAL
        m_ssao_blurbuffer->bind();
        m_ssaobuffer->bind_colour_texture(0,0);
        p->set_uniform("direction", 0);
        geometry->draw();
        m_ssao_blurbuffer->unbind();

        // BLUR VERTICAL
        m_ssaobuffer->bind();
        m_ssao_blurbuffer->bind_colour_texture(0,0);
        p->set_uniform("direction", 1);
        geometry->draw();
        m_ssaobuffer->unbind();
        p->release();
    }
}

void SSAO::resize(glm::uvec2 vp_size) {
    m_ssaobuffer->resize(vp_size);
    m_ssao_blurbuffer->resize(vp_size);
}

void SSAO::bind_ssao_texture(unsigned int location) {
    m_ssaobuffer->bind_colour_texture(0, location);
}

}
