/*****************************************************************************
 * Alpine Terrain Renderer
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

#include <QMetaObject>
#include <QObject>

namespace nucleus::utils::thread {

template <typename Function, typename = std::enable_if_t<std::is_void_v<std::invoke_result_t<Function>>>> void async_call(QObject* context, Function fun)
{
    QMetaObject::invokeMethod(context, fun, Qt::ConnectionType::QueuedConnection);
}

template <typename Function, typename = std::enable_if_t<std::is_void_v<std::invoke_result_t<Function>>>> void sync_call(QObject* context, Function fun)
{
    QMetaObject::invokeMethod(context, fun, Qt::ConnectionType::BlockingQueuedConnection);
}

template <typename Function, typename = std::enable_if_t<!std::is_void_v<std::invoke_result_t<Function>>>>
auto sync_call(QObject* context, Function fun) -> std::invoke_result_t<Function>
{
    using ReturnType = std::invoke_result_t<Function>;
    ReturnType retval = {};
    QMetaObject::invokeMethod(context, fun, Qt::ConnectionType::BlockingQueuedConnection, &retval);
    return retval;
}

} // namespace nucleus::utils::thread
