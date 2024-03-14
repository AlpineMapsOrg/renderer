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
#pragma once

#include <map>
#include <string>
#include <filesystem>
#include "webgpu.hpp"

namespace webgpu_engine {

class ShaderModuleManager {
public:
    const static inline std::filesystem::path DEFAULT_PREFIX = ":/wgsl_shaders/";

public:
    ShaderModuleManager(wgpu::Device& device, const std::filesystem::path& prefix = DEFAULT_PREFIX);
    ~ShaderModuleManager() = default;

    void create_shader_modules();
    void release_shader_modules();

    wgpu::ShaderModule debug_triangle() const;
    wgpu::ShaderModule debug_config_and_camera() const;

private:
    std::string read_file_contents(const std::string& name) const;
    std::string get_contents(const std::string& name);
    std::string preprocess(const std::string& code);
    wgpu::ShaderModule create_shader_module(const std::string& code);

private:
    wgpu::Device* m_device;
    std::filesystem::path m_prefix;

    std::map<std::string, std::string> m_shader_name_to_code;

    wgpu::ShaderModule m_debug_triangle_shader_module = nullptr;
    wgpu::ShaderModule m_debug_config_and_camera_shader_module = nullptr;
};

}
