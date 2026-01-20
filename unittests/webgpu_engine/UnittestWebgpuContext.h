/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "webgpu/webgpu.h"
#include "webgpu_engine/ShaderModuleManager.h"

using namespace webgpu_engine;

struct UnittestWebgpuContext {
    WGPULimits default_limits();

    UnittestWebgpuContext(bool use_default_limits = true, WGPULimits required_limits = {});

    WGPUInstanceDescriptor instance_desc;

    WGPUInstance instance = nullptr;
    WGPUSurface surface = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;

    std::unique_ptr<ShaderModuleManager> shader_module_manager = nullptr;
};
