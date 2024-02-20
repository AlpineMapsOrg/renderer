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
#include <GoofyTC/goofy_tc.h>
#include <catch2/catch_test_macros.hpp>
#include <gl_engine/Texture.h>
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#endif

#include "UnittestGLContext.h"
#include "gl_engine/Framebuffer.h"
#include "gl_engine/ShaderProgram.h"
#include "gl_engine/helpers.h"
#include "nucleus/utils/CompressedTexture.h"

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
ShaderProgram create_debug_shader(const char* fragmentShaderOverride = nullptr)
{
    static const char* const fragment_source = R"(
        uniform sampler2D texture_sampler;
        in highp vec2 texcoords;
        out lowp vec4 out_color;
        void main() {
            out_color = texture(texture_sampler, vec2(texcoords.x, 1.0 - texcoords.y));
        }
    )";
    ShaderProgram tmp(vertex_source, fragmentShaderOverride ? fragmentShaderOverride : fragment_source, gl_engine::ShaderCodeSource::PLAINTEXT);
    return tmp;
}
} // namespace

TEST_CASE("gl texture")
{
    UnittestGLContext::initialise();

    const auto* c = QOpenGLContext::currentContext();
    QOpenGLExtraFunctions* f = c->extraFunctions();
    REQUIRE(f);

    QImage test_texture(256, 256, QImage::Format_ARGB32);
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

    SECTION("compression")
    {
        {
            const auto compressed = CompressedTexture(test_texture, CompressedTexture::Algorithm::DXT1);
            CHECK(compressed.n_bytes() == 256 * 128);
        }
        {
            const auto compressed = CompressedTexture(test_texture, CompressedTexture::Algorithm::ETC1);
            CHECK(compressed.n_bytes() == 256 * 128);
        }
        {
            const auto compressed = CompressedTexture(test_texture, CompressedTexture::Algorithm::Uncompressed_RGBA);
            CHECK(compressed.n_bytes() == 256 * 256 * 4);
        }
    }

    SECTION("verify test methodology")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, {{Framebuffer::ColourFormat::RGBA8}}, {256, 256});
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
        Framebuffer b(Framebuffer::DepthFormat::None, { { Framebuffer::ColourFormat::RGBA8 } }, { 256, 256 });
        b.bind();

        const auto compressed = CompressedTexture(test_texture, gl_engine::Texture::compression_algorithm());
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
        Framebuffer b(Framebuffer::DepthFormat::None, { { Framebuffer::ColourFormat::RGBA8 } }, { 256, 256 });
        b.bind();

        const auto compressed = CompressedTexture(test_texture, CompressedTexture::Algorithm::Uncompressed_RGBA);
        gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2d, gl_engine::Texture::Format::RGBA8);
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
        CHECK(diff / (256 * 256 * 3) < 0.001);
    }

    SECTION("rg8")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { { Framebuffer::ColourFormat::RGBA8 } }, { 1, 1 });
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

    SECTION("red16")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { { Framebuffer::ColourFormat::RGBA8 } }, { 1, 1 });
        b.bind();

        const auto tex = nucleus::Raster<uint16_t>({ 1, 1 }, uint16_t(120 * 256));
        gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2d, gl_engine::Texture::Format::R16UI);
        opengl_texture.bind(0);
        opengl_texture.setParams(gl_engine::Texture::Filter::Nearest, gl_engine::Texture::Filter::Nearest);
        opengl_texture.upload(tex);

        ShaderProgram shader = create_debug_shader(R"(
            uniform mediump usampler2D texture_sampler;
            in highp vec2 texcoords;
            out lowp vec4 out_color;
            void main() {
                mediump uint v = texture(texture_sampler, vec2(0.5, 0.5)).r;
                highp float v2 = float(v);  // need temporary for android, otherwise it is cast to a mediump float and 0 is returned.
                out_color = vec4(v2 / 65535.0, 0, 0, 1);
            }
        )");
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        // render_result.save("render_result.png");
        Framebuffer::unbind();
        CHECK(qRed(render_result.pixel(0, 0)) == 120);
        CHECK(qGreen(render_result.pixel(0, 0)) == 0);
        CHECK(qBlue(render_result.pixel(0, 0)) == 0);
        CHECK(qAlpha(render_result.pixel(0, 0)) == 255);
    }

    SECTION("rgba array")
    {
        Framebuffer framebuffer(Framebuffer::DepthFormat::None,
            { { Framebuffer::ColourFormat::RGBA8 }, { Framebuffer::ColourFormat::RGBA8 }, { Framebuffer::ColourFormat::RGBA8 } }, { 256, 256 });
        framebuffer.bind();

        std::array texture_types = { CompressedTexture::Algorithm::Uncompressed_RGBA, gl_engine::Texture::compression_algorithm() };
        for (auto texture_type : texture_types) {
            const auto format = (texture_type == CompressedTexture::Algorithm::Uncompressed_RGBA) ? gl_engine::Texture::Format::RGBA8
                                                                                                  : gl_engine::Texture::Format::CompressedRGBA8;
            gl_engine::Texture opengl_texture(gl_engine::Texture::Target::_2dArray, format);
            opengl_texture.bind(0);
            opengl_texture.setParams(gl_engine::Texture::Filter::Linear, gl_engine::Texture::Filter::Linear);
            opengl_texture.allocate_array(256, 256, 3);
            {
                const auto compressed = CompressedTexture(test_texture, texture_type);
                opengl_texture.upload(compressed, 0);
            }
            {
                QImage test_texture(256, 256, QImage::Format_ARGB32);
                test_texture.fill(qRgba(42, 142, 242, 255));
                const auto compressed = CompressedTexture(test_texture, texture_type);
                opengl_texture.upload(compressed, 1);
            }
            {
                QImage test_texture(256, 256, QImage::Format_ARGB32);
                test_texture.fill(qRgba(222, 111, 0, 255));
                const auto compressed = CompressedTexture(test_texture, texture_type);
                opengl_texture.upload(compressed, 2);
            }
            ShaderProgram shader = create_debug_shader(R"(
                uniform sampler2DArray texture_sampler;
                in highp vec2 texcoords;
                out lowp vec4 out_color_0;
                out lowp vec4 out_color_1;
                out lowp vec4 out_color_2;
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
}
