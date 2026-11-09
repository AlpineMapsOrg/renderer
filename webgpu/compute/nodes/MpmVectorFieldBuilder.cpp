/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

#include "MpmVectorFieldBuilder.h"
#include "MpmSolverNode.h"

#include <algorithm>

namespace webgpu_compute::nodes {

static_assert(sizeof(MpmBaselineFieldBuilder::Params) == 32, "struct Params in mpm_vector_field.wgsl");

namespace {

    WGPUBindGroupLayoutEntry buffer_entry(uint32_t binding, WGPUBufferBindingType type)
    {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = binding;
        entry.visibility = WGPUShaderStage_Compute;
        entry.buffer.type = type;
        return entry;
    }

    constexpr uint32_t div_ceil(uint32_t value, uint32_t divisor) { return (value + divisor - 1u) / divisor; }

} // namespace

MpmBaselineFieldBuilder::MpmBaselineFieldBuilder(webgpu::Context& ctx)
    : m_ctx(&ctx)
{
    m_layout = std::make_unique<webgpu::raii::BindGroupLayout>(ctx.device(),
        std::vector<WGPUBindGroupLayoutEntry> {
            buffer_entry(0, WGPUBufferBindingType_Uniform),
            buffer_entry(1, WGPUBufferBindingType_ReadOnlyStorage), // grid velocity
            buffer_entry(2, WGPUBufferBindingType_Storage), // the field
        },
        "mpm vector field bind group layout");
    const auto module = ctx.resource_registry().compile_shader_from_code(ctx.device(), "///use webgpu_compute::mpm_vector_field\n", "mpm vector field");
    m_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(
        ctx.device(), *module, std::vector<const webgpu::raii::BindGroupLayout*> { m_layout.get() }, "mpm vector field pipeline");
}

bool MpmBaselineFieldBuilder::accepts(const RawSource& source) const
{
    const auto* raw = std::get_if<const MpmRawView*>(&source);
    return raw != nullptr && *raw != nullptr && (*raw)->layout_version == MpmRawView::LAYOUT_VERSION;
}

void MpmBaselineFieldBuilder::allocate(const MpmRawView& raw)
{
    const glm::uvec2 columns(raw.grid_resolution.x, raw.grid_resolution.y);
    const auto storage = WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc);
    const size_t words = size_t(columns.x) * columns.y * VECTOR_FIELD_STRIDE_WORDS;
    for (size_t slot = 0; slot < 2; ++slot) {
        m_fields[slot] = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(), storage, words, "mpm vector field");
        m_params[slot] = std::make_unique<webgpu::raii::RawBuffer<Params>>(
            m_ctx->device(), WGPUBufferUsage(WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst), 1, "mpm vector field params");
        m_bind_groups[slot] = std::make_unique<webgpu::raii::BindGroup>(m_ctx->device(), *m_layout,
            std::vector<WGPUBindGroupEntry> {
                m_params[slot]->create_bind_group_entry(0),
                raw.grid_velocity->create_bind_group_entry(1),
                m_fields[slot]->create_bind_group_entry(2),
            },
            "mpm vector field bind group");
    }
    m_allocated_columns = columns;
    m_bound_grid_velocity = raw.grid_velocity;
    m_bound_raw_generation = raw.resource_generation;
    m_have_previous = false;
    m_slot = 0;
    m_field.resource_generation++;
}

void MpmBaselineFieldBuilder::encode(WGPUCommandEncoder encoder, const RawSource& source)
{
    if (!accepts(source)) {
        m_has_field = false;
        return;
    }
    const MpmRawView& raw = *std::get<const MpmRawView*>(source);
    if (raw.grid_velocity == nullptr || raw.particles == nullptr || raw.grid_resolution.x == 0 || raw.grid_resolution.y == 0 || raw.grid_resolution.z == 0) {
        m_has_field = false;
        return;
    }

    const glm::uvec2 columns(raw.grid_resolution.x, raw.grid_resolution.y);
    if (columns != m_allocated_columns || raw.grid_velocity != m_bound_grid_velocity || raw.resource_generation != m_bound_raw_generation)
        allocate(raw);

    // Reseeded: the field before it belongs to another flow.
    if (raw.particles->reset_count != m_last_reset_count) {
        m_have_previous = false;
        m_last_reset_count = raw.particles->reset_count;
    }

    // What changes every frame, without a new chunk behind it.
    m_field.interpolation = raw.interpolation;
    m_field.simulated_time = raw.simulated_time;

    if (raw.generation != m_last_raw_generation) {
        m_last_raw_generation = raw.generation;

        const uint32_t slot = m_slot;
        const Params params { raw.grid_resolution, raw.particles->dx, raw.kg_per_mass_unit, raw.snow_density, min_mass, 0.0f };
        m_params[slot]->write(m_ctx->queue(), &params, 1, 0);
        {
            webgpu::raii::ComputePassEncoder pass(encoder, {});
            m_pipeline->set_binding(0, *m_bind_groups[slot]);
            m_pipeline->run(pass, glm::uvec3(div_ceil(columns.x, 8u), div_ceil(columns.y, 8u), 1u));
        }

        m_field.previous = m_have_previous ? m_fields[1 - slot].get() : m_fields[slot].get();
        m_field.current = m_fields[slot].get();
        m_field.generation++;
        m_have_previous = true;
        m_slot = 1 - slot;
    }

    m_field.resolution = columns;
    m_field.origin_metres = raw.particles->domain_origin;
    m_field.cell_size_m = raw.particles->dx;
    m_field.world_from_metres = raw.particles->world_from_metres;
    m_has_field = m_field.current != nullptr;
}

} // namespace webgpu_compute::nodes
