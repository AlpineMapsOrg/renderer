/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#define CATCH_CONFIG_ENABLE_BENCHMARKING

#include <iostream>

#include <QFile>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>
#include <QRgb>
#include <QScreen>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "gl_engine/Framebuffer.h"
#include "gl_engine/ShaderProgram.h"
#include "gl_engine/helpers.h"
#include "nucleus/utils/bit_coding.h"

#include "UnittestGLContext.h"

#ifdef ANDROID
#include "GLES3/gl3.h"
#endif

using Catch::Approx;
using gl_engine::Framebuffer;
using gl_engine::ShaderProgram;

static const char* const vertex_source = R"(
out highp vec2 texcoords;
void main() {
    vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
})";

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

TEST_CASE("gl framebuffer")
{
    UnittestGLContext::initialise();
    const auto* c = QOpenGLContext::currentContext();
    QOpenGLExtraFunctions* f = c->extraFunctions();
    REQUIRE(f);
    SECTION("rgba8 bit")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::RGBA8} }, { 501, 211 });
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
    // Only color renderable on WEBGL with EXT_color_buffer_float extension
    SECTION("rgba32f color format")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::RGBA32F} });
        b.bind();
        ShaderProgram shader = create_debug_shader( R"(
            out highp vec4 out_Color;
            void main() {
                out_Color = vec4(1.0, 20000.0, 600000000.0, 15000000000.0);
            }
        )");
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();
        glm::vec4 value_at_0_0 = b.read_colour_attachment_pixel<glm::vec4>(0, glm::dvec2(-1.0, -1.0));
        CHECK(value_at_0_0.x == Approx(1.0f));
        CHECK(value_at_0_0.y == Approx(20000.0f));
        CHECK(value_at_0_0.z == Approx(600000000.0f));
        CHECK(value_at_0_0.w == Approx(15000000000.0f));
    }

    SECTION("rg16 direct texture test")
    {
        GLsizei width = 64;
        GLsizei height = 64;
        GLenum error;

        // Generate the texture
        GLuint textureID;
        f->glGenTextures(1, &textureID);
        f->glBindTexture(GL_TEXTURE_2D, textureID);
        error = f->glGetError();
        CHECK(error == GL_NO_ERROR);
        if (error != GL_NO_ERROR) {
            qCritical() << "Texture generation or binding failed with error" << error;
        }

        // Create an array filled with the value 400 for each RG component
        std::vector<GLushort> data;
        data.resize(width * height * 2);
        for (int i = 0; i < width * height * 2; i += 2) {
            data[i] = 400;     // R component
            data[i + 1] = 400; // G component
        }

        // Define the texture image
        f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16UI, width, height, 0, GL_RG_INTEGER, GL_UNSIGNED_SHORT, data.data());
        error = f->glGetError();
        CHECK(error == GL_NO_ERROR);
        if (error != GL_NO_ERROR) {
            qCritical() << "glTexImage2D failed with error" << error;
        }

        // Set the texture parameters
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Unbind the texture
        f->glBindTexture(GL_TEXTURE_2D, 0);
    }
    SECTION("rg16 QOpenGLTexture test")
    {
        GLsizei width = 64;
        GLsizei height = 64;

        // Create the texture object
        QOpenGLTexture texture(QOpenGLTexture::Target2D);
        texture.setFormat(QOpenGLTexture::RG16U);
        texture.setSize(width, height);

        // IMPORTANT: We have to manually define PixelFormat and PixelType. The default won't work for the OpenGL ES implementation!
        texture.allocateStorage(QOpenGLTexture::PixelFormat::RG_Integer, QOpenGLTexture::PixelType::UInt16);

        // Fill the array with the value 400 for each RG component
        std::vector<GLushort> data;
        data.resize(width * height * 2);
        for (int i = 0; i < width * height * 2; i += 2) {
            data[i] = 400;     // R component
            data[i + 1] = 400; // G component
        }

        // Upload the data to the texture
        texture.setData(0, QOpenGLTexture::PixelFormat::RG_Integer, QOpenGLTexture::PixelType::UInt16, data.data());

        // Set the texture parameters
        texture.setMinificationFilter(QOpenGLTexture::Nearest);
        texture.setMagnificationFilter(QOpenGLTexture::Nearest);
        texture.setWrapMode(QOpenGLTexture::ClampToEdge);


        // Check for errors
        GLenum error = f->glGetError();
        CHECK(error == GL_NO_ERROR);
        if (error != GL_NO_ERROR) {
            qCritical() << "An error occurred while setting up the texture:" << error;
        }
    }

    SECTION("rgba8 bit pixel read benchmark")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::RGBA8} });
        b.bind();
        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();
        f->glFinish();

        glm::u8vec4 value = {};
        BENCHMARK("rgba8 bit pixel read colour buffer")
        {
            value += b.read_colour_attachment_pixel<glm::u8vec4>(0, glm::dvec2(-1.0, -1.0));
        };
    }

#ifndef __EMSCRIPTEN__
    // NOTE: Tests dont terminate on webgl for me if this benchmark is enabled. The function is not in use anyway...
    SECTION("rgba8 bit read benchmark")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::RGBA8} }, { 1920, 1080 });
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
#endif
    SECTION("read pixel")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::RGBA8} }, { 1920, 1080 });
        b.bind();
        ShaderProgram shader = create_debug_shader();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        auto pixel = b.read_colour_attachment_pixel<glm::u8vec4>(0, glm::dvec2(0, 0));

        Framebuffer::unbind();
        CHECK(pixel[0] == unsigned(0.2f * 255));
        CHECK(pixel[1] == unsigned(0.0f * 255));
        CHECK(pixel[2] == unsigned(1.0f * 255));
        CHECK(pixel[3] == unsigned(0.8f * 255));
    }
    SECTION("f32 depth buffer")
    {
        QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
        Framebuffer b(Framebuffer::DepthFormat::Float32, {});
        b.bind();
        f->glClearDepthf(0.9);
        f->glClear(GL_DEPTH_BUFFER_BIT);
        f->glDepthFunc(GL_ALWAYS);
        ShaderProgram p1 = create_debug_shader(R"(
            void main() {
                gl_FragDepth = 0.42;
            }
        )");
        p1.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw_with_depth_test();
        f->glFinish();
        // Now lets try to use it as an input texture
        Framebuffer b2(Framebuffer::DepthFormat::None, { { Framebuffer::ColourFormat::RGBA32F } });
        b2.bind();
        ShaderProgram p2 = create_debug_shader(R"(
            in highp vec2 texcoords;
            out highp vec4 out_Color;
            uniform highp sampler2D texin_depth;
            void main() {
                highp float depth = texture(texin_depth, texcoords).r;
                out_Color = vec4(depth, depth, depth, depth);
            }
        )");
        p2.bind();
        p2.set_uniform("texin_depth", 0);
        b.bind_depth_texture(0);
        gl_engine::helpers::create_screen_quad_geometry().draw();
        f->glFinish();
        const auto value_at_0_0 = b2.read_colour_attachment_pixel<glm::vec4>(0, glm::dvec2(-1.0, -1.0));
        CHECK(value_at_0_0.x == Catch::Approx(0.42));
    }
}
