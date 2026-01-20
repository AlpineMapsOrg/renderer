/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
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

#include "Node.h"

namespace webgpu_engine::compute::nodes {

class LoadTextureNode : public Node {
    Q_OBJECT

public:
    struct LoadTextureNodeSettings {
        // path to texture to load
        std::string file_path;

        // WebGPU texture parameters
        WGPUTextureFormat format = WGPUTextureFormat_RGBA8Uint;
        WGPUTextureUsage usage = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);
    };

    LoadTextureNode(WGPUDevice device);
    LoadTextureNode(WGPUDevice device, const LoadTextureNodeSettings& settings);

    void set_settings(const LoadTextureNodeSettings& settings);

public slots:
    void run_impl() override;

private:
    static std::unique_ptr<webgpu::raii::TextureWithSampler> create_texture(WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage);

private:
    WGPUDevice m_device;
    WGPUQueue m_queue;
    LoadTextureNodeSettings m_settings;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
};

} // namespace webgpu_engine::compute::nodes
