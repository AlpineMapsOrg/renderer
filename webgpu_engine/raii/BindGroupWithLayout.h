/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "base_types.h"
#include <memory>
#include <webgpu/webgpu.h>

namespace webgpu_engine::util {
class BindGroupWithLayoutInfo;
}

namespace webgpu_engine::raii {

class BindGroupWithLayout {
public:
    BindGroupWithLayout(WGPUDevice device, const util::BindGroupWithLayoutInfo& bind_group_with_layout);

    void bind(WGPURenderPassEncoder& render_pass, uint32_t group_index) const;

    const raii::BindGroupLayout& bind_group_layout() const;
    const raii::BindGroup& bind_group() const;

private:
    std::unique_ptr<raii::BindGroupLayout> m_bind_group_layout;
    std::unique_ptr<raii::BindGroup> m_bind_group;
};

} // namespace webgpu_engine::raii
