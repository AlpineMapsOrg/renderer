/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <cassert>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace nucleus {

template <typename T>
class Raster {
    std::vector<T> m_data;
    size_t m_width = 0;
    size_t m_height = 0;

public:
    Raster() = default;
    Raster(size_t square_side_length, std::vector<T>&& vector)
        : m_data(std::move(vector))
        , m_width(square_side_length)
        , m_height(square_side_length)
    {
        assert(m_data.size() == m_width * m_height);
    }
    Raster(size_t square_side_length)
        : m_data(square_side_length * square_side_length)
        , m_width(square_side_length)
        , m_height(square_side_length)
    {
    }
    Raster(const glm::uvec2& size)
        : m_data(size.x * size.y)
        , m_width(size.x)
        , m_height(size.y)
    {
    }
    Raster(const glm::uvec2& size, const T& fill_value)
        : m_data(size.x * size.y, fill_value)
        , m_width(size.x)
        , m_height(size.y)
    {
    }

    [[nodiscard]] const auto& buffer() const { return m_data; }
    [[nodiscard]] auto& buffer() { return m_data; }
    [[nodiscard]] size_t width() const { return m_width; }
    [[nodiscard]] size_t height() const { return m_height; }
    [[nodiscard]] glm::uvec2 size() const { return { m_width, m_height }; }
    [[nodiscard]] size_t buffer_length() const { return m_data.size(); }
    [[nodiscard]] const T& pixel(const glm::uvec2& position) const { return m_data[position.x + m_width * position.y]; }
    [[nodiscard]] T& pixel(const glm::uvec2& position) { return m_data[position.x + m_width * position.y]; }

    [[nodiscard]] const uint8_t& byte(const uintptr_t& index) const
    {
        assert(index < m_data.size() * sizeof(T));
        return *(reinterpret_cast<const uint8_t*>(m_data.data()) + index);
    }
    [[nodiscard]] uint8_t& byte(const uintptr_t& index)
    {
        assert(index < m_data.size() * sizeof(T));
        return *(reinterpret_cast<uint8_t*>(m_data.data()) + index);
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
}
