/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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
#include "ShaderModuleManager.h"

#include <QFile>
#include <iostream>
#include <regex>

namespace webgpu_engine {

ShaderModuleManager::ShaderModuleManager(WGPUDevice device, const std::filesystem::path& prefix)
    : m_device(device)
    , m_prefix(prefix)
{}

void ShaderModuleManager::create_shader_modules()
{
    m_debug_triangle_shader_module = create_shader_module(get_contents("DebugTriangle.wgsl"));
    m_debug_config_and_camera_shader_module = create_shader_module(get_contents("DebugConfigAndCamera.wgsl"));
}

void ShaderModuleManager::release_shader_modules()
{
    wgpuShaderModuleRelease(m_debug_triangle_shader_module);
    wgpuShaderModuleRelease(m_debug_config_and_camera_shader_module);
}

WGPUShaderModule ShaderModuleManager::debug_triangle() const { return m_debug_triangle_shader_module; }

WGPUShaderModule ShaderModuleManager::debug_config_and_camera() const { return m_debug_config_and_camera_shader_module; }

std::string ShaderModuleManager::read_file_contents(const std::string& name) const {
    const auto path = m_prefix / name; // operator/ concats paths
    auto file = QFile(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "could not open shader file " << path << std::endl;
        throw std::runtime_error("could not open file " + path.string());
    }
    return file.readAll().toStdString();
}

std::string ShaderModuleManager::get_contents(const std::string& name) {
    const auto found_it = m_shader_name_to_code.find(name);
    if (found_it != m_shader_name_to_code.end()) {
        return found_it->second;
    }
    const auto file_contents = read_file_contents(name);
    const auto preprocessed_contents = preprocess(file_contents);
    m_shader_name_to_code[name] = preprocessed_contents;
    return preprocessed_contents;
}

WGPUShaderModule ShaderModuleManager::create_shader_module(const std::string& code)
{
    WGPUShaderModuleDescriptor shaderModuleDesc {};
    WGPUShaderModuleWGSLDescriptor wgslDesc {};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = WGPUSType::WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = code.data();
    shaderModuleDesc.nextInChain = &wgslDesc.chain;
    return wgpuDeviceCreateShaderModule(m_device, &shaderModuleDesc);
}

std::string ShaderModuleManager::preprocess(const std::string& code)
{
    std::string preprocessed_code = code;
    const std::regex include_regex("#include \"([a-zA-Z0-9 ._-]+)\"");
    for (std::smatch include_match; std::regex_search(preprocessed_code, include_match, include_regex);) {
        const std::string included_filename = include_match[1].str(); // first submatch
        const std::string included_file_contents = get_contents(included_filename);
        preprocessed_code.replace(include_match.position(), include_match.length(), included_file_contents);
    }
    return preprocessed_code;
}

}