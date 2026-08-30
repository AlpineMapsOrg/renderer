/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek
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

#include "TextureCompressor.h"

#include "Framebuffer.h"
#include "ShaderProgram.h"
#include "Texture.h"
#include "helpers.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QtAssert>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr unsigned max_shader_mip_levels = 16;

std::optional<std::string> contract_error(bool condition, const char* message)
{
    if (condition)
        return std::nullopt;
    Q_ASSERT_X(false, "TextureCompressor", message);
    return std::string(message);
}

struct DrawState {
    GLint draw_framebuffer = 0;
    GLint viewport[4] = {};
    GLboolean colour_mask[4] = {};
    GLboolean blend_enabled = GL_FALSE;
    GLboolean cull_enabled = GL_FALSE;
    GLboolean depth_enabled = GL_FALSE;
    GLboolean scissor_enabled = GL_FALSE;

    explicit DrawState(QOpenGLExtraFunctions* f)
    {
        f->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);
        f->glGetIntegerv(GL_VIEWPORT, viewport);
        f->glGetBooleanv(GL_COLOR_WRITEMASK, colour_mask);
        blend_enabled = f->glIsEnabled(GL_BLEND);
        cull_enabled = f->glIsEnabled(GL_CULL_FACE);
        depth_enabled = f->glIsEnabled(GL_DEPTH_TEST);
        scissor_enabled = f->glIsEnabled(GL_SCISSOR_TEST);
    }

    void prepare(QOpenGLExtraFunctions* f) const
    {
        f->glDisable(GL_BLEND);
        f->glDisable(GL_CULL_FACE);
        f->glDisable(GL_DEPTH_TEST);
        f->glDisable(GL_SCISSOR_TEST);
        f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    void restore(QOpenGLExtraFunctions* f) const
    {
        if (blend_enabled)
            f->glEnable(GL_BLEND);
        else
            f->glDisable(GL_BLEND);
        if (cull_enabled)
            f->glEnable(GL_CULL_FACE);
        else
            f->glDisable(GL_CULL_FACE);
        if (depth_enabled)
            f->glEnable(GL_DEPTH_TEST);
        else
            f->glDisable(GL_DEPTH_TEST);
        if (scissor_enabled)
            f->glEnable(GL_SCISSOR_TEST);
        else
            f->glDisable(GL_SCISSOR_TEST);
        f->glColorMask(colour_mask[0], colour_mask[1], colour_mask[2], colour_mask[3]);
        f->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(draw_framebuffer));
    }
};
}

gl_engine::TextureCompressor::TextureCompressor(std::weak_ptr<Texture> scratch, std::weak_ptr<Texture> destination)
    : TextureCompressor(std::move(scratch), std::move(destination), Settings {})
{
}

gl_engine::TextureCompressor::TextureCompressor(std::weak_ptr<Texture> scratch,
    std::weak_ptr<Texture> destination,
    Settings settings)
    : m_scratch(std::move(scratch))
    , m_destination(std::move(destination))
    , m_settings(settings)
{
    m_initialisation_error = initialise();
}

gl_engine::TextureCompressor::~TextureCompressor()
{
    Q_ASSERT(QOpenGLContext::currentContext());
    m_program.reset();
    m_encoding_framebuffer.reset();
    m_copy_framebuffer.reset();
    m_screen_quad.reset();
    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glDeleteBuffers(1, &m_encoded_buffer);
}

std::optional<std::string> gl_engine::TextureCompressor::initialise()
{
    auto scratch = m_scratch.lock();
    auto destination = m_destination.lock();
    if (!scratch || !destination)
        return "Texture compressor input or destination expired during construction";
    if (auto error = validate_textures(*scratch, *destination))
        return error;
    if (auto error = contract_error(m_settings.search_effort <= 10, "Texture compression search effort must be at most 10"))
        return error;

    m_width = scratch->m_width;
    m_height = scratch->m_height;
    m_scratch_layers = scratch->m_n_layers;
    m_destination_layers = destination->m_n_layers;
    m_mip_levels = scratch->m_mip_levels;
    m_screen_quad = std::make_unique<helpers::ScreenQuadGeometry>(helpers::create_screen_quad_geometry());

    if (destination->m_format == Texture::Format::SRGBA8) {
        m_operation = Operation::Copy;
        m_copy_framebuffer = std::make_unique<Framebuffer>(
            Framebuffer::DepthFormat::None,
            std::vector { Framebuffer::ColourFormat::RGBA8 },
            glm::uvec2 { m_width, m_height });
        m_program = std::make_unique<ShaderProgram>("screen_pass.vert", "texture_copy.frag");
        auto* f = QOpenGLContext::currentContext()->extraFunctions();
        f->glGenBuffers(1, &m_encoded_buffer);
        f->glBindBuffer(GL_PIXEL_PACK_BUFFER, m_encoded_buffer);
        f->glBufferData(GL_PIXEL_PACK_BUFFER, GLsizeiptr(size_t(m_width) * m_height * 4), nullptr, GL_STREAM_DRAW);
        f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        return std::nullopt;
    }

    const auto format = Texture::compression_algorithm();
    m_operation = format == nucleus::utils::ColourTexture::Format::DXT1 ? Operation::Dxt1 : Operation::Etc;

    size_t maximum_size = 0;
    for (unsigned level = 0; level < m_mip_levels; ++level) {
        maximum_size += compressed_level_size(
            std::max(1u, m_width >> level), std::max(1u, m_height >> level));
    }
    maximum_size *= m_scratch_layers;

    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    GLint maximum_texture_size = 0;
    f->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum_texture_size);

    const auto create_output = [&](ReadbackMode mode) -> std::optional<std::string> {
        const auto bytes_per_pixel = mode == ReadbackMode::RG32UI ? size_t(8) : size_t(16);
        const auto pixels = (maximum_size + bytes_per_pixel - 1) / bytes_per_pixel;
        m_atlas_width = GLsizei(std::min({ pixels, size_t(maximum_texture_size), size_t(256) }));
        m_atlas_height = GLsizei((pixels + size_t(m_atlas_width) - 1) / size_t(m_atlas_width));
        if (m_atlas_width <= 0 || m_atlas_height <= 0 || m_atlas_height > maximum_texture_size)
            return "Texture compression output atlas exceeds the maximum texture size";

        const auto colour_format = mode == ReadbackMode::RG32UI
            ? Framebuffer::ColourFormat::RG32UI
            : Framebuffer::ColourFormat::RGBA32UI;
        auto candidate = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None,
            std::vector { colour_format },
            glm::uvec2 { unsigned(m_atlas_width), unsigned(m_atlas_height) });
        candidate->bind_for_reading();
        const auto framebuffer_status = f->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
        if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
            return mode == ReadbackMode::RG32UI
                ? "RG32UI texture compression framebuffer is incomplete"
                : "RGBA32UI texture compression framebuffer is incomplete";

        if (mode == ReadbackMode::RG32UI) {
            GLint implementation_read_format = 0;
            GLint implementation_read_type = 0;
            f->glReadBuffer(GL_COLOR_ATTACHMENT0);
            f->glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &implementation_read_format);
            f->glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &implementation_read_type);
            if (implementation_read_format != GL_RG_INTEGER || implementation_read_type != GL_UNSIGNED_INT)
                return "RG32UI framebuffer readback is unavailable";
        }
        m_encoding_framebuffer = std::move(candidate);
        return std::nullopt;
    };

    GLint previous_draw_framebuffer = 0;
    GLint previous_read_framebuffer = 0;
    GLint previous_texture = 0;
    f->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_framebuffer);
    f->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_framebuffer);
    f->glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);

    auto requested_mode = m_settings.readback_mode;
    auto selected_mode = requested_mode == ReadbackMode::RGBA32UI ? ReadbackMode::RGBA32UI : ReadbackMode::RG32UI;
    auto output_error = create_output(selected_mode);
    if (output_error && requested_mode == ReadbackMode::Auto) {
        selected_mode = ReadbackMode::RGBA32UI;
        output_error = create_output(selected_mode);
        if (!output_error)
            qInfo() << "RG32UI texture compression readback is unavailable; using RGBA32UI";
    }

    f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(previous_draw_framebuffer));
    f->glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(previous_read_framebuffer));
    f->glBindTexture(GL_TEXTURE_2D, GLuint(previous_texture));
    if (output_error)
        return output_error;
    m_effective_readback_mode = selected_mode;

    f->glGenBuffers(1, &m_encoded_buffer);
    f->glBindBuffer(GL_PIXEL_PACK_BUFFER, m_encoded_buffer);
    const auto bytes_per_pixel = selected_mode == ReadbackMode::RG32UI ? size_t(8) : size_t(16);
    f->glBufferData(GL_PIXEL_PACK_BUFFER,
        GLsizeiptr(size_t(m_atlas_width) * m_atlas_height * bytes_per_pixel),
        nullptr,
        GL_STREAM_DRAW);
    f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    std::vector<QString> defines;
    if (selected_mode == ReadbackMode::RGBA32UI)
        defines.push_back(QStringLiteral("#define ALP_COMPRESS_TWO_BLOCKS"));
    if ((m_operation == Operation::Dxt1 && m_settings.dxt1_algorithm == Dxt1Algorithm::DebugChecksum)
        || (m_operation == Operation::Etc && m_settings.etc_algorithm == EtcAlgorithm::DebugChecksum)) {
        defines.push_back(QStringLiteral("#define ALP_COMPRESS_CHECKSUM"));
    } else if (m_operation == Operation::Etc) {
        defines.push_back(QStringLiteral("#define ALP_COMPRESS_ETC1"));
        if (m_settings.etc_algorithm == EtcAlgorithm::Fastest || m_settings.etc_algorithm == EtcAlgorithm::Fast)
            defines.push_back(QStringLiteral("#define ALP_COMPRESS_ETC1_SPLIT_FUSED"));
        if (m_settings.etc_algorithm == EtcAlgorithm::Fast)
            defines.push_back(QStringLiteral("#define ALP_COMPRESS_ETC1_REFINE_RESIDUAL"));
    }
    m_program = std::make_unique<ShaderProgram>(
        "screen_pass.vert", "texture_compress.frag", ShaderCodeSource::FILE, defines);
    return std::nullopt;
}

std::optional<std::string> gl_engine::TextureCompressor::validate_textures(
    const Texture& scratch, const Texture& destination) const
{
    if (auto error = contract_error(scratch.m_target == Texture::Target::_2dArray,
            "Texture compressor scratch must be a 2D texture array"))
        return error;
    if (auto error = contract_error(destination.m_target == Texture::Target::_2dArray,
            "Texture compressor destination must be a 2D texture array"))
        return error;
    if (auto error = contract_error(scratch.m_format == Texture::Format::RGBA8 || scratch.m_format == Texture::Format::RGB565,
            "Texture compressor scratch must use non-sRGB RGBA8 or RGB565 storage"))
        return error;
    if (auto error = contract_error(destination.m_format == Texture::Format::CompressedRGBA8
                || destination.m_format == Texture::Format::SRGBA8,
            "Texture compressor destination must use compressed sRGB or SRGBA8 storage"))
        return error;
    if (auto error = contract_error(scratch.m_width == destination.m_width && scratch.m_height == destination.m_height,
            "Texture compressor scratch and destination sizes must agree"))
        return error;
    if (auto error = contract_error(scratch.m_mip_levels == destination.m_mip_levels,
            "Texture compressor scratch and destination mip counts must agree"))
        return error;
    if (auto error = contract_error(scratch.m_width > 0 && scratch.m_height > 0 && scratch.m_n_layers > 0
                && destination.m_n_layers > 0 && scratch.m_mip_levels > 0,
            "Texture compressor textures must have allocated storage"))
        return error;
    if (auto error = contract_error(scratch.m_mip_levels <= max_shader_mip_levels,
            "Texture compressor supports at most 16 mip levels"))
        return error;
    return std::nullopt;
}

std::expected<gl_engine::TextureCompressor::Result, std::string> gl_engine::TextureCompressor::compress(
    std::span<const unsigned> destination_layers)
{
    if (m_initialisation_error)
        return std::unexpected(*m_initialisation_error);
    auto scratch = m_scratch.lock();
    auto destination = m_destination.lock();
    if (!scratch || !destination)
        return std::unexpected("Texture compressor input or destination has expired");
    if (auto error = validate_textures(*scratch, *destination))
        return std::unexpected(*error);
    if (scratch->m_width != m_width || scratch->m_height != m_height || scratch->m_n_layers != m_scratch_layers
        || scratch->m_mip_levels != m_mip_levels || destination->m_n_layers != m_destination_layers) {
        Q_ASSERT_X(false, "TextureCompressor", "Texture storage changed after compressor construction");
        return std::unexpected("Texture storage changed after compressor construction");
    }
    if (auto error = contract_error(!destination_layers.empty(), "Texture compressor requires at least one layer"))
        return std::unexpected(*error);
    if (auto error = contract_error(destination_layers.size() <= m_scratch_layers,
            "Texture compressor batch exceeds the scratch layer count"))
        return std::unexpected(*error);
    for (const auto layer : destination_layers) {
        if (auto error = contract_error(layer < m_destination_layers,
                "Texture compressor destination layer is out of range"))
            return std::unexpected(*error);
    }

    if (m_operation == Operation::Copy)
        return copy_srgb(*scratch, *destination, destination_layers);
    return compress_blocks(*scratch, *destination, destination_layers);
}

std::expected<gl_engine::TextureCompressor::Result, std::string> gl_engine::TextureCompressor::compress_blocks(
    const Texture& scratch, Texture& destination, std::span<const unsigned> destination_layers)
{
    Result result {
        .bytes_written = 0,
        .layers_written = unsigned(destination_layers.size()),
        .mip_levels_written = m_mip_levels,
    };
    std::vector<size_t> level_offsets;
    std::vector<int> level_offsets_blocks;
    std::vector<int> level_blocks_x;
    std::vector<int> level_blocks_y;
    level_offsets.reserve(m_mip_levels);
    level_offsets_blocks.reserve(m_mip_levels);
    level_blocks_x.reserve(m_mip_levels);
    level_blocks_y.reserve(m_mip_levels);
    for (unsigned level = 0; level < m_mip_levels; ++level) {
        level_offsets.push_back(result.bytes_written);
        const auto level_width = std::max(1u, m_width >> level);
        const auto level_height = std::max(1u, m_height >> level);
        level_offsets_blocks.push_back(int(result.bytes_written / 8));
        level_blocks_x.push_back(int(std::max(1u, (level_width + 3) / 4)));
        level_blocks_y.push_back(int(std::max(1u, (level_height + 3) / 4)));
        result.bytes_written += compressed_level_size(level_width, level_height) * destination_layers.size();
    }

    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    const DrawState draw_state(f);
    draw_state.prepare(f);
    const auto paired_blocks = *m_effective_readback_mode == ReadbackMode::RGBA32UI;
    const auto total_blocks = result.bytes_written / 8;
    const auto encoding_pixels = paired_blocks ? (total_blocks + 1) / 2 : total_blocks;
    const auto encoding_width = GLsizei(std::min(encoding_pixels, size_t(m_atlas_width)));
    const auto encoding_height = GLsizei((encoding_pixels + size_t(encoding_width) - 1) / size_t(encoding_width));

    m_encoding_framebuffer->bind_for_drawing();
    f->glViewport(0, 0, encoding_width, encoding_height);
    m_program->bind();
    m_program->set_uniform("source_texture", 7);
    m_program->set_uniform("texture_width", int(m_width));
    m_program->set_uniform("texture_height", int(m_height));
    m_program->set_uniform("effort", int(m_settings.search_effort));
    m_program->set_uniform("atlas_width", int(encoding_width));
    m_program->set_uniform("total_blocks", int(total_blocks));
    m_program->set_uniform("mip_levels", int(m_mip_levels));
    m_program->set_uniform_array("level_offsets", level_offsets_blocks);
    m_program->set_uniform_array("level_blocks_x", level_blocks_x);
    m_program->set_uniform_array("level_blocks_y", level_blocks_y);
    f->glActiveTexture(GL_TEXTURE7);
    f->glBindTexture(GL_TEXTURE_2D_ARRAY, scratch.m_id);
    m_screen_quad->draw();
    m_program->release();
    draw_state.restore(f);

    GLint previous_read_framebuffer = 0;
    GLint previous_read_buffer = 0;
    GLint previous_pack_alignment = 0;
    f->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_framebuffer);
    f->glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
    f->glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);
    m_encoding_framebuffer->bind_for_reading();
    f->glReadBuffer(GL_COLOR_ATTACHMENT0);
    f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    f->glBindBuffer(GL_PIXEL_PACK_BUFFER, m_encoded_buffer);
    f->glReadPixels(0,
        0,
        encoding_width,
        encoding_height,
        paired_blocks ? GL_RGBA_INTEGER : GL_RG_INTEGER,
        GL_UNSIGNED_INT,
        nullptr);
    f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    f->glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);
    f->glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(previous_read_framebuffer));
    f->glReadBuffer(GLenum(previous_read_buffer));

    f->glBindTexture(GL_TEXTURE_2D_ARRAY, destination.m_id);
    f->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_encoded_buffer);
    const auto format = Texture::compressed_texture_format();
    for (unsigned level = 0; level < m_mip_levels; ++level) {
        const auto level_width = std::max(1u, m_width >> level);
        const auto level_height = std::max(1u, m_height >> level);
        const auto layer_size = compressed_level_size(level_width, level_height);
        for (size_t layer = 0; layer < destination_layers.size(); ++layer) {
            const auto offset = level_offsets[level] + layer_size * layer;
            f->glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                GLint(level),
                0,
                0,
                GLint(destination_layers[layer]),
                GLsizei(level_width),
                GLsizei(level_height),
                1,
                format,
                GLsizei(layer_size),
                reinterpret_cast<const void*>(quintptr(offset)));
        }
    }
    f->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    f->glActiveTexture(GL_TEXTURE0);
    return result;
}

std::expected<gl_engine::TextureCompressor::Result, std::string> gl_engine::TextureCompressor::copy_srgb(
    const Texture& scratch, Texture& destination, std::span<const unsigned> destination_layers)
{
    Result result {
        .bytes_written = 0,
        .layers_written = unsigned(destination_layers.size()),
        .mip_levels_written = m_mip_levels,
    };
    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    const DrawState draw_state(f);
    GLint previous_read_framebuffer = 0;
    GLint previous_read_buffer = 0;
    f->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_framebuffer);
    f->glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
    draw_state.prepare(f);
    m_copy_framebuffer->bind();
    m_program->bind();
    m_program->set_uniform("source_texture", 7);
    f->glActiveTexture(GL_TEXTURE7);
    f->glBindTexture(GL_TEXTURE_2D_ARRAY, scratch.m_id);

    GLint previous_pack_alignment = 0;
    GLint previous_unpack_alignment = 0;
    f->glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);
    f->glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
    f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned level = 0; level < m_mip_levels; ++level) {
        const auto level_width = std::max(1u, m_width >> level);
        const auto level_height = std::max(1u, m_height >> level);
        const auto layer_size = size_t(level_width) * level_height * 4;
        f->glViewport(0, 0, GLsizei(level_width), GLsizei(level_height));
        m_program->set_uniform("source_level", int(level));
        for (size_t layer = 0; layer < destination_layers.size(); ++layer) {
            m_program->set_uniform("source_layer", int(layer));
            m_screen_quad->draw();

            f->glReadBuffer(GL_COLOR_ATTACHMENT0);
            f->glBindBuffer(GL_PIXEL_PACK_BUFFER, m_encoded_buffer);
            f->glReadPixels(0,
                0,
                GLsizei(level_width),
                GLsizei(level_height),
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                nullptr);
            f->glActiveTexture(GL_TEXTURE0);
            f->glBindTexture(GL_TEXTURE_2D_ARRAY, destination.m_id);
            f->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_encoded_buffer);
            f->glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                GLint(level),
                0,
                0,
                GLint(destination_layers[layer]),
                GLsizei(level_width),
                GLsizei(level_height),
                1,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                nullptr);
            f->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            f->glActiveTexture(GL_TEXTURE7);
            result.bytes_written += layer_size;
        }
    }
    f->glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
    m_program->release();
    draw_state.restore(f);
    f->glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(previous_read_framebuffer));
    f->glReadBuffer(GLenum(previous_read_buffer));
    f->glActiveTexture(GL_TEXTURE0);
    return result;
}

size_t gl_engine::TextureCompressor::compressed_level_size(unsigned width, unsigned height)
{
    return size_t(std::max(1u, (width + 3) / 4)) * std::max(1u, (height + 3) / 4) * 8;
}

unsigned gl_engine::TextureCompressor::mip_level_count(unsigned width, unsigned height)
{
    Q_ASSERT(width > 0 && height > 0);
    return 1u + unsigned(std::floor(std::log2(std::max(width, height))));
}

std::optional<gl_engine::TextureCompressor::ReadbackMode> gl_engine::TextureCompressor::effective_readback_mode() const
{
    return m_effective_readback_mode;
}
