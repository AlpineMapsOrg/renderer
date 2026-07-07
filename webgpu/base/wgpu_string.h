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
