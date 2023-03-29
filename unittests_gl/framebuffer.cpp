/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch.hpp>
#include <QGuiApplication>
#include <QRgb>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>
#include <QOffscreenSurface>
#include <QScreen>

#include "gl_engine/Framebuffer.h"
#include "gl_engine/ShaderProgram.h"
#include "gl_engine/helpers.h"

using gl_engine::Framebuffer;
using gl_engine::ShaderProgram;

static const char* const vertex_source = R"(
out highp vec2 texcoords;
void main() {
    vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0),
                             vec2(3.0, -1.0),
                             vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
})";

ShaderProgram create_debug_shader()
{
    static const char* const fragment_source = R"(
    out lowp vec4 out_Color;
    void main() {
        out_Color = vec4(0.2, 0.0, 1.0, 0.8);
    })";
    return ShaderProgram(vertex_source, fragment_source);
}

TEST_CASE("gl framebuffer")
{
    int argc = 0;
    char argv_c = '\0';
    std::array<char*, 1> argv = { &argv_c };
    QGuiApplication app(argc, argv.data());
    QOffscreenSurface surface;
    surface.create();
    const auto size = surface.size();
    const auto screensize = surface.screen()->size();
    QOpenGLContext c;
    REQUIRE(c.create());
    c.makeCurrent(&surface);
    QOpenGLDebugLogger logger;
    logger.initialize();
    logger.disableMessages(QList<GLuint>({ 131185 }));
    QObject::connect(&logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
        qDebug() << message;
    });
    logger.startLogging(QOpenGLDebugLogger::SynchronousLogging);

    QOpenGLExtraFunctions* f = c.extraFunctions();
    REQUIRE(f);
    SECTION("rgba8 bit")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 });
        b.resize({ 501, 211 });
        b.bind();
        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage tex = b.read_colour_attachment(0);
        //        tex.save("/home/madam/Documents/work/tuw/alpinemaps/test.png");
        Framebuffer::unbind();
        REQUIRE(!tex.isNull());
        CHECK(tex.width() == 501);
        CHECK(tex.height() == 211);
        bool good = true;
        for (int i = 0; i < tex.width(); ++i) {
            for (int j = 0; j < tex.height(); ++j) {
                good = good && (tex.pixel(i, j) == qRgba(51, 0, 255, 204));
            }
        }
        CHECK(good);
    }
    SECTION("rgba8 bit read benchmark")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 });
        b.resize({ 1920, 1080 });
        b.bind();
        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        f->glFinish();
        BENCHMARK("rgba8 bit read colour buffer")
        {
            return b.read_colour_attachment(0);
        };
    }

    SECTION("read pixel")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 });
        b.resize({ 1920, 1080 });
        b.bind();
        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        auto pixel = b.read_colour_attachment_pixel(0, glm::dvec2(0, 0));

        Framebuffer::unbind();
        CHECK(pixel[0] == unsigned(0.2f * 255));
        CHECK(pixel[1] == unsigned(0.0f * 255));
        CHECK(pixel[2] == unsigned(1.0f * 255));
        CHECK(pixel[3] == unsigned(0.8f * 255));
    }
}
