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
#include <QDebug>
#include <memory>
#include <chrono>
#include <webgpu/raii/base_types.h>
#include <webgpu/util/string_cast.h>

namespace webgpu_engine {

ShaderModuleManager::ShaderModuleManager(WGPUDevice device)
    : m_device(device)
{
    // Set up the file reader for the preprocessor
    m_preprocessor.set_file_reader([](const std::string& name) {
        return ShaderModuleManager::read_file_contents(name);
    });

    // Set up error callback
    m_preprocessor.set_error_callback([](const std::string& message) {
        qFatal("%s", message.c_str());
    });
}

void ShaderModuleManager::create_shader_modules()
{
    auto start = std::chrono::high_resolution_clock::now();

    m_render_tiles_shader_module = create_shader_module_for_file("render_tiles.wgsl");
    m_render_atmosphere_shader_module = create_shader_module_for_file("render_atmosphere.wgsl");
    m_render_lines_module = create_shader_module_for_file("render_lines.wgsl");
    m_compose_pass_shader_module = create_shader_module_for_file("compose_pass.wgsl");

    m_normals_compute_module = create_shader_module_for_file("compute/normals_compute.wgsl");
    m_snow_compute_module = create_shader_module_for_file("compute/snow_compute.wgsl");
    m_downsample_compute_module = create_shader_module_for_file("compute/downsample_compute.wgsl");
    m_upsample_textures_compute_module = create_shader_module_for_file("compute/upsample_textures_compute.wgsl");
    m_avalanche_trajectories_compute_module = create_shader_module_for_file("compute/avalanche_trajectories_compute.wgsl");
    m_buffer_to_texture_compute_module = create_shader_module_for_file("compute/buffer_to_texture_compute.wgsl");
    m_avalanche_influence_area_compute_module = create_shader_module_for_file("compute/avalanche_influence_area_compute.wgsl");
    m_d8_compute_module = create_shader_module_for_file("compute/d8_compute.wgsl");
    m_release_point_compute_module = create_shader_module_for_file("compute/compute_release_points.wgsl");
    m_height_decode_compute_module = create_shader_module_for_file("compute/height_decode_compute.wgsl");

    m_mipmap_creation_compute_module = create_shader_module_for_file("compute/mipmap_creation_compute.wgsl");
    m_fxaa_compute_module = create_shader_module_for_file("compute/fxaa_compute.wgsl");
    m_iterative_simulation_compute_module = create_shader_module_for_file("compute/iterative_simulation_compute.wgsl");

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    qDebug() << "Shader module creation took" << duration.count() << "ms";
}

void ShaderModuleManager::release_shader_modules()
{
    m_preprocessor.clear_cache();

    m_render_tiles_shader_module.release();
    m_compose_pass_shader_module.release();
    m_render_atmosphere_shader_module.release();
    m_render_lines_module.release();

    m_normals_compute_module.release();
    m_snow_compute_module.release();
    m_downsample_compute_module.release();
    m_upsample_textures_compute_module.release();
    m_avalanche_trajectories_compute_module.release();
    m_buffer_to_texture_compute_module.release();
    m_avalanche_influence_area_compute_module.release();
    m_d8_compute_module.release();
    m_release_point_compute_module.release();
    m_height_decode_compute_module.release();

    m_mipmap_creation_compute_module.release();
    m_fxaa_compute_module.release();
    m_iterative_simulation_compute_module.release();
}

const webgpu::raii::ShaderModule& ShaderModuleManager::render_tiles() const { return *m_render_tiles_shader_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::render_atmosphere() const { return *m_render_atmosphere_shader_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::render_lines() const { return *m_render_lines_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::compose_pass() const { return *m_compose_pass_shader_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::normals_compute() const { return *m_normals_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::snow_compute() const { return *m_snow_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::downsample_compute() const { return *m_downsample_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::upsample_textures_compute() const { return *m_upsample_textures_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::avalanche_trajectories_compute() const { return *m_avalanche_trajectories_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::buffer_to_texture_compute() const { return *m_buffer_to_texture_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::avalanche_influence_area_compute() const { return *m_avalanche_influence_area_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::d8_compute() const { return *m_d8_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::release_point_compute() const { return *m_release_point_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::height_decode_compute() const { return *m_height_decode_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::mipmap_creation_compute() const { return *m_mipmap_creation_compute_module; }
const webgpu::raii::ShaderModule& ShaderModuleManager::fxaa_compute() const { return *m_fxaa_compute_module; }

const webgpu::raii::ShaderModule& ShaderModuleManager::iterative_simulation_compute() const { return *m_iterative_simulation_compute_module; }

std::unique_ptr<webgpu::raii::ShaderModule> ShaderModuleManager::create_shader_module(WGPUDevice device, const std::string& label, const std::string& code)
{
    WGPUShaderSourceWGSL wgsl_desc {};
    wgsl_desc.code = WGPUStringView { .data = code.c_str(), .length = WGPU_STRLEN };
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;

    WGPUShaderModuleDescriptor shader_module_desc {};
    shader_module_desc.label = WGPUStringView { .data = label.c_str(), .length = WGPU_STRLEN };
    shader_module_desc.nextInChain = &wgsl_desc.chain;

    return std::make_unique<webgpu::raii::ShaderModule>(device, shader_module_desc);
}

std::string ShaderModuleManager::read_file_contents(const std::string& name)
{
    static bool local_available = false;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;

#if defined(ALP_ENABLE_WGSL_MINIFICATION) || defined(__EMSCRIPTEN__)
        // If minification is enabled or building for Emscripten, always use embedded shaders
        local_available = false;
        qInfo("Using embedded shaders");
#else
        // For native builds, try local shaders first
        const auto test_path = LOCAL_PREFIX / name;
        QFile test_file(test_path);
        if (test_file.exists()) {
            local_available = true;
            qInfo("Using local shaders from: %s", LOCAL_PREFIX.string().c_str());
        } else {
            local_available = false;
            qInfo("Local shaders not found, using embedded shaders");
        }
#endif
    }

    const auto path = local_available ? (LOCAL_PREFIX / name) : (QRC_PREFIX / name);
    auto file = QFile(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qFatal("Could not open shader file %s", path.string().c_str());
    }
    auto contents = file.readAll();
    return contents.toStdString();
}

std::unique_ptr<webgpu::raii::ShaderModule> ShaderModuleManager::create_shader_module_for_file(const std::string& name)
{
    const std::string code = m_preprocessor.preprocess_file(name);
    return create_shader_module(m_device, name, code);
}

std::unique_ptr<webgpu::raii::ShaderModule> ShaderModuleManager::create_shader_module_for_code(const std::string& code, const std::string& label)
{
    const std::string preprocessed_code = m_preprocessor.preprocess_code(code);
    return create_shader_module(m_device, label, preprocessed_code);
}

} // namespace webgpu_engine
