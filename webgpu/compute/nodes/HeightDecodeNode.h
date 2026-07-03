/*****************************************************************************
 * weBIGeo
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

#include "Node.h"
#include <webgpu/base/Buffer.h>
#include <webgpu/base/Context.h>
#include <webgpu/base/raii/CombinedComputePipeline.h>

namespace webgpu_compute::nodes {

class HeightDecodeNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(HeightDecodeNode)

    static glm::uvec3 SHADER_WORKGROUP_SIZE; // TODO currently hardcoded in shader! can we somehow not hardcode it? maybe using overrides

    struct HeightDecodeSettings {
        // The usage flags of the output texture
        WGPUTextureUsage texture_usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    };

    struct HeightDecodeSettingsUniform {
        glm::vec2 aabb_min;
        glm::vec2 aabb_max;
    };

    explicit HeightDecodeNode(webgpu::Context& ctx); // default-configured; for the NodeRegistry
    HeightDecodeNode(webgpu::Context& ctx, HeightDecodeSettings settings);

    // settings are consumed lazily in run_impl, applying them at any time is safe
    void set_settings(const HeightDecodeSettings& settings) { m_settings = settings; }
    const HeightDecodeSettings& get_settings() const { return m_settings; }
    void serialize_settings(QJsonObject& out) const override;
    void deserialize_settings(const QJsonObject& in) override;

public slots:
    void run_impl() override;

private:
    webgpu::Context* m_ctx;

    HeightDecodeSettings m_settings;
    webgpu::Buffer<HeightDecodeSettingsUniform> m_settings_uniform;
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_output_texture;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_pipeline;
};

} // namespace webgpu_compute::nodes
