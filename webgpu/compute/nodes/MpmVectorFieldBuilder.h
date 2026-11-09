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

#pragma once

#include "../VectorField.h"

#include <webgpu/base/Context.h>
#include <webgpu/base/raii/BindGroup.h>
#include <webgpu/base/raii/BindGroupLayout.h>
#include <webgpu/base/raii/CombinedComputePipeline.h>
#include <webgpu/base/raii/RawBuffer.h>

#include <array>
#include <memory>

namespace webgpu_compute::nodes {

/// The baseline vector field of the MLS-MPM solver: its grid velocity averaged over depth,
/// weighted by node mass, one record per column of the terrain-following grid
/// (shaders/mpm_vector_field.wgsl). Velocity, depth and mass are provided; pressure and
/// coherence are not (their flags stay unset).
///
/// It has its own pipeline and bind group and reads only MpmRawView::grid_velocity, so it works
/// next to the solver's bind group, which already uses all the storage buffers a stage may have.
/// The pipeline is compiled once, at construction: no shader hot reload.
///
/// Keeps two output buffers and alternates between them, so field().previous is the field of the
/// update before, for interpolating between the two in real-time playback.
class MpmBaselineFieldBuilder : public FieldBuilder {
public:
    explicit MpmBaselineFieldBuilder(webgpu::Context& ctx);

    std::string name() const override { return "MPM baseline: grid velocity averaged over depth"; }
    bool accepts(const RawSource& source) const override;
    void encode(WGPUCommandEncoder encoder, const RawSource& source) override;
    const VectorField* field() const override { return m_has_field ? &m_field : nullptr; }

    /// A column holding less node mass than this (units of the normalised particle mass, so
    /// "this many particles' worth") is empty. Nodes at the edge of the flow have almost no mass
    /// but can have large velocities; this keeps a column made of them alone out of the field.
    float min_mass = 0.5f;

    /// The uniform of mpm_vector_field.wgsl (struct Params).
    struct Params {
        glm::uvec3 grid_res;
        float dx;
        float kg_per_mass_unit;
        float snow_density;
        float min_mass;
        float unused;
    };

private:
    void allocate(const MpmRawView& raw);

    webgpu::Context* m_ctx;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_layout;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_pipeline;

    // Per output slot: the field, its uniform (so an encode into one slot never changes what an
    // earlier pass into the other reads) and the bind group.
    std::array<std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>>, 2> m_fields;
    std::array<std::unique_ptr<webgpu::raii::RawBuffer<Params>>, 2> m_params;
    std::array<std::unique_ptr<webgpu::raii::BindGroup>, 2> m_bind_groups;

    glm::uvec2 m_allocated_columns = glm::uvec2(0);
    const webgpu::raii::RawBuffer<uint32_t>* m_bound_grid_velocity = nullptr;
    uint64_t m_bound_raw_generation = ~uint64_t(0); // the solver's resource_generation at the last allocate()
    uint64_t m_last_raw_generation = ~uint64_t(0); // the solver's generation at the last encode
    uint32_t m_last_reset_count = ~uint32_t(0);
    uint32_t m_slot = 0; // where the next encode writes
    bool m_have_previous = false;

    VectorField m_field;
    bool m_has_field = false;
};

} // namespace webgpu_compute::nodes
