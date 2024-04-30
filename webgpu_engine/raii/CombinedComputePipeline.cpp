#include "CombinedComputePipeline.h"

#include <algorithm>
#include <iterator>

namespace webgpu_engine::raii {

CombinedComputePipeline::CombinedComputePipeline(
    WGPUDevice device, const raii::ShaderModule& shader_module, const std::vector<const raii::BindGroupLayout*>& bind_group_layouts)
{
    std::vector<WGPUBindGroupLayout> bind_group_layout_handles;
    std::transform(bind_group_layouts.begin(), bind_group_layouts.end(), std::back_insert_iterator(bind_group_layout_handles),
        [](const raii::BindGroupLayout* layout) { return layout->handle(); });
    m_layout = std::make_unique<raii::PipelineLayout>(device, bind_group_layout_handles);

    WGPUComputePipelineDescriptor desc {};
    desc.label = "test"; // TODO
    desc.compute = {};
    desc.compute.entryPoint = "computeMain"; // TODO
    desc.compute.module = shader_module.handle();
    desc.layout = m_layout->handle();
    m_pipeline = std::make_unique<raii::ComputePipeline>(device, desc);
}

void CombinedComputePipeline::run(const raii::CommandEncoder& encoder, const glm::uvec3& workgroup_counts) const
{
    WGPUComputePassDescriptor compute_pass_desc {};
    compute_pass_desc.label = "compute pass"; // TODO
    raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);
    run(compute_pass, workgroup_counts);
}

void CombinedComputePipeline::run(const raii::ComputePassEncoder& compute_pass, const glm::uvec3& workgroup_counts) const
{
    wgpuComputePassEncoderSetPipeline(compute_pass.handle(), m_pipeline->handle());
    for (const auto& [location, bind_group] : m_bindings) {
        wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), location, bind_group->handle(), 0, nullptr);
    }
    wgpuComputePassEncoderDispatchWorkgroups(compute_pass.handle(), workgroup_counts.x, workgroup_counts.y, workgroup_counts.z);
};

void CombinedComputePipeline::set_binding(uint32_t location, const raii::BindGroup& binding)
{
    // TODO could validate against pipeline layout (= list of bind group layouts) here
    m_bindings[location] = &binding;
}

} // namespace webgpu_engine::raii
