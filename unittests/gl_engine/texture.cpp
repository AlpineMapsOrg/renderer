/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
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

#include <QPainter>
#include <catch2/catch_test_macros.hpp>

#include "UnittestGLContext.h"
#include <gl_engine/Framebuffer.h>
#include <gl_engine/ShaderProgram.h>
#include <gl_engine/Texture.h>
#include <gl_engine/helpers.h>
#include <nucleus/utils/ColourTexture.h>
#include <nucleus/utils/tile_conversion.h>

using gl_engine::Framebuffer;
using gl_engine::ShaderProgram;
using namespace nucleus::utils;

static const char* const vertex_source = R"(
out highp vec2 texcoords;
void main() {
    vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
})";

namespace {
ShaderProgram create_debug_shader(const QString& fragment_source = R"(
        uniform sampler2D texture_sampler;
        in highp vec2 texcoords;
        out lowp vec4 out_color;
        void main() {
            out_color = texture(texture_sampler, vec2(texcoords.x, 1.0 - texcoords.y));
        }
    )")
{
    // qDebug() << fragment_source;
    ShaderProgram tmp(vertex_source, fragment_source, gl_engine::ShaderCodeSource::PLAINTEXT);
    return tmp;
}

template <int length, typename Type> QString texel_component(const glm::vec<length, Type>& texel, int i)
{
    if (i < length)
        return QString("%1u").arg(texel[i]);
    return "0u";
};

template <typename Type> QString texel_component(const Type& texel, int) { return QString("%1u").arg(texel); };

template <int length, typename Type, typename TexelType = glm::vec<length, Type>>
void test_unsigned_texture_with(const TexelType& texel_value, gl_engine::Texture::Format format)
{
    Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 1, 1 });
    b.bind();

    const auto tex = nucleus::Raster<TexelType>({ 1, 1 }, texel_value);
    gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2d, format);
    opengl_texture.bind(0);
    opengl_texture.setParams(gl_engine::Texture::Filter::Nearest, gl_engine::Texture::Filter::Nearest);
    opengl_texture.upload(tex);

    const auto precision = []() -> QString {
        if (sizeof(Type) == 1)
            return "lowp";
        if (sizeof(Type) == 2)
            return "mediump";
        if (sizeof(Type) == 4)
            return "highp";
        assert(false);
        return "Type has unexpected size";
    };

    ShaderProgram shader = create_debug_shader(QString(R"(
            uniform %1 usampler2D texture_sampler;
            out lowp vec4 out_color;
            void main() {
                %1 uvec4 v = texelFetch(texture_sampler, ivec2(0, 0), 0);
                out_color = vec4((v.r == %2) ? 123.0 / 255.0 : 9.0 / 255.0,
                                 (%6 < 2 || v.g == %3) ? 124.0 / 255.0 : 9.0 / 255.0,
                                 (%6 < 3 || v.b == %4) ? 125.0 / 255.0 : 9.0 / 255.0,
                                 (%6 < 4 || v.a == %5) ? 126.0 / 255.0 : 9.0 / 255.0);
            }
        )")
                                                   .arg(precision())
                                                   .arg(texel_component(texel_value, 0))
                                                   .arg(texel_component(texel_value, 1))
                                                   .arg(texel_component(texel_value, 2))
                                                   .arg(texel_component(texel_value, 3))
                                                   .arg(length));
    shader.bind();
    gl_engine::helpers::create_screen_quad_geometry().draw();

    const QImage render_result = b.read_colour_attachment(0);
    // render_result.save("render_result.png");
    Framebuffer::unbind();
    CHECK(qRed(render_result.pixel(0, 0)) == 123);
    CHECK(qGreen(render_result.pixel(0, 0)) == 124);
    CHECK(qBlue(render_result.pixel(0, 0)) == 125);
    CHECK(qAlpha(render_result.pixel(0, 0)) == 126);
}
} // namespace

TEST_CASE("gl texture")
{
    UnittestGLContext::initialise();

    const auto* c = QOpenGLContext::currentContext();
    QOpenGLExtraFunctions* f = c->extraFunctions();
    REQUIRE(f);

    QImage test_texture(256, 256, QImage::Format_RGBA8888);
    test_texture.fill(qRgba(0, 0, 0, 255));
    {
        QPainter painter(&test_texture);
        QRadialGradient grad;
        grad.setCenter(85, 108);
        grad.setRadius(100);
        grad.setFocalPoint(120, 150);
        grad.setColorAt(0, qRgb(245, 200, 5));
        grad.setColorAt(1, qRgb(145, 100, 0));
        grad.setSpread(QGradient::ReflectSpread);
        painter.setBrush(grad);
        painter.setPen(qRgba(242, 0, 42, 255));
        // painter.drawRect(-1, -1, 257, 257);
        painter.drawRect(0, 0, 255, 255);
        test_texture.save("test_texture.png");
    }

    const auto test_raster = nucleus::utils::tile_conversion::to_rgba8raster(test_texture);

    SECTION("compression")
    {
        {
            const auto compressed = ColourTexture(test_raster, ColourTexture::Format::DXT1);
            CHECK(compressed.n_bytes() == 256 * 128);
        }
        {
            const auto compressed = ColourTexture(test_raster, ColourTexture::Format::ETC1);
            CHECK(compressed.n_bytes() == 256 * 128);
        }
        {
            const auto compressed = ColourTexture(test_raster, ColourTexture::Format::Uncompressed_RGBA);
            CHECK(compressed.n_bytes() == 256 * 256 * 4);
        }
    }

    SECTION("verify test methodology")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 256, 256 });
        b.bind();
        QOpenGLTexture opengl_texture(test_texture);
        opengl_texture.setWrapMode(QOpenGLTexture::WrapMode::ClampToBorder);
        opengl_texture.setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
        opengl_texture.bind();

        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        // render_result.save("render_result.png");
        Framebuffer::unbind();
        double diff = 0;
        for (int i = 0; i < render_result.width(); ++i) {
            for (int j = 0; j < render_result.height(); ++j) {
                diff += std::abs(qRed(render_result.pixel(i, j)) - qRed(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qGreen(render_result.pixel(i, j)) - qGreen(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qBlue(render_result.pixel(i, j)) - qBlue(test_texture.pixel(i, j))) / 255.0;
            }
        }
        CHECK(diff / (256 * 256 * 3) < 0.001);
    }

    SECTION("compressed rgba")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 256, 256 });
        b.bind();

        const auto compressed = ColourTexture(test_raster, gl_engine::Texture::compression_algorithm());
        gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2d, gl_engine::Texture::Format::CompressedRGBA8);
        opengl_texture.bind(0);
        opengl_texture.setParams(gl_engine::Texture::Filter::Linear, gl_engine::Texture::Filter::Linear);
        opengl_texture.upload(compressed);

        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        // render_result.save("render_result.png");
        Framebuffer::unbind();
        double diff = 0;
        for (int i = 0; i < render_result.width(); ++i) {
            for (int j = 0; j < render_result.height(); ++j) {
                diff += std::abs(qRed(render_result.pixel(i, j)) - qRed(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qGreen(render_result.pixel(i, j)) - qGreen(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qBlue(render_result.pixel(i, j)) - qBlue(test_texture.pixel(i, j))) / 255.0;
            }
        }
        CHECK(diff / (256 * 256 * 3) < 0.017);
    }

    SECTION("rgba")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 256, 256 });
        b.bind();
        
        const auto compressed = ColourTexture(test_raster, ColourTexture::Format::Uncompressed_RGBA);
        gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2d, gl_engine::Texture::Format::RGBA8);
        opengl_texture.bind(0);
        opengl_texture.setParams(gl_engine::Texture::Filter::Linear, gl_engine::Texture::Filter::Linear);
        opengl_texture.upload(compressed);

        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        render_result.save("render_result.png");
        Framebuffer::unbind();
        double diff = 0;
        for (int i = 0; i < render_result.width(); ++i) {
            for (int j = 0; j < render_result.height(); ++j) {
                diff += std::abs(qRed(render_result.pixel(i, j)) - qRed(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qGreen(render_result.pixel(i, j)) - qGreen(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qBlue(render_result.pixel(i, j)) - qBlue(test_texture.pixel(i, j))) / 255.0;
            }
        }
        CHECK(diff / (256 * 256 * 3) < 0.001);
    }

    SECTION("rg8")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 1, 1 });
        b.bind();

        const auto tex = nucleus::Raster<glm::u8vec2>({ 1, 1 }, glm::u8vec2(240, 120));
        gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2d, gl_engine::Texture::Format::RG8);
        opengl_texture.bind(0);
        opengl_texture.setParams(gl_engine::Texture::Filter::Linear, gl_engine::Texture::Filter::Linear);
        opengl_texture.upload(tex);

        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        // render_result.save("render_result.png");
        Framebuffer::unbind();
        CHECK(qRed(render_result.pixel(0, 0)) == 240);
        CHECK(qGreen(render_result.pixel(0, 0)) == 120);
        CHECK(qBlue(render_result.pixel(0, 0)) == 0);
        CHECK(qAlpha(render_result.pixel(0, 0)) == 255);
    }

    SECTION("rgba8ui") { test_unsigned_texture_with<4, unsigned char>({ 1, 2, 255, 140 }, gl_engine::Texture::Format::RGBA8UI); }
    SECTION("rg32ui") { test_unsigned_texture_with<2, uint32_t>({ 3000111222, 4000111222 }, gl_engine::Texture::Format::RG32UI); }
    SECTION("red16ui") { test_unsigned_texture_with<1, uint16_t, uint16_t>(uint16_t(60123), gl_engine::Texture::Format::R16UI); }
    SECTION("red32ui") { test_unsigned_texture_with<1, uint32_t, uint32_t>(uint32_t(4000111222), gl_engine::Texture::Format::R32UI); }

    SECTION("rgba array (compressed and uncompressed)")
    {
        Framebuffer framebuffer(Framebuffer::DepthFormat::None,
            { Framebuffer::ColourFormat::RGBA8, Framebuffer::ColourFormat::RGBA8, Framebuffer::ColourFormat::RGBA8 }, { 256, 256 });
        framebuffer.bind();
        
        std::array texture_types = { ColourTexture::Format::Uncompressed_RGBA, gl_engine::Texture::compression_algorithm() };
        for (auto texture_type : texture_types) {
            const auto format
                = (texture_type == ColourTexture::Format::Uncompressed_RGBA) ? gl_engine::Texture::Format::RGBA8 : gl_engine::Texture::Format::CompressedRGBA8;
            gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2dArray, format);
            opengl_texture.bind(0);
            opengl_texture.setParams(gl_engine::Texture::Filter::Linear, gl_engine::Texture::Filter::Linear);
            opengl_texture.allocate_array(256, 256, 3);
            {
                const auto compressed = ColourTexture(test_raster, texture_type);
                opengl_texture.upload(compressed, 0);
            }
            {
                auto test_raster = nucleus::Raster<glm::u8vec4>(glm::uvec2(256), glm::u8vec4(42,142,242,255));
                const auto compressed = ColourTexture(test_raster, texture_type);
                opengl_texture.upload(compressed, 1);
            }
            {
                auto test_raster = nucleus::Raster<glm::u8vec4>(glm::uvec2(256), glm::u8vec4(222,111,0,255));
                const auto compressed = ColourTexture(test_raster, texture_type);
                opengl_texture.upload(compressed, 2);
            }
            ShaderProgram shader = create_debug_shader(R"(
                uniform lowp sampler2DArray texture_sampler;
                in highp vec2 texcoords;
                layout (location = 0) out lowp vec4 out_color_0;
                layout (location = 1) out lowp vec4 out_color_1;
                layout (location = 2) out lowp vec4 out_color_2;
                void main() {
                    out_color_0 = texture(texture_sampler, vec3(texcoords.x, 1.0 - texcoords.y, 0.0));
                    out_color_1 = texture(texture_sampler, vec3(texcoords.x, 1.0 - texcoords.y, 1.0));
                    out_color_2 = texture(texture_sampler, vec3(texcoords.x, 1.0 - texcoords.y, 2.0));
                }
            )");
            shader.bind();
            gl_engine::helpers::create_screen_quad_geometry().draw();

            {
                const QImage render_result = framebuffer.read_colour_attachment(0);
                // render_result.save("render_result.png");
                // test_texture.save("test_texture.png");
                double diff = 0;
                for (int i = 0; i < render_result.width(); ++i) {
                    for (int j = 0; j < render_result.height(); ++j) {
                        diff += std::abs(qRed(render_result.pixel(i, j)) - qRed(test_texture.pixel(i, j))) / 255.0;
                        diff += std::abs(qGreen(render_result.pixel(i, j)) - qGreen(test_texture.pixel(i, j))) / 255.0;
                        diff += std::abs(qBlue(render_result.pixel(i, j)) - qBlue(test_texture.pixel(i, j))) / 255.0;
                    }
                }
                CHECK(diff / (256 * 256 * 3) < 0.017);
            }
            {
                const QImage render_result = framebuffer.read_colour_attachment(1);
                // render_result.save("render_result1.png");
                double diff = 0;
                for (int i = 0; i < render_result.width(); ++i) {
                    for (int j = 0; j < render_result.height(); ++j) {
                        diff += std::abs(qRed(render_result.pixel(i, j)) - 42) / 255.0;
                        diff += std::abs(qGreen(render_result.pixel(i, j)) - 142) / 255.0;
                        diff += std::abs(qBlue(render_result.pixel(i, j)) - 242) / 255.0;
                    }
                }
                CHECK(diff / (256 * 256 * 3) < 0.017);
            }
            {
                const QImage render_result = framebuffer.read_colour_attachment(2);
                // render_result.save("render_result2.png");
                double diff = 0;
                for (int i = 0; i < render_result.width(); ++i) {
                    for (int j = 0; j < render_result.height(); ++j) {
                        diff += std::abs(qRed(render_result.pixel(i, j)) - 222) / 255.0;
                        diff += std::abs(qGreen(render_result.pixel(i, j)) - 111) / 255.0;
                        diff += std::abs(qBlue(render_result.pixel(i, j)) - 0) / 255.0;
                    }
                }
                CHECK(diff / (256 * 256 * 3) < 0.017);
            }
        }
    }

    SECTION("red16 array")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8, Framebuffer::ColourFormat::RGBA8 }, { 1, 1 });
        b.bind();

        gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2dArray, gl_engine::Texture::Format::R16UI);
        opengl_texture.allocate_array(1, 1, 2);
        opengl_texture.setParams(gl_engine::Texture::Filter::Nearest, gl_engine::Texture::Filter::Nearest);
        opengl_texture.upload(nucleus::Raster<uint16_t>({ 1, 1 }, uint16_t((120 * 65535) / 255)), 0);
        opengl_texture.upload(nucleus::Raster<uint16_t>({ 1, 1 }, uint16_t((190 * 65535) / 255)), 1);

        ShaderProgram shader = create_debug_shader(R"(
            uniform mediump usampler2DArray texture_sampler;
            layout (location = 0) out lowp vec4 out_color1;
            layout (location = 1) out lowp vec4 out_color2;
            void main() {
                {
                    mediump uint v = texture(texture_sampler, vec3(0.5, 0.5, 0)).r;
                    highp float v2 = float(v);  // need temporary for android, otherwise it is cast to a mediump float and 0 is returned.
                    out_color1 = vec4(v2 / 65535.0, 0, 0, 1);
                }

                {
                    mediump uint v = texture(texture_sampler, vec3(0.5, 0.5, 1)).r;
                    highp float v2 = float(v);  // need temporary for android, otherwise it is cast to a mediump float and 0 is returned.
                    out_color2 = vec4(v2 / 65535.0, 0, 0, 1);
                }
            }
        )");
        shader.bind();
        opengl_texture.bind(0);
        shader.set_uniform("texture_sampler", 0);
        gl_engine::helpers::create_screen_quad_geometry().draw();

        {
            const QImage render_result = b.read_colour_attachment(0);
            CHECK(qRed(render_result.pixel(0, 0)) == 120);
            CHECK(qGreen(render_result.pixel(0, 0)) == 0);
            CHECK(qBlue(render_result.pixel(0, 0)) == 0);
            CHECK(qAlpha(render_result.pixel(0, 0)) == 255);
        }
        {
            const QImage render_result = b.read_colour_attachment(1);
            CHECK(qRed(render_result.pixel(0, 0)) == 190);
            CHECK(qGreen(render_result.pixel(0, 0)) == 0);
            CHECK(qBlue(render_result.pixel(0, 0)) == 0);
            CHECK(qAlpha(render_result.pixel(0, 0)) == 255);
        }
    }
}
