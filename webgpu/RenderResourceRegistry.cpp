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

#include "RenderResourceRegistry.h"

#include <QDebug>
#include <QFile>
#include <cassert>
#include <chrono>

namespace webgpu {

RenderResourceRegistry::RenderResourceRegistry()
{
    m_preprocessor.set_file_reader([this](const std::string& name) { return read_shader_source(name); });
    m_preprocessor.set_error_callback([](const std::string& message) { qFatal("%s", message.c_str()); });
}

void RenderResourceRegistry::set_local_shader_path(const std::string& path) { m_local_shader_path = path; }

void RenderResourceRegistry::register_shader(const std::string& name, const std::string& source_path)
{
    if (has_shader(name)) // treat re-registration as a no-op
        return;
    m_shader_index[name] = m_shaders.size();
    m_shaders.push_back({ source_path, nullptr });
    if (m_device != nullptr)
        m_shaders.back().module = compile_shader(m_device, source_path);
}

bool RenderResourceRegistry::has_shader(const std::string& name) const { return m_shader_index.find(name) != m_shader_index.end(); }

const raii::ShaderModule& RenderResourceRegistry::shader(const std::string& name) const
{
    auto it = m_shader_index.find(name);
    assert(it != m_shader_index.end());
    return *m_shaders[it->second].module;
}

void RenderResourceRegistry::register_bind_group_layout(const std::string& name, std::function<std::unique_ptr<raii::BindGroupLayout>(WGPUDevice)> factory)
{
    if (has_bind_group_layout(name)) // treat re-registration as a no-op
        return;
    m_layout_index[name] = m_layouts.size();
    m_layouts.push_back({ name, factory, nullptr });
    if (m_device != nullptr)
        m_layouts.back().layout = factory(m_device);
}

bool RenderResourceRegistry::has_bind_group_layout(const std::string& name) const { return m_layout_index.find(name) != m_layout_index.end(); }

const raii::BindGroupLayout& RenderResourceRegistry::bind_group_layout(const std::string& name) const
{
    auto it = m_layout_index.find(name);
    assert(it != m_layout_index.end());
    return *m_layouts[it->second].layout;
}

void RenderResourceRegistry::register_pipeline(std::function<void(WGPUDevice, const RenderResourceRegistry&)> recreate_fn)
{
    m_pipeline_fns.push_back(recreate_fn);
    if (m_device != nullptr)
        recreate_fn(m_device, *this);
}

void RenderResourceRegistry::recreate_all(WGPUDevice device)
{
    m_device = device;

    auto start = std::chrono::high_resolution_clock::now();

    m_preprocessor.clear_cache();

    for (auto& entry : m_shaders)
        entry.module = compile_shader(device, entry.source_path);

    for (auto& entry : m_layouts)
        entry.layout = entry.factory(device);

    for (auto& fn : m_pipeline_fns)
        fn(device, *this);

    auto end = std::chrono::high_resolution_clock::now();
    qDebug() << "RenderResourceRegistry::recreate_all took" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";
}

std::string RenderResourceRegistry::read_shader_source(const std::string& source_path) const
{
    if (!m_local_shader_path.empty()) {
        const std::string local = m_local_shader_path + source_path;
        QFile local_file(QString::fromStdString(local));
        if (local_file.open(QIODevice::ReadOnly | QIODevice::Text))
            return local_file.readAll().toStdString();
    }

    const std::string qrc = std::string(QRC_PREFIX) + source_path;
    QFile file(QString::fromStdString(qrc));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        qFatal("Could not open shader file %s", qrc.c_str());
    return file.readAll().toStdString();
}

std::unique_ptr<raii::ShaderModule> RenderResourceRegistry::compile_shader_from_code(WGPUDevice device, const std::string& code, const std::string& label)
{
    const std::string preprocessed = m_preprocessor.preprocess_code(code);

    WGPUShaderSourceWGSL wgsl_desc {};
    wgsl_desc.code = WGPUStringView { .data = preprocessed.c_str(), .length = WGPU_STRLEN };
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;

    WGPUShaderModuleDescriptor desc {};
    desc.label = WGPUStringView { .data = label.c_str(), .length = WGPU_STRLEN };
    desc.nextInChain = &wgsl_desc.chain;

    return std::make_unique<raii::ShaderModule>(device, desc);
}

std::unique_ptr<raii::ShaderModule> RenderResourceRegistry::compile_shader(WGPUDevice device, const std::string& source_path)
{
    const std::string code = m_preprocessor.preprocess_file(source_path);

    WGPUShaderSourceWGSL wgsl_desc {};
    wgsl_desc.code = WGPUStringView { .data = code.c_str(), .length = WGPU_STRLEN };
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;

    WGPUShaderModuleDescriptor desc {};
    desc.label = WGPUStringView { .data = source_path.c_str(), .length = WGPU_STRLEN };
    desc.nextInChain = &wgsl_desc.chain;

    return std::make_unique<raii::ShaderModule>(device, desc);
}

} // namespace webgpu
