/*****************************************************************************
 * Alpine Renderer
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
#include "gl_engine/UniformBuffer.h"
#include "gl_engine/UniformBufferObjects.h"
#include "gl_engine/helpers.h"

#include "UnittestGLContext.h"

using Catch::Approx;
using gl_engine::Framebuffer;
using gl_engine::ShaderProgram;

static const char* const vertex_source2 = R"(
out highp vec2 texcoords;
void main() {
    vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
})";

ShaderProgram create_debug_shader2(const char* fragmentShaderOverride = nullptr)
{
    static const char* const fragment_source = R"(
    out lowp vec4 out_Color;
    void main() {
        out_Color = vec4(0.2, 0.0, 1.0, 0.8);
    })";
    ShaderProgram tmp(vertex_source2, fragmentShaderOverride ? fragmentShaderOverride : fragment_source, gl_engine::ShaderCodeSource::PLAINTEXT);
    return tmp;
}

TEST_CASE("gl uniformbuffer")
{
    UnittestGLContext::initialise();

    const auto* c = QOpenGLContext::currentContext();
    QOpenGLExtraFunctions* f = c->extraFunctions();
    REQUIRE(f);

    SECTION("test buffer content")
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::R32UI} });
        ShaderProgram shader = create_debug_shader2(R"(
            layout (std140) uniform test_config {
                highp vec4 tv4;
                highp float tf32;
                highp uint tu32;
            } ubo;
            out highp uint out_Number;
            void main() {
                out_Number = 0u;
                if (ubo.tv4.x != 0.1)   out_Number = out_Number | 0x00000001u;
                if (ubo.tv4.y != 0.2)   out_Number = out_Number | 0x00000010u;
                if (ubo.tv4.z != 0.3)   out_Number = out_Number | 0x00000100u;
                if (ubo.tv4.w != 0.4)   out_Number = out_Number | 0x00001000u;
                if (ubo.tf32 != 0.5)    out_Number = out_Number | 0x00010000u;
                if (ubo.tu32 != 201u)   out_Number = out_Number | 0x00100000u;
            }
        )");
        auto ubo = std::make_unique<gl_engine::UniformBuffer<gl_engine::uboTestConfig>>(0, "test_config");
        ubo->init();
        ubo->bind_to_shader(&shader);

        b.bind();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();
        glm::u32vec1 value_at_0_0;
        b.read_colour_attachment_pixel(0, glm::dvec2(-1.0, -1.0), &value_at_0_0[0]);
        CHECK(value_at_0_0.x == 0u);
    }

    SECTION("test shared config buffer")
    {
        // NOTE: If theres an error here, check proper alignment first!!!
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::R32UI} });
        ShaderProgram shader = create_debug_shader2(R"(
            #include "shared_config.glsl"
            out highp uint out_Number;
            void main() {
                out_Number = 0u;
                if (conf.phong_enabled == 22u) out_Number = 1u;
            }
        )");
        auto ubo = std::make_unique<gl_engine::UniformBuffer<gl_engine::uboSharedConfig>>(0, "shared_config");
        ubo->init();
        ubo->bind_to_shader(&shader);

        ubo->data.m_phong_enabled = 22;
        ubo->update_gpu_data();

        b.bind();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();
        glm::u32vec1 value_at_0_0;
        b.read_colour_attachment_pixel(0, glm::dvec2(-1.0, -1.0), &value_at_0_0[0]);
        CHECK(value_at_0_0.x == 1u);
    }

    SECTION("test shared camera buffer")
    {
        // NOTE: If theres an error here, check proper alignment first!!!
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::R32UI} });
        ShaderProgram shader = create_debug_shader2(R"(
            #include "camera_config.glsl"
            out highp uint out_Number;
            void main() {
                out_Number = 0u;
                if (camera.viewport_size.y == 128.0) out_Number = 1u;
            }
        )");
        auto ubo = std::make_unique<gl_engine::UniformBuffer<gl_engine::uboCameraConfig>>(0, "camera_config");
        ubo->init();
        ubo->bind_to_shader(&shader);

        ubo->data.viewport_size.y = 128.0f;
        ubo->update_gpu_data();

        b.bind();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();
        glm::u32vec1 value_at_0_0;
        b.read_colour_attachment_pixel(0, glm::dvec2(-1.0, -1.0), &value_at_0_0[0]);
        CHECK(value_at_0_0.x == 1);
    }

    SECTION("test shared Shadow config buffer")
    {
        // NOTE: If theres an error here, check proper alignment first!!!
        Framebuffer b(Framebuffer::DepthFormat::None, { {Framebuffer::ColourFormat::R32UI} });
        ShaderProgram shader = create_debug_shader2(R"(
            #include "shadow_config.glsl"
            out highp uint out_Number;
            void main() {
                out_Number = 0u;
                if (shadow.shadowmap_size.y == 400.0) out_Number = 1u;
            }
        )");
        auto ubo = std::make_unique<gl_engine::UniformBuffer<gl_engine::uboShadowConfig>>(0, "shadow_config");
        ubo->init();
        ubo->bind_to_shader(&shader);

        ubo->data.shadowmap_size.y = 400.0;
        ubo->update_gpu_data();

        b.bind();
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();
        glm::u32vec1 value_at_0_0;
        b.read_colour_attachment_pixel(0, glm::dvec2(-1.0, -1.0), &value_at_0_0[0]);
        CHECK(value_at_0_0.x == 1u);
    }
    SECTION("encode decode shared buffer as base64 string")
    {
        auto byteLength = sizeof(gl_engine::uboSharedConfig);
        auto ubo = std::make_unique<gl_engine::UniformBuffer<gl_engine::uboSharedConfig>>(0, "shared_config");
        QByteArray before_encode_decode((char*)(void*)&ubo->data, byteLength);
        auto encoded = ubo->data_as_string();
        bool result = ubo->data_from_string(encoded);
        CHECK(result == true);
        QByteArray after_encode_decode((char*)(void*)&ubo->data, byteLength);
        CHECK(before_encode_decode == after_encode_decode);
    }
    SECTION("encode decode test buffer as base64 string")
    {
        auto byteLength = sizeof(gl_engine::uboTestConfig);
        auto ubo = std::make_unique<gl_engine::UniformBuffer<gl_engine::uboTestConfig>>(0, "test_config");
        QByteArray before_encode_decode((char*)(void*)&ubo->data, byteLength);
        auto encoded = ubo->data_as_string();
        bool result = ubo->data_from_string(encoded);
        CHECK(result == true);
        QByteArray after_encode_decode((char*)(void*)&ubo->data, byteLength);
        CHECK(before_encode_decode == after_encode_decode);
    }
    SECTION("test comparison operator for shared config")
    {
        gl_engine::uboSharedConfig ubo1;
        gl_engine::uboSharedConfig ubo2;
        gl_engine::uboSharedConfig ubo3;
        ubo3.m_overlay_shadowmaps_enabled = 200;
        CHECK((ubo1 != ubo2) == false);
        CHECK((ubo1 != ubo3) == true);
    }
}
