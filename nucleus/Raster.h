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
    Raster(std::vector<T>&& vector, size_t square_side_length)
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
    const auto& buffer() const { return m_data; }
    [[nodiscard]] size_t width() const { return m_width; }
    [[nodiscard]] size_t height() const { return m_height; }
    [[nodiscard]] size_t buffer_length() const { return m_data.size(); }
    [[nodiscard]] const T& pixel(const glm::uvec2& position) const { return m_data[position.x + m_width * position.y]; }
    [[nodiscard]] T& pixel(const glm::uvec2& position) { return m_data[position.x + m_width * position.y]; }

    auto begin() { return m_data.begin(); }
    auto end() { return m_data.end(); }
    auto begin() const { return m_data.begin(); }
    auto end() const { return m_data.end(); }
    auto cbegin() const { return m_data.cbegin(); }
    auto cend() const { return m_data.cend(); }
};
}
