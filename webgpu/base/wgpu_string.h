/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Matthias Huerbe
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

#include <webgpu/webgpu.h>
#include <cstring> // std::strlen & std::memcmp
#include <string_view>

namespace webgpu
{

// utility to create a webgpu string view from the string literal
inline constexpr WGPUStringView operator""_wsv(const char* s, size_t l) noexcept
{
    return { s, l };
}

// utility to create a webgpu string view from std::string, std::string_view, const char*
inline constexpr WGPUStringView wsv(std::string_view s) noexcept
{
    return { s.data(), s.size() };
}

// length of a possibly WGPU_STRLEN-sentinel string view
inline constexpr size_t wsv_length(WGPUStringView s) noexcept
{
    return (s.length == WGPU_STRLEN) ? (s.data ? std::strlen(s.data) : 0) : s.length;
}

// content equality of two string views
inline constexpr bool operator==(WGPUStringView a, WGPUStringView b) noexcept
{
    size_t na = wsv_length(a), nb = wsv_length(b);
    return na == nb && (na == 0 || std::memcmp(a.data, b.data, na) == 0);
}

} // namespace webgpu

// usable by include
using webgpu::operator""_wsv;
using webgpu::operator==;
