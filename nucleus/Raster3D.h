/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2026 Wendelin Muth
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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <vector>

namespace nucleus {
template <typename T>
class Raster3D {
    std::vector<T> m_data;
    unsigned m_width = 0;
    unsigned m_height = 0;
    unsigned m_depth = 0;

public:
    Raster3D() = default;
    Raster3D(unsigned square_side_length, unsigned depth, std::vector<T>&& vector)
        : m_data(std::move(vector))
        , m_width(square_side_length)
        , m_height(square_side_length)
        , m_depth(depth)
    {
        assert(m_data.size() == m_width * m_height * m_depth);
    }
    Raster3D(unsigned square_side_length, unsigned depth)
        : m_data(square_side_length * square_side_length * depth)
        , m_width(square_side_length)
        , m_height(square_side_length)
        , m_depth(depth)
    {
    }
    Raster3D(const glm::uvec3& size)
        : m_data(size.x * size.y * size.z)
        , m_width(size.x)
        , m_height(size.y)
        , m_depth(size.z)
    {
    }
    Raster3D(const glm::uvec3& size, const T& fill_value)
        : m_data(size.x * size.y * size.z, fill_value)
        , m_width(size.x)
        , m_height(size.y)
        , m_depth(size.z)
    {
    }

    [[nodiscard]] const std::vector<T>& buffer() const { return m_data; }
    [[nodiscard]] std::vector<T>& buffer() { return m_data; }
    [[nodiscard]] unsigned width() const { return m_width; }
    [[nodiscard]] unsigned height() const { return m_height; }
    [[nodiscard]] unsigned depth() const { return m_depth; }
    [[nodiscard]] glm::uvec3 size() const { return { m_width, m_height, m_depth }; }
    [[nodiscard]] size_t size_in_bytes() const { return m_data.size() * sizeof(T); }
    [[nodiscard]] size_t size_per_line() const { return m_width * sizeof(T); }
    [[nodiscard]] size_t size_per_slice() const { return m_width * m_height * sizeof(T); }
    [[nodiscard]] size_t buffer_length() const { return m_data.size(); }
    [[nodiscard]] const T& pixel(const glm::uvec3& position) const
    {
        assert(position.x < m_width);
        assert(position.y < m_height);
        assert(position.z < m_depth);
        return m_data[position.x + m_width * position.y + m_width * m_height * position.z];
    }
    [[nodiscard]] T& pixel(const glm::uvec3& position)
    {
        assert(position.x < m_width);
        assert(position.y < m_height);
        assert(position.z < m_depth);
        return m_data[position.x + m_width * position.y + m_width * m_height * position.z];
    }
    [[nodiscard]] const uint8_t* bytes() const { return reinterpret_cast<const uint8_t*>(m_data.data()); }
    [[nodiscard]] uint8_t* bytes() { return reinterpret_cast<uint8_t*>(m_data.data()); }

    void fill(const T& value) { std::fill(begin(), end(), value); }

    auto begin() { return m_data.begin(); }
    auto end() { return m_data.end(); }
    auto begin() const { return m_data.begin(); }
    auto end() const { return m_data.end(); }
    auto cbegin() const { return m_data.cbegin(); }
    auto cend() const { return m_data.cend(); }

    const T* data() const { return m_data.data(); }
    T* data() { return m_data.data(); }
};
} // namespace nucleus