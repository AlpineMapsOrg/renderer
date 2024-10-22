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
#include <QString>
#include <tl/expected.hpp>
#include <utility>

namespace nucleus::utils::error {
template <typename ErrorType = QString, typename F> auto wrap_to_expected(F&& func)
{
    return [func = std::forward<F>(func)](auto&& value) {
        using ReturnType = decltype(func(std::forward<decltype(value)>(value)));
        return tl::expected<ReturnType, ErrorType> { func(std::forward<decltype(value)>(value)) };
    };
}

} // namespace nucleus::utils::error
