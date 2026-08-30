/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek
 * SPDX-License-Identifier: GPL-3.0-or-later
 *****************************************************************************/

#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

#include <gl_engine/Framebuffer.h>
#include <gl_engine/ShaderProgram.h>
#include <gl_engine/Texture.h>
#include <gl_engine/TextureCompressor.h>
#include <gl_engine/helpers.h>

namespace {
using Raster = radix::Raster<glm::u8vec4>;
using Compressor = gl_engine::TextureCompressor;

Raster test_raster(unsigned resolution)
{
    Raster result { glm::uvec2(resolution) };
    for (unsigned y = 0; y < resolution; ++y) {
        for (unsigned x = 0; x < resolution; ++x) {
            result.pixel({ x, y }) = glm::u8vec4(
                uint8_t((x * 17 + y * 3) & 255),
                uint8_t((x * 5 + y * 11) & 255),
                uint8_t((x * 7 + y * 13) & 255),
                255);
        }
    }
    return result;
}

std::shared_ptr<gl_engine::Texture> rgba_scratch(std::span<const Raster> sources, unsigned mip_levels)
{
    const auto width = unsigned(sources.front().width());
    const auto height = unsigned(sources.front().height());
    auto scratch = std::make_shared<gl_engine::Texture>(
        gl_engine::Texture::Target::_2dArray, gl_engine::Texture::Format::RGBA8);
    scratch->setParams(gl_engine::Texture::Filter::Nearest, gl_engine::Texture::Filter::Nearest);
    scratch->allocate_array(width, height, unsigned(sources.size()), mip_levels);
    for (size_t layer = 0; layer < sources.size(); ++layer)
        scratch->upload(sources[layer], unsigned(layer));
    if (mip_levels > 1)
        scratch->generate_mipmaps();
    return scratch;
}

std::shared_ptr<gl_engine::Texture> destination(
    gl_engine::Texture::Format format, unsigned resolution, unsigned layers, unsigned mip_levels)
{
    auto result = std::make_shared<gl_engine::Texture>(gl_engine::Texture::Target::_2dArray, format);
    result->setParams(mip_levels > 1 ? gl_engine::Texture::Filter::MipMapLinear : gl_engine::Texture::Filter::Linear,
        gl_engine::Texture::Filter::Linear);
    result->allocate_array(resolution, resolution, layers, mip_levels);
    return result;
}

QImage reconstruct_srgb(gl_engine::Texture& texture, unsigned resolution, unsigned layer, unsigned level = 0)
{
    const auto level_resolution = std::max(1u, resolution >> level);
    gl_engine::Framebuffer framebuffer(gl_engine::Framebuffer::DepthFormat::None,
        { gl_engine::Framebuffer::ColourFormat::RGBA8 },
        { level_resolution, level_resolution });
    framebuffer.bind();
    gl_engine::ShaderProgram shader(R"(
        out highp vec2 texcoords;
        void main() {
            highp vec2 vertices[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
            gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
            texcoords = 0.5 * gl_Position.xy + vec2(0.5);
        })",
        R"(
        uniform lowp sampler2DArray texture_sampler;
        uniform highp int texture_layer;
        uniform highp int mip_level;
        in highp vec2 texcoords;
        out lowp vec4 out_color;
        highp vec3 linear_to_srgb(highp vec3 linear) {
            return mix(12.92 * linear,
                1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055,
                step(vec3(0.0031308), linear));
        }
        void main() {
            lowp vec4 colour = textureLod(texture_sampler,
                vec3(texcoords.x, 1.0 - texcoords.y, float(texture_layer)), float(mip_level));
            out_color = vec4(linear_to_srgb(colour.rgb), colour.a);
        })",
        gl_engine::ShaderCodeSource::PLAINTEXT);
    shader.bind();
    texture.bind(0);
    shader.set_uniform("texture_sampler", 0);
    shader.set_uniform("texture_layer", int(layer));
    shader.set_uniform("mip_level", int(level));
    gl_engine::helpers::create_screen_quad_geometry().draw();
    const auto result = framebuffer.read_colour_attachment(0);
    gl_engine::Framebuffer::unbind();
    return result;
}

double psnr(const QImage& image, const Raster& source)
{
    double squared_error = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto actual = image.pixel(x, y);
            const auto expected = source.pixel({ x, y });
            const std::array<int, 3> delta {
                qRed(actual) - int(expected.x),
                qGreen(actual) - int(expected.y),
                qBlue(actual) - int(expected.z),
            };
            for (const auto value : delta)
                squared_error += double(value * value);
        }
    }
    const auto mse = squared_error / double(image.width() * image.height() * 3);
    return mse == 0.0 ? std::numeric_limits<double>::infinity() : 10.0 * std::log10(255.0 * 255.0 / mse);
}
}

TEST_CASE("GPU texture compression processes external scratch layers and mipmaps")
{
    constexpr unsigned resolution = 64;
    const auto mip_levels = Compressor::mip_level_count(resolution, resolution);
    const std::vector<Raster> sources {
        test_raster(resolution),
        Raster(glm::uvec2(resolution), glm::u8vec4(42, 142, 242, 255)),
    };
    auto scratch = rgba_scratch(sources, mip_levels);
    auto output = destination(gl_engine::Texture::Format::CompressedRGBA8, resolution, 3, mip_levels);
    Compressor compressor(scratch, output, { .search_effort = 4 });
    const std::array<unsigned, 2> layers { 2, 0 };

    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    while (f->glGetError() != GL_NO_ERROR) { }
    const auto result = compressor.compress(layers);
    REQUIRE(result);
    CHECK(f->glGetError() == GL_NO_ERROR);
    CHECK(result->layers_written == 2);
    CHECK(result->mip_levels_written == mip_levels);
    size_t expected_size = 0;
    for (unsigned level = 0; level < mip_levels; ++level) {
        expected_size += Compressor::compressed_level_size(
            std::max(1u, resolution >> level), std::max(1u, resolution >> level));
    }
    CHECK(result->bytes_written == expected_size * sources.size());
    CHECK(psnr(reconstruct_srgb(*output, resolution, 2), sources[0]) > 10.0);
    CHECK(psnr(reconstruct_srgb(*output, resolution, 0), sources[1]) > 20.0);
    for (unsigned level = 1; level < mip_levels; ++level)
        CHECK(psnr(reconstruct_srgb(*output, resolution, 0, level),
                  Raster(glm::uvec2(std::max(1u, resolution >> level)), glm::u8vec4(42, 142, 242, 255)))
            > 20.0);
}

TEST_CASE("GPU texture compression supports automatic and explicit readback modes")
{
    constexpr unsigned resolution = 16;
    const std::vector<Raster> sources { test_raster(resolution) };
    auto scratch = rgba_scratch(sources, 1);
    const std::array<unsigned, 1> layers { 0 };

    auto auto_output = destination(gl_engine::Texture::Format::CompressedRGBA8, resolution, 1, 1);
    Compressor automatic(scratch, auto_output, { .readback_mode = Compressor::ReadbackMode::Auto });
    REQUIRE(automatic.compress(layers));

    auto paired_output = destination(gl_engine::Texture::Format::CompressedRGBA8, resolution, 1, 1);
    Compressor paired(scratch, paired_output, { .readback_mode = Compressor::ReadbackMode::RGBA32UI });
    REQUIRE(paired.compress(layers));
    CHECK(paired.effective_readback_mode() == Compressor::ReadbackMode::RGBA32UI);
    CHECK(reconstruct_srgb(*auto_output, resolution, 0) == reconstruct_srgb(*paired_output, resolution, 0));

    auto direct_output = destination(gl_engine::Texture::Format::CompressedRGBA8, resolution, 1, 1);
    Compressor direct(scratch, direct_output, { .readback_mode = Compressor::ReadbackMode::RG32UI });
    const auto direct_result = direct.compress(layers);
    if (direct_result) {
        CHECK(direct.effective_readback_mode() == Compressor::ReadbackMode::RG32UI);
        CHECK(automatic.effective_readback_mode() == Compressor::ReadbackMode::RG32UI);
        CHECK(reconstruct_srgb(*auto_output, resolution, 0) == reconstruct_srgb(*direct_output, resolution, 0));
    } else {
        CHECK(direct_result.error().find("RG32UI") != std::string::npos);
        CHECK(automatic.effective_readback_mode() == Compressor::ReadbackMode::RGBA32UI);
    }
}

TEST_CASE("GPU texture compressor is reusable and preserves framebuffer bindings")
{
    constexpr unsigned resolution = 16;
    const std::vector<Raster> sources { test_raster(resolution) };
    auto scratch = rgba_scratch(sources, 1);
    auto output = destination(gl_engine::Texture::Format::CompressedRGBA8, resolution, 2, 1);

    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    std::array<GLint, 4> expected_viewport {};
    f->glGetIntegerv(GL_VIEWPORT, expected_viewport.data());
    Compressor compressor(scratch, output, { .readback_mode = Compressor::ReadbackMode::RGBA32UI });
    std::array<GLint, 4> actual_viewport {};
    f->glGetIntegerv(GL_VIEWPORT, actual_viewport.data());
    CHECK(actual_viewport == expected_viewport);

    const std::array<unsigned, 1> first_layer { 0 };
    const std::array<unsigned, 1> second_layer { 1 };

    gl_engine::Framebuffer draw_framebuffer(
        gl_engine::Framebuffer::DepthFormat::None, { gl_engine::Framebuffer::ColourFormat::RGBA8 });
    gl_engine::Framebuffer read_framebuffer(
        gl_engine::Framebuffer::DepthFormat::None, { gl_engine::Framebuffer::ColourFormat::RGBA8 });
    draw_framebuffer.bind_for_drawing();
    read_framebuffer.bind_for_reading();

    GLint expected_draw_framebuffer = 0;
    GLint expected_read_framebuffer = 0;
    f->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &expected_draw_framebuffer);
    f->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &expected_read_framebuffer);
    REQUIRE(expected_draw_framebuffer != expected_read_framebuffer);

    const auto first_result = compressor.compress(first_layer);
    REQUIRE(first_result);
    const auto second_result = compressor.compress(second_layer);
    REQUIRE(second_result);
    CHECK(second_result->bytes_written == first_result->bytes_written);
    CHECK(second_result->layers_written == 1);
    CHECK(second_result->mip_levels_written == 1);

    GLint actual_draw_framebuffer = 0;
    GLint actual_read_framebuffer = 0;
    f->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &actual_draw_framebuffer);
    f->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &actual_read_framebuffer);
    CHECK(actual_draw_framebuffer == expected_draw_framebuffer);
    CHECK(actual_read_framebuffer == expected_read_framebuffer);
    gl_engine::Framebuffer::unbind();
    CHECK(psnr(reconstruct_srgb(*output, resolution, 1), sources.front()) > 10.0);
}

TEST_CASE("GPU texture compressor exposes every platform algorithm")
{
    constexpr unsigned resolution = 8;
    const std::vector<Raster> sources { test_raster(resolution) };
    auto scratch = rgba_scratch(sources, 1);
    const std::array<unsigned, 1> layers { 0 };
    std::vector<Compressor::Settings> settings;
    if (gl_engine::Texture::compression_algorithm() == nucleus::utils::ColourTexture::Format::DXT1) {
        settings.push_back({ .dxt1_algorithm = Compressor::Dxt1Algorithm::SlowSearch });
        settings.push_back({ .dxt1_algorithm = Compressor::Dxt1Algorithm::DebugChecksum });
    } else {
        settings.push_back({ .etc_algorithm = Compressor::EtcAlgorithm::Fastest });
        settings.push_back({ .etc_algorithm = Compressor::EtcAlgorithm::Fast });
        settings.push_back({ .etc_algorithm = Compressor::EtcAlgorithm::SlowSearch });
        settings.push_back({ .etc_algorithm = Compressor::EtcAlgorithm::DebugChecksum });
    }

    for (const auto& setting : settings) {
        auto output = destination(gl_engine::Texture::Format::CompressedRGBA8, resolution, 1, 1);
        Compressor compressor(scratch, output, setting);
        CHECK(compressor.compress(layers));
    }
}

TEST_CASE("GPU texture compressor copies sRGB bytes through an RGBA8 framebuffer")
{
    constexpr unsigned resolution = 16;
    const auto mip_levels = Compressor::mip_level_count(resolution, resolution);
    const std::vector<Raster> sources {
        test_raster(resolution),
        Raster(glm::uvec2(resolution), glm::u8vec4(23, 101, 207, 255)),
    };
    auto scratch = rgba_scratch(sources, mip_levels);
    auto output = destination(gl_engine::Texture::Format::SRGBA8, resolution, 3, mip_levels);
    Compressor compressor(scratch, output);
    const std::array<unsigned, 2> layers { 2, 0 };
    const auto result = compressor.compress(layers);
    REQUIRE(result);
    CHECK_FALSE(compressor.effective_readback_mode());
    size_t expected_size = 0;
    for (unsigned level = 0; level < mip_levels; ++level)
        expected_size += size_t(std::max(1u, resolution >> level)) * std::max(1u, resolution >> level) * 4 * sources.size();
    CHECK(result->bytes_written == expected_size);
    CHECK(psnr(reconstruct_srgb(*output, resolution, 2), sources[0]) > 45.0);
    CHECK(psnr(reconstruct_srgb(*output, resolution, 0), sources[1]) > 45.0);
}

TEST_CASE("GPU texture compressor accepts RGB565 scratch storage")
{
    constexpr unsigned resolution = 4;
    constexpr uint16_t packed = uint16_t((21u << 11u) | (37u << 5u) | 9u);
    auto scratch = std::make_shared<gl_engine::Texture>(
        gl_engine::Texture::Target::_2dArray, gl_engine::Texture::Format::RGB565);
    scratch->setParams(gl_engine::Texture::Filter::Nearest, gl_engine::Texture::Filter::Nearest);
    scratch->allocate_array(resolution, resolution, 1, 1);
    scratch->upload(radix::Raster<uint16_t>(glm::uvec2(resolution), packed), 0);
    auto output = destination(gl_engine::Texture::Format::SRGBA8, resolution, 1, 1);
    Compressor compressor(scratch, output);
    const std::array<unsigned, 1> layers { 0 };
    REQUIRE(compressor.compress(layers));

    const glm::u8vec4 expected(
        uint8_t(21u * 255u / 31u),
        uint8_t(37u * 255u / 63u),
        uint8_t(9u * 255u / 31u),
        255);
    CHECK(psnr(reconstruct_srgb(*output, resolution, 0), Raster(glm::uvec2(resolution), expected)) > 40.0);
}

TEST_CASE("GPU texture compressor reports expired textures")
{
    constexpr unsigned resolution = 4;
    const std::vector<Raster> sources { test_raster(resolution) };
    auto scratch = rgba_scratch(sources, 1);
    auto output = destination(gl_engine::Texture::Format::SRGBA8, resolution, 1, 1);
    Compressor compressor(scratch, output);
    scratch.reset();
    output.reset();
    const std::array<unsigned, 1> layers { 0 };
    const auto result = compressor.compress(layers);
    REQUIRE_FALSE(result);
    CHECK(result.error().find("expired") != std::string::npos);
}
