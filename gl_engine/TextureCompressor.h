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

#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <qopengl.h>
#include <span>
#include <string>

namespace gl_engine {
class Framebuffer;
class ShaderProgram;
class Texture;
namespace helpers {
struct ScreenQuadGeometry;
}

class TextureCompressor {
public:
    enum class ReadbackMode {
        Auto,
        RG32UI,
        RGBA32UI,
    };

    enum class Dxt1Algorithm {
        SlowSearch,
        DebugChecksum,
    };

    enum class EtcAlgorithm {
        Fastest,
        Fast,
        SlowSearch,
        DebugChecksum,
    };

    struct Settings {
        ReadbackMode readback_mode = ReadbackMode::Auto;
        Dxt1Algorithm dxt1_algorithm = Dxt1Algorithm::SlowSearch;
        EtcAlgorithm etc_algorithm = EtcAlgorithm::Fast;
        unsigned search_effort = 0;
    };

    struct Result {
        size_t bytes_written = 0;
        unsigned layers_written = 0;
        unsigned mip_levels_written = 0;
    };

    TextureCompressor(std::weak_ptr<Texture> scratch,
        std::weak_ptr<Texture> destination);
    TextureCompressor(std::weak_ptr<Texture> scratch,
        std::weak_ptr<Texture> destination,
        Settings settings);
    TextureCompressor(const TextureCompressor&) = delete;
    TextureCompressor& operator=(const TextureCompressor&) = delete;
    TextureCompressor(TextureCompressor&&) = delete;
    TextureCompressor& operator=(TextureCompressor&&) = delete;
    ~TextureCompressor();

    [[nodiscard]] std::expected<Result, std::string> compress(std::span<const unsigned> destination_layers);

    [[nodiscard]] static size_t compressed_level_size(unsigned width, unsigned height);
    [[nodiscard]] static unsigned mip_level_count(unsigned width, unsigned height);
    [[nodiscard]] std::optional<ReadbackMode> effective_readback_mode() const;

private:
    enum class Operation {
        Dxt1,
        Etc,
        Copy,
    };

    [[nodiscard]] std::optional<std::string> initialise();
    [[nodiscard]] std::optional<std::string> validate_textures(const Texture& scratch, const Texture& destination) const;
    [[nodiscard]] std::expected<Result, std::string> compress_blocks(
        const Texture& scratch, Texture& destination, std::span<const unsigned> destination_layers);
    [[nodiscard]] std::expected<Result, std::string> copy_srgb(
        const Texture& scratch, Texture& destination, std::span<const unsigned> destination_layers);

    std::weak_ptr<Texture> m_scratch;
    std::weak_ptr<Texture> m_destination;
    Settings m_settings;
    Operation m_operation = Operation::Copy;
    std::optional<ReadbackMode> m_effective_readback_mode;
    std::optional<std::string> m_initialisation_error;

    unsigned m_width = 0;
    unsigned m_height = 0;
    unsigned m_scratch_layers = 0;
    unsigned m_destination_layers = 0;
    unsigned m_mip_levels = 0;
    GLsizei m_atlas_width = 0;
    GLsizei m_atlas_height = 0;
    GLuint m_encoded_buffer = 0;

    std::unique_ptr<ShaderProgram> m_program;
    std::unique_ptr<Framebuffer> m_encoding_framebuffer;
    std::unique_ptr<Framebuffer> m_copy_framebuffer;
    std::unique_ptr<helpers::ScreenQuadGeometry> m_screen_quad;
};

} // namespace gl_engine
