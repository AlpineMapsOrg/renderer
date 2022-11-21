/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include <catch2/catch.hpp>
#include <QGuiApplication>
#include <QRgb>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>
#include <QOffscreenSurface>

#include "alpine_gl_renderer/GLHelpers.h"
#include "alpine_gl_renderer/ShaderProgram.h"
#include "alpine_gl_renderer/Framebuffer.h"

ShaderProgram create_debug_shader()
{
    static const char* const vertex_source = R"(
    void main() {
        vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0),
                                 vec2(3.0, -1.0),
                                 vec2(-1.0, 3.0));
        gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    })";

    static const char* const fragment_source = R"(
    out lowp vec4 out_Color;
    void main() {
        out_Color = vec4(0.2, 0.4, 0.6, 0.8);
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
    SECTION("create")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 });
        b.resize({ 10, 20 });
        b.bind();
        ShaderProgram shader = create_debug_shader();
        shader.bind();
        f->glDisable(GL_DEPTH_TEST);
        f->glClearColor(0.0, 0.0, 0.0, 1);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        const auto screen_quad_geometry = gl_helpers::create_screen_quad_geometry();
        screen_quad_geometry.draw();
        f->glFinish();

        const QImage tex = b.read_colour_attachment(0);
        Framebuffer::unbind();
        f->glFinish();
        REQUIRE(!tex.isNull());
        REQUIRE(tex.width() == 10);
        REQUIRE(tex.height() == 20);
        for (int i = 0; i < tex.width(); ++i) {
            for (int j = 0; j < tex.height(); ++j) {
                CHECK(tex.pixel(i, j) == qRgba(51, 102, 153, 204));
            }
        }

    }
}
