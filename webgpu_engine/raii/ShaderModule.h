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

#include <string>
#include <webgpu/webgpu.h>

namespace webgpu_engine::raii {

class ShaderModule {
public:
    ShaderModule(WGPUDevice device, const std::string& name, const std::string& code);
    ~ShaderModule();

    // delete copy constructor and copy-assignment operator
    ShaderModule(const ShaderModule& other) = delete;
    ShaderModule& operator=(const ShaderModule& other) = delete;

    WGPUShaderModule handle() const;

private:
    std::string m_name;
    WGPUShaderModule m_shader_module;
};

} // namespace webgpu_engine::raii
