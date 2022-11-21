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
#include <QOpenGLDebugLogger>
#include <QOpenGLWindow>
#include <QOpenGLExtraFunctions>

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
        out_Color = vec4(1.0, 0.0, 0.0, 1.0);
    })";
    return ShaderProgram(vertex_source, fragment_source);
}

TEST_CASE("gl framebuffer")
{
    int argc = 0;
    char argv_c = '\0';
    std::array<char*, 1> argv = { &argv_c };
    QGuiApplication app(argc, argv.data());
    class GLWindow : public QOpenGLWindow {
        QGuiApplication* app = nullptr;

    public:
        GLWindow(QGuiApplication* app)
            : app(app)
        {
        }
        void initializeGL() override
        {
            QOpenGLDebugLogger logger;
            logger.initialize();
            connect(&logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
                qDebug() << message;
            });
            logger.startLogging(QOpenGLDebugLogger::SynchronousLogging);
            const auto c = QOpenGLContext::currentContext();
            REQUIRE(c);
            QOpenGLExtraFunctions* f = c->extraFunctions();
            REQUIRE(f);
            SECTION("create")
            {
                Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 });
                b.resize({ 100, 100 });
                b.bind();
                ShaderProgram shader = create_debug_shader();
                shader.bind();
                f->glDisable(GL_DEPTH_TEST);
                f->glClearColor(0.0, 0.0, 0.0, 1);
                f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                const auto screen_quad_geometry = gl_helpers::create_screen_quad_geometry();
                screen_quad_geometry.draw();
                Framebuffer::unbind();

                const auto tex = b.colour_attachment(0);
                REQUIRE(!tex.isNull());

            }

            app->quit();
        }
    } test(&app);
    test.show();
    app.exec();
}
