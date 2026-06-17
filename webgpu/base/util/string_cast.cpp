/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "util/string_cast.h"
#include <map>

namespace webgpu::util {

const char* bufferMapAsyncStatusToString(WGPUMapAsyncStatus status)
{
    static const std::map<WGPUMapAsyncStatus, const char*> statusToStringMap = {
        { WGPUMapAsyncStatus_Success, "Success" },
        { WGPUMapAsyncStatus_CallbackCancelled, "CallbackCancelled" },
        { WGPUMapAsyncStatus_Error, "Error" },
        { WGPUMapAsyncStatus_Aborted, "Aborted" },
        { WGPUMapAsyncStatus_Force32, "Force32" },
    };

    auto it = statusToStringMap.find(status);
    if (it != statusToStringMap.end()) {
        return it->second;
    }
    return "UnknownStatus";
}

} // namespace webgpu::util
