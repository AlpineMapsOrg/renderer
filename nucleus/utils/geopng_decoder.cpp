/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
 * Copyright (C) 2025 Patrick Komon
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

#include "geopng_decoder.h"

#include "image_writer.h"
#include <QFile>
#include <QString>
#include <QTextStream>
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>

namespace nucleus::utils::geopng {

std::vector<std::filesystem::path> possible_aabb_paths(const std::filesystem::path& image_path)
{
    const auto dir = image_path.parent_path();
    const std::string stem = image_path.stem().string();

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(dir / (stem + "_aabb.txt"));

    const size_t us = stem.find('_');
    if (us != std::string::npos) {
        auto track_candidate = dir / (stem.substr(0, us) + "_aabb.txt");
        if (track_candidate != candidates.front())
            candidates.push_back(std::move(track_candidate));
    }

    candidates.push_back(dir / "aabb.txt");
    return candidates;
}

tl::expected<radix::geometry::Aabb<2, double>, std::string> load_aabb_from_file(const std::filesystem::path& file_path)
{
    const std::string path_str = file_path.string();

    QFile aabb_file(QString::fromStdString(path_str));
    if (!aabb_file.open(QIODevice::ReadOnly)) {
        return tl::make_unexpected("Failed to open file " + path_str);
    }
    QTextStream file_contents(&aabb_file);

    std::array<float, 4> contents;
    bool float_conversion_ok = false;
    for (size_t i = 0; i < contents.size(); i++) {
        QString line = file_contents.readLine();
        contents[i] = line.toFloat(&float_conversion_ok);
        if (!float_conversion_ok) {
            return tl::make_unexpected("Failed to parse file " + path_str + ": Could not convert \"" + line.toStdString() + "\" to float");
        }
    }

    if (contents[0] >= contents[2]) {
        return tl::make_unexpected("Failed to parse file " + path_str + ": x_min (" + std::to_string(contents[0]) + ") must not be >= x_max (" + std::to_string(contents[2]) + ")");
    }

    if (contents[1] >= contents[3]) {
        return tl::make_unexpected("Failed to parse file " + path_str + ": y_min (" + std::to_string(contents[1]) + ") must not be >= y_max (" + std::to_string(contents[3]) + ")");
    }

    return radix::geometry::Aabb<2, double> { { contents[0], contents[1] }, { contents[2], contents[3] } };
}

void write_encoded_float_png(const Raster<float>& data, const QString& filename)
{
    constexpr float range = ENCODED_FLOAT_RANGE_MAX - ENCODED_FLOAT_RANGE_MIN;
    Raster<glm::u8vec4> out(glm::uvec2(data.width(), data.height()));
    for (size_t i = 0; i < data.buffer().size(); ++i) {
        const float clamped = std::clamp(data.buffer()[i], ENCODED_FLOAT_RANGE_MIN, ENCODED_FLOAT_RANGE_MAX);
        const uint32_t packed = static_cast<uint32_t>((clamped - ENCODED_FLOAT_RANGE_MIN) / range * static_cast<double>(std::numeric_limits<uint32_t>::max()));
        out.buffer()[i] = glm::u8vec4((packed >> 24) & 0xFF, (packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF);
    }
    image_writer::rgba8_as_png(out, filename);
}

glm::vec2 scan_encoded_float_range(const Raster<glm::u8vec4>& image, bool& likely_encoded_float)
{
    constexpr float range = ENCODED_FLOAT_RANGE_MAX - ENCODED_FLOAT_RANGE_MIN;
    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();
    size_t zero_count = 0;

    for (const glm::u8vec4& px : image) {
        const uint32_t packed = (uint32_t(px.x) << 24) | (uint32_t(px.y) << 16) | (uint32_t(px.z) << 8) | uint32_t(px.w);
        if (packed == 0)
            continue;
        const float value = ENCODED_FLOAT_RANGE_MIN + (float(packed) / float(std::numeric_limits<uint32_t>::max())) * range;
        if (std::abs(value) < 0.01f)
            ++zero_count;
        min_val = std::min(min_val, value);
        max_val = std::max(max_val, value);
    }

    const size_t total = image.buffer().size();
    const bool has_range = min_val <= max_val;
    const bool many_zeros = total > 0 && float(zero_count) / float(total) >= 0.01f;
    const bool same_sign = has_range && (min_val >= 0.0f || max_val <= 0.0f);
    likely_encoded_float = many_zeros || same_sign;

    return has_range ? glm::vec2(min_val, max_val) : glm::vec2(ENCODED_FLOAT_RANGE_MIN, ENCODED_FLOAT_RANGE_MAX);
}

} // namespace nucleus::utils::geopng
