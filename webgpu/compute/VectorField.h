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

#include <webgpu/base/raii/RawBuffer.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace webgpu_compute {

namespace nodes {
    struct MpmRawView; // nodes/MpmSolverNode.h
}

/// The vector field of an avalanche model: a flow vector and a few attributes per cell of a
/// regular 2D grid, on the GPU. This is the contract between the models and whatever reads a
/// flow field (a streamline renderer, an export); neither side includes the other's code.
/// See mpm-mls-doc/16-vector-field.md.
///
/// Two layers:
/// - The baseline: every model has a FieldBuilder that turns its raw data into this record, and a
///   test that pins it. It always works with the current model.
/// - Experiments: any number of other FieldBuilders on the models' raw views, switched at run
///   time. A builder may append channels to the record; the baseline channels come first.
///
/// The record layout is shaders/vector_field.wgsl (struct FieldRecord); both change together,
/// and VECTOR_FIELD_VERSION with them.
constexpr uint32_t VECTOR_FIELD_VERSION = 1;
constexpr uint32_t VECTOR_FIELD_STRIDE_WORDS = 8;

/// Bits of FieldRecord::flags: which channels of a cell are valid. Same values as FIELD_* in
/// vector_field.wgsl.
enum FieldFlag : uint32_t {
    FIELD_VELOCITY = 1,
    FIELD_DEPTH = 2,
    FIELD_MASS = 4,
    FIELD_PRESSURE = 8,
    FIELD_COHERENCE = 16,
};

/// One named quantity in the record, so a reader can find what a builder provides.
struct FieldChannel {
    std::string name;
    uint32_t offset_words = 0; // in the record
    uint32_t components = 1;
    std::string unit;
};

/// The channels of the baseline record, in order.
inline std::vector<FieldChannel> baseline_channels()
{
    return {
        { "velocity", 0, 3, "m/s" },
        { "depth", 3, 1, "m" },
        { "mass", 4, 1, "kg" },
        { "pressure", 5, 1, "Pa" },
        { "coherence", 6, 1, "" },
        { "flags", 7, 1, "" },
    };
}

struct VectorField {
    /// One record of stride_words 32-bit words per cell, cell (x, y) at index y * resolution.x + x.
    const webgpu::raii::RawBuffer<uint32_t>* current = nullptr;
    /// The field before the last update, for interpolating between the two; the same as `current`
    /// when there is none (a static field, the first update, a reset).
    const webgpu::raii::RawBuffer<uint32_t>* previous = nullptr;
    float interpolation = 1.0f; // 0..1 from `previous` to `current`
    glm::uvec2 resolution = glm::uvec2(0);
    /// Cell (x, y) is sampled at origin_metres + (x, y) * cell_size_m, in the field's metre frame.
    glm::vec2 origin_metres = glm::vec2(0.0f);
    float cell_size_m = 1.0f;
    /// The field's metre frame to weBIGeo world xy (Web Mercator): world = M * (x, y, 1). The
    /// velocity is in the metre frame, so its direction on the map uses the linear part of M.
    glm::dmat3 world_from_metres = glm::dmat3(1.0);
    uint32_t stride_words = VECTOR_FIELD_STRIDE_WORDS;
    std::vector<FieldChannel> channels = baseline_channels();
    std::optional<float> simulated_time; // [s]; empty for a static field
    uint64_t generation = 0; // changes whenever the contents change
    uint64_t resource_generation = 0; // changes whenever the buffers are reallocated: rebuild bind groups
};

/// What a builder can read: a model's raw data, documented and versioned. One alternative per
/// model; MpmRawView so far.
using RawSource = std::variant<const nodes::MpmRawView*>;

/// Turns a model's raw data into a VectorField. The baseline is one builder, experiments are
/// more. A builder is cheap to keep around: it encodes work only when the model has new data.
class FieldBuilder {
public:
    virtual ~FieldBuilder() = default;

    /// Shown in a panel.
    virtual std::string name() const = 0;
    /// Whether this builder can read that model's data.
    virtual bool accepts(const RawSource& source) const = 0;
    /// Records the work for the model's current data into `encoder`, if it changed since the last
    /// call (the model's generation); otherwise nothing. Call before the frame that reads field().
    virtual void encode(WGPUCommandEncoder encoder, const RawSource& source) = 0;
    /// The field as of the last encode(), or nullptr while there is none.
    virtual const VectorField* field() const = 0;
};

/// Implemented by a model's node, next to TimeSteppedSimulation. Consumers find the models the
/// way SimulationDriver finds simulations: a dynamic_cast over the nodes of the graph.
class VectorFieldSource {
public:
    virtual ~VectorFieldSource() = default;

    /// The raw data of the model as it is now. The pointer stays valid as long as the node lives;
    /// the view's buffers are null while the model has not run.
    virtual RawSource raw() const = 0;
};

} // namespace webgpu_compute
