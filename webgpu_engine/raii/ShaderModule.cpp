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

#include "ShaderModule.h"

namespace webgpu_engine::raii {

ShaderModule::ShaderModule(WGPUDevice device, const std::string& name, const std::string& code)
    : m_name(name)
{
    WGPUShaderModuleDescriptor shader_module_desc {};
    WGPUShaderModuleWGSLDescriptor wgsl_desc {};
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType::WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.code = code.data();
    shader_module_desc.label = name.data();
    shader_module_desc.nextInChain = &wgsl_desc.chain;
    m_shader_module = wgpuDeviceCreateShaderModule(device, &shader_module_desc);
}

ShaderModule::~ShaderModule() { wgpuShaderModuleRelease(m_shader_module); }

WGPUShaderModule ShaderModule::handle() const { return m_shader_module; }

} // namespace webgpu_engine::raii
