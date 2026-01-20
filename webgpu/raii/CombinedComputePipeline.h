#pragma once

#include "BindGroup.h"
#include "BindGroupLayout.h"
#include "PipelineLayout.h"
#include "base_types.h"
#include <glm/glm.hpp>
#include <map>
#include <memory>

namespace webgpu::raii {

/// Convenience wrapper for compute pipeline
/// Usage:
///  - initialize with shader module and bind group layouts
///  - set pipeline input/output: specify what bind groups to use for what location
///  - run pipeline: calling run binds pipeline, sets bind groups and runs pipeline
class CombinedComputePipeline {
public:
    CombinedComputePipeline(WGPUDevice device, const raii::ShaderModule& shader_module, const std::vector<const raii::BindGroupLayout*>& bind_group_layouts,
        const std::string& label = "CombinedComputePipeline [no name]");

    // creates a new compute pass and only uses that to render
    void run(const raii::CommandEncoder& encoder, const glm::uvec3& workgroup_counts) const;
    void run(const raii::ComputePassEncoder& compute_pass, const glm::uvec3& workgroup_counts) const;

    void set_binding(uint32_t location, const raii::BindGroup& binding);

protected:
    std::unique_ptr<raii::PipelineLayout> m_layout;
    std::unique_ptr<raii::ComputePipeline> m_pipeline;
    std::map<uint32_t, const raii::BindGroup*> m_bindings;
};

} // namespace webgpu::raii
