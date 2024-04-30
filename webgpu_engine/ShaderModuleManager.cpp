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

#include "raii/base_types.h"
#include <QFile>
#include <iostream>
#include <memory>
#include <regex>

namespace webgpu_engine {

ShaderModuleManager::ShaderModuleManager(WGPUDevice device, const std::filesystem::path& prefix)
    : m_device(device)
    , m_prefix(prefix)
{}

void ShaderModuleManager::create_shader_modules()
{
    m_tile_shader_module = create_shader_module("Tile.wgsl");
    m_screen_pass_vert_shader_module = create_shader_module("screen_pass_vert.wgsl");
    m_compose_frag_shader_module = create_shader_module("compose_frag.wgsl");
    m_atmosphere_frag_shader_module = create_shader_module("atmosphere_frag.wgsl");
    m_dummy_compute_module = create_shader_module("dummy_compute.wgsl");
}

void ShaderModuleManager::release_shader_modules()
{
    m_tile_shader_module.release();
    m_screen_pass_vert_shader_module.release();
    m_compose_frag_shader_module.release();
    m_atmosphere_frag_shader_module.release();
    m_dummy_compute_module.release();
}

const raii::ShaderModule& ShaderModuleManager::tile() const { return *m_tile_shader_module; }

const raii::ShaderModule& ShaderModuleManager::screen_pass_vert() const { return *m_screen_pass_vert_shader_module; }

const raii::ShaderModule& ShaderModuleManager::compose_frag() const { return *m_compose_frag_shader_module; }

const raii::ShaderModule& ShaderModuleManager::atmosphere_frag() const { return *m_atmosphere_frag_shader_module; }

const raii::ShaderModule& ShaderModuleManager::dummy_compute() const { return *m_dummy_compute_module; }

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

std::unique_ptr<raii::ShaderModule> ShaderModuleManager::create_shader_module(const std::string& name)
{
    const std::string code = get_contents(name);
    WGPUShaderModuleDescriptor shader_module_desc {};
    WGPUShaderModuleWGSLDescriptor wgsl_desc {};
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType::WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.code = code.data();
    shader_module_desc.label = name.data();
    shader_module_desc.nextInChain = &wgsl_desc.chain;
    return std::make_unique<raii::ShaderModule>(m_device, shader_module_desc);
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

} // namespace webgpu_engine
