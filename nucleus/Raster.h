/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2024 Lucas Dworschak
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
#include <glm/gtx/component_wise.hpp>

namespace nucleus {

template <typename T>
class Raster {
    std::vector<T> m_data;
    unsigned m_width = 0;
    unsigned m_height = 0;

public:
    Raster() = default;
    Raster(unsigned square_side_length, std::vector<T>&& vector)
        : m_data(std::move(vector))
        , m_width(square_side_length)
        , m_height(square_side_length)
    {
        assert(m_data.size() == m_width * m_height);
    }
    Raster(unsigned square_side_length)
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

    [[nodiscard]] const std::vector<T>& buffer() const { return m_data; }
    [[nodiscard]] std::vector<T>& buffer() { return m_data; }
    [[nodiscard]] unsigned width() const { return m_width; }
    [[nodiscard]] unsigned height() const { return m_height; }
    [[nodiscard]] glm::uvec2 size() const { return { m_width, m_height }; }
    [[nodiscard]] size_t size_in_bytes() const { return m_data.size() * sizeof(T); }
    [[nodiscard]] size_t size_per_line() const { return m_width * sizeof(T); }
    [[nodiscard]] size_t buffer_length() const { return m_data.size(); }
    [[nodiscard]] const T& pixel(const glm::uvec2& position) const
    {
        assert(position.x < m_width);
        assert(position.y < m_height);
        return m_data[position.x + m_width * position.y];
    }
    [[nodiscard]] T& pixel(const glm::uvec2& position)
    {
        assert(position.x < m_width);
        assert(position.y < m_height);
        return m_data[position.x + m_width * position.y];
    }
    [[nodiscard]] const uint8_t* bytes() const { return reinterpret_cast<const uint8_t*>(m_data.data()); }
    [[nodiscard]] uint8_t* bytes() { return reinterpret_cast<uint8_t*>(m_data.data()); }

    void fill(const T& value) { std::fill(begin(), end(), value); }

    void combine(const Raster<T>& other)
    {
        // currently only supports combining with other raster of equal width (otherwise we need to fill either the current or the other raster with 0 values)
        assert(other.width() == m_width);

        m_height += other.height();

        m_data.reserve(m_data.size() + (other.width() * other.height()));
        m_data.insert(end(), other.begin(), other.end());
    }

    auto begin() { return m_data.begin(); }
    auto end() { return m_data.end(); }
    auto begin() const { return m_data.begin(); }
    auto end() const { return m_data.end(); }
    auto cbegin() const { return m_data.cbegin(); }
    auto cend() const { return m_data.cend(); }

    const T* data() const { return m_data.data(); }
    T* data() { return m_data.data(); }
};

namespace detail {
    template <int n_dims, typename T>
    glm::vec<n_dims, T> avg(const glm::vec<n_dims, T>& a, const glm::vec<n_dims, T>& b, const glm::vec<n_dims, T>& c, const glm::vec<n_dims, T>& d)
    {
        glm::vec<n_dims, T> r;
        for (unsigned i = 0u; i < n_dims; ++i) {
            r[i] = (a[i] + b[i] + c[i] + d[i]) / 4;
        }
        return r;
    }
    template <typename T> T avg(const T& a, const T& b, const T& c, const T& d) { return (a + b + c + d) / 4; }
} // namespace detail

template <typename T> std::vector<Raster<T>> generate_mipmap(Raster<T> raster)
{
    assert(raster.width() == raster.height()); // this code is not tested for differing sizes
    assert(raster.width() > 1); // also not tested
    assert((raster.width() & (raster.width() - 1)) == 0); // only power of two tested
    auto resolution = raster.size();
    std::vector<Raster<T>> mipmap;
    {
        auto n = 0;
        for (auto x = glm::compMax(resolution); x != 0; x >>= 1)
            ++n;

        mipmap.reserve(n);
    }
    mipmap.push_back(std::move(raster));
    while (glm::compMax(resolution) > 1) {
        resolution = resolution / 2u;
        Raster<T>& u = mipmap.back();
        mipmap.push_back(Raster<T>(resolution));
        Raster<T>& r = mipmap.back();
        for (unsigned i = 0u; i < resolution.x; ++i) {
            for (unsigned j = 0u; j < resolution.y; ++j) {
                // clang-format off
                r.pixel({ i, j }) = detail::avg(u.pixel({ i * 2, j * 2 }),
                                                u.pixel({ i * 2, j * 2 + 1 }),
                                                u.pixel({ i * 2 + 1, j * 2 }),
                                                u.pixel({ i * 2 + 1, j * 2 + 1 }));
                // clang-format on
            }
        }
    }
    return mipmap;
}

template <typename T> Raster<T> resize(const Raster<T>& raster, const glm::uvec2& new_size, T fill)
{
    Raster<T> n = Raster<T>(new_size, fill);
    for (auto i = 0u; i < std::min(raster.width(), new_size.x); ++i) {
        for (auto j = 0u; j < std::min(raster.height(), new_size.y); ++j) {
            n.pixel({ i, j }) = raster.pixel({ i, j });
        }
    }
    return n;
}
}
