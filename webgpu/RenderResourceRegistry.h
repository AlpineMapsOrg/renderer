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

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <webgpu/raii/BindGroupLayout.h>
#include <webgpu/raii/base_types.h>
#include <webgpu/util/ShaderPreprocessor.h>
#include <webgpu/webgpu.h>

namespace webgpu {

class RenderResourceRegistry {
public:
    RenderResourceRegistry();

    // Set local filesystem path prefix for shader hot-reload
    void set_local_shader_path(const std::string& path);

    // Register a shader by name and source path.
    void register_shader(const std::string& name, const std::string& source_path);
    [[nodiscard]] bool has_shader(const std::string& name) const;
    const raii::ShaderModule& shader(const std::string& name) const;

    // Register a bind group layout by name and factory.
    void register_bind_group_layout(const std::string& name, std::function<std::unique_ptr<raii::BindGroupLayout>(WGPUDevice)> factory);
    [[nodiscard]] bool has_bind_group_layout(const std::string& name) const;
    const raii::BindGroupLayout& bind_group_layout(const std::string& name) const;

    // Register a pipeline constructor
    // NOTE: Since we have multiple types of Pipelines its easier that the caller owns the pipeline object
    void register_pipeline(std::function<void(WGPUDevice, const RenderResourceRegistry&)> recreate_fn);

    // Recreate order: shaders -> layouts -> pipelines
    void recreate_all(WGPUDevice device);

    // Compile inline WGSL code (with #include preprocessing) without registering it.
    std::unique_ptr<raii::ShaderModule> compile_shader_from_code(WGPUDevice device, const std::string& code, const std::string& label);

private:
    std::string read_shader_source(const std::string& source_path) const;
    std::unique_ptr<raii::ShaderModule> compile_shader(WGPUDevice device, const std::string& source_path);

private:
    static constexpr const char* QRC_PREFIX = ":/wgsl_shaders/";
    std::string m_local_shader_path;

    webgpu::util::ShaderPreprocessor m_preprocessor;

    struct ShaderEntry {
        std::string source_path;
        std::unique_ptr<raii::ShaderModule> module;
    };
    struct LayoutEntry {
        std::string name;
        std::function<std::unique_ptr<raii::BindGroupLayout>(WGPUDevice)> factory;
        std::unique_ptr<raii::BindGroupLayout> layout;
    };

    std::unordered_map<std::string, size_t> m_shader_index;
    std::vector<ShaderEntry> m_shaders;

    std::unordered_map<std::string, size_t> m_layout_index;
    std::vector<LayoutEntry> m_layouts;

    std::vector<std::function<void(WGPUDevice, const RenderResourceRegistry&)>> m_pipeline_fns;

    // Set by recreate_all(); non-null means resources can be created on demand.
    WGPUDevice m_device = nullptr;
};

} // namespace webgpu
