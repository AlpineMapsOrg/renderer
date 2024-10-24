/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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

#include <chrono>
#if !(defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION >= 15000))
#include <concepts>
#endif

namespace nucleus::utils {
#if defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION < 15000)
// #define STRING2(x) #x
// #define STRING(x) STRING2(x)
// #pragma message STRING(_LIBCPP_VERSION)

// the android stl used by qt doesn't know concepts (it's at version 11.000 at the time of writing)
template <class T, class U>
concept convertible_to = std::is_convertible_v<T, U>;

namespace detail {
    template <class T, class U>
    concept SameHelper = std::is_same_v<T, U>;
}

template <class T, class U>
concept same_as = detail::SameHelper<T, U> && detail::SameHelper<U, T>;

#else
template <class T, class U>
concept convertible_to = std::convertible_to<T, U>;

template <class T, class U>
concept same_as = std::same_as<T, U>;
#endif

inline uint64_t time_since_epoch() { return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()); }

} // namespace nucleus::utils
