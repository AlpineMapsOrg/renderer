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

#include "UnittestGLContext.h"
#include "gl_engine/Framebuffer.h"
#include "gl_engine/ShaderProgram.h"
#include "gl_engine/helpers.h"
#include "nucleus/utils/texture_compression.h"

using gl_engine::Framebuffer;
using gl_engine::ShaderProgram;

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
    out lowp vec4 out_Color;
    void main() {
        out_Color = vec4(0.2, 0.0, 1.0, 0.8);
    })";
    ShaderProgram tmp(vertex_source, fragmentShaderOverride ? fragmentShaderOverride : fragment_source, gl_engine::ShaderCodeSource::PLAINTEXT);
    return tmp;
}
} // namespace

TEST_CASE("gl compressed textures")
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
        painter.drawRect(0, 0, 255, 255);
        test_texture.save("test_texture.png");
    }

    SECTION("compression")
    {
        const auto compressed = nucleus::utils::texture_compression::to_dxt1(test_texture);
        CHECK(compressed.size() == 256 * 256 * 2);
    }

    SECTION("verify test methodology")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, {{Framebuffer::ColourFormat::RGBA8}}, {256, 256});
        b.bind();
        QOpenGLTexture opengl_texture(test_texture);
        opengl_texture.setWrapMode(QOpenGLTexture::WrapMode::ClampToBorder);
        opengl_texture.setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
        opengl_texture.bind();

        ShaderProgram shader = create_debug_shader(R"(
            uniform sampler2D texture_sampler;
            in highp vec2 texcoords;
            out lowp vec4 out_color;
            void main() {
                out_color = texture(texture_sampler, vec2(texcoords.x, 1-texcoords.y));
            }
        )");
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        render_result.save("render_result.png");
        Framebuffer::unbind();
        REQUIRE(!render_result.isNull());
        CHECK(render_result.width() == test_texture.width());
        CHECK(render_result.height() == test_texture.height());
        double diff = 0;
        for (int i = 0; i < render_result.width(); ++i) {
            for (int j = 0; j < render_result.height(); ++j) {
                diff += std::abs(qRed(render_result.pixel(i, j)) - qRed(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qGreen(render_result.pixel(i, j)) - qGreen(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qBlue(render_result.pixel(i, j)) - qBlue(test_texture.pixel(i, j))) / 255.0;
            }
        }
        CHECK(diff / (256 * 256 * 3) < 0.01);
    }

    SECTION("compression")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, {{Framebuffer::ColourFormat::RGBA8}}, {256, 256});
        b.bind();
        QOpenGLTexture opengl_texture(QOpenGLTexture::Target2D);
        opengl_texture.setSize(256, 256);
        opengl_texture.setFormat(QOpenGLTexture::TextureFormat::RGBA_DXT1);
        // opengl_texture.setCompressedData();
        opengl_texture.setWrapMode(QOpenGLTexture::WrapMode::ClampToBorder);
        opengl_texture.setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
        opengl_texture.bind();

        ShaderProgram shader = create_debug_shader(R"(
            uniform sampler2D texture_sampler;
            in highp vec2 texcoords;
            out lowp vec4 out_color;
            void main() {
                out_color = texture(texture_sampler, vec2(texcoords.x, 1-texcoords.y));
            }
        )");
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        render_result.save("render_result.png");
        Framebuffer::unbind();
        REQUIRE(!render_result.isNull());
        CHECK(render_result.width() == test_texture.width());
        CHECK(render_result.height() == test_texture.height());
        double diff = 0;
        for (int i = 0; i < render_result.width(); ++i) {
            for (int j = 0; j < render_result.height(); ++j) {
                diff += std::abs(qRed(render_result.pixel(i, j)) - qRed(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qGreen(render_result.pixel(i, j)) - qGreen(test_texture.pixel(i, j))) / 255.0;
                diff += std::abs(qBlue(render_result.pixel(i, j)) - qBlue(test_texture.pixel(i, j))) / 255.0;
            }
        }
        CHECK(diff / (256 * 256 * 3) < 0.01);
    }
}
