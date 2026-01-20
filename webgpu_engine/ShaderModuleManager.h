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

#include <filesystem>
#include <map>
#include <string>
#include <webgpu/raii/base_types.h>
#include <webgpu/util/ShaderPreprocessor.h>
#include <webgpu/webgpu.h>

namespace webgpu_engine {

class ShaderModuleManager {
public:
    const static inline std::filesystem::path QRC_PREFIX = ":/wgsl_shaders/";
    const static inline std::filesystem::path LOCAL_PREFIX = ALP_RESOURCES_PREFIX;

public:
    ShaderModuleManager(WGPUDevice device);
    ~ShaderModuleManager() = default;

    void create_shader_modules();
    void release_shader_modules();

    const webgpu::raii::ShaderModule& render_tiles() const;
    const webgpu::raii::ShaderModule& render_atmosphere() const;
    const webgpu::raii::ShaderModule& render_lines() const;
    const webgpu::raii::ShaderModule& compose_pass() const;

    const webgpu::raii::ShaderModule& normals_compute() const;
    const webgpu::raii::ShaderModule& snow_compute() const;
    const webgpu::raii::ShaderModule& downsample_compute() const;
    const webgpu::raii::ShaderModule& upsample_textures_compute() const;
    const webgpu::raii::ShaderModule& avalanche_trajectories_compute() const;
    const webgpu::raii::ShaderModule& buffer_to_texture_compute() const;
    const webgpu::raii::ShaderModule& avalanche_influence_area_compute() const;
    const webgpu::raii::ShaderModule& d8_compute() const;
    const webgpu::raii::ShaderModule& release_point_compute() const;
    const webgpu::raii::ShaderModule& height_decode_compute() const;
    const webgpu::raii::ShaderModule& fxaa_compute() const;
    const webgpu::raii::ShaderModule& iterative_simulation_compute() const;

    const webgpu::raii::ShaderModule& mipmap_creation_compute() const;

    static std::unique_ptr<webgpu::raii::ShaderModule> create_shader_module(WGPUDevice device, const std::string& label, const std::string& code);

    std::unique_ptr<webgpu::raii::ShaderModule> create_shader_module_for_file(const std::string& name);
    std::unique_ptr<webgpu::raii::ShaderModule> create_shader_module_for_code(const std::string& code, const std::string& label = "inline_shader");

private:
    static std::string read_file_contents(const std::string& name);

private:
    WGPUDevice m_device;
    webgpu::util::ShaderPreprocessor m_preprocessor;

    std::unique_ptr<webgpu::raii::ShaderModule> m_render_tiles_shader_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_render_atmosphere_shader_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_render_lines_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_compose_pass_shader_module;

    std::unique_ptr<webgpu::raii::ShaderModule> m_normals_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_snow_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_downsample_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_upsample_textures_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_avalanche_trajectories_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_buffer_to_texture_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_avalanche_influence_area_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_d8_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_release_point_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_height_decode_compute_module;

    std::unique_ptr<webgpu::raii::ShaderModule> m_mipmap_creation_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_fxaa_compute_module;
    std::unique_ptr<webgpu::raii::ShaderModule> m_iterative_simulation_compute_module;
};

} // namespace webgpu_engine
