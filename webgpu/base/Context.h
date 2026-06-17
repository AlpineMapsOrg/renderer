/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "RenderResourceRegistry.h"
#include <webgpu/webgpu.h>

namespace webgpu {

class Context {
public:
    Context() = default;
    ~Context() = default;

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    WGPUInstance instance() const { return m_instance; }
    WGPUDevice device() const { return m_device; }
    WGPUAdapter adapter() const { return m_adapter; }
    WGPUSurface surface() const { return m_surface; }
    WGPUQueue queue() const { return m_queue; }

    void init(WGPUInstance instance, WGPUDevice device, WGPUAdapter adapter, WGPUSurface surface, WGPUQueue queue)
    {
        m_instance = instance;
        m_device = device;
        m_adapter = adapter;
        m_surface = surface;
        m_queue = queue;
    }

    RenderResourceRegistry& resource_registry() { return m_registry; }
    const RenderResourceRegistry& resource_registry() const { return m_registry; }

private:
    WGPUInstance m_instance = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUQueue m_queue = nullptr;

    RenderResourceRegistry m_registry;
};

} // namespace webgpu
