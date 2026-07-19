/*****************************************************************************
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

#ifndef DEFER_CONCAT_INTERNAL
#define DEFER_CONCAT_INTERNAL(x, y) x##y
#endif
#ifndef DEFER_CONCAT
#define DEFER_CONCAT(x, y) DEFER_CONCAT_INTERNAL(x, y)
#endif

#if defined(_MSC_VER)
#define DEFER_FORCEINLINE __forceinline
#else
#define DEFER_FORCEINLINE inline __attribute__((always_inline))
#endif

namespace InternalDefer {
template <typename F>
struct Deferrer : F {
  DEFER_FORCEINLINE constexpr Deferrer(F&& f) : F(static_cast<F&&>(f)) {}
  DEFER_FORCEINLINE constexpr ~Deferrer() { F::operator()(); }
};
}

#define defer [[maybe_unused]] const ::InternalDefer::Deferrer DEFER_CONCAT(_defer_, __COUNTER__) = [&](void) -> void
