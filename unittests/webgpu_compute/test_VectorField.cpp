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

// The baseline vector field of the MLS-MPM solver (MpmBaselineFieldBuilder), through the real C++
// class and the real WGSL kernel, on a synthetic grid velocity: every record against a float64
// reference, then the update protocol (two output buffers, a new field only for a new
// generation, the previous field dropped on a reset). The baseline is the contract between the
// solver and everything that reads a flow field, so this test pins it; see VectorField.h.

#include "../webgpu_engine/UnittestWebgpuContext.h"

#include <catch2/catch_test_macros.hpp>
#include <webgpu/base/raii/RawBuffer.h>
#include <webgpu/compute/VectorField.h>
#include <webgpu/compute/nodes/MpmSolverNode.h>
#include <webgpu/compute/nodes/MpmVectorFieldBuilder.h>

#include <cmath>
#include <cstring>
#include <random>

using namespace webgpu_compute;
using namespace webgpu_compute::nodes;

namespace {

constexpr glm::uvec3 RES(24, 16, 6); // columns x, y; layers
constexpr float DX = 12.5f;
constexpr float DENSITY = 400.0f;
constexpr float KG_PER_UNIT = 2.5f;
constexpr float MIN_MASS = 0.5f;

enum class Kind { Empty, Flow, Fringe };

struct Grid {
    std::vector<glm::vec4> nodes; // layer-major, as grid_velocity
    std::vector<Kind> kinds; // per column
};

// Empty columns hold velocity without mass (it must not count), flow columns random mass and
// velocity, fringe columns almost no mass and a huge velocity (it must not show up).
Grid make_grid(uint32_t seed, float mass_scale)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const uint32_t columns = RES.x * RES.y;
    Grid grid;
    grid.nodes.assign(size_t(columns) * RES.z, glm::vec4(0.0f));
    grid.kinds.resize(columns);
    for (uint32_t column = 0; column < columns; ++column) {
        const float pick = unit(rng);
        const Kind kind = pick < 0.35f ? Kind::Empty : pick < 0.85f ? Kind::Flow : Kind::Fringe;
        grid.kinds[column] = kind;
        for (uint32_t layer = 0; layer < RES.z; ++layer) {
            glm::vec4& node = grid.nodes[size_t(layer) * columns + column];
            switch (kind) {
            case Kind::Empty:
                node = glm::vec4(40.0f * (unit(rng) - 0.5f), 40.0f * (unit(rng) - 0.5f), 5.0f * (unit(rng) - 0.5f), 0.0f);
                break;
            case Kind::Flow:
                node = glm::vec4(30.0f * (unit(rng) - 0.5f), 30.0f * (unit(rng) - 0.5f), 4.0f * (unit(rng) - 0.5f), mass_scale * 3.0f * unit(rng));
                break;
            case Kind::Fringe:
                node = glm::vec4(300.0f * (unit(rng) - 0.5f), 300.0f * (unit(rng) - 0.5f), 50.0f * (unit(rng) - 0.5f), mass_scale * 0.02f * unit(rng));
                break;
            }
        }
    }
    return grid;
}

struct Expected {
    glm::dvec3 velocity { 0.0 };
    double depth = 0.0;
    double mass = 0.0;
    uint32_t flags = 0;
};

Expected reference(const Grid& grid, uint32_t column, float kg_per_unit)
{
    const uint32_t columns = RES.x * RES.y;
    double mass = 0.0;
    glm::dvec3 momentum(0.0);
    for (uint32_t layer = 0; layer < RES.z; ++layer) {
        const glm::vec4& node = grid.nodes[size_t(layer) * columns + column];
        mass += double(node.w);
        momentum += double(node.w) * glm::dvec3(node.x, node.y, node.z);
    }
    Expected e;
    if (mass >= double(MIN_MASS) && mass > 0.0) {
        e.velocity = momentum / mass;
        e.flags = FIELD_VELOCITY;
        if (kg_per_unit > 0.0f) {
            e.mass = mass * double(kg_per_unit);
            e.depth = e.mass / (double(DENSITY) * double(DX) * double(DX));
            e.flags |= FIELD_DEPTH | FIELD_MASS;
        }
    }
    return e;
}

struct Fixture {
    UnittestWebgpuContext context;
    webgpu::raii::RawBuffer<uint32_t> grid_velocity;
    webgpu::raii::RawBuffer<uint32_t> column_floor;
    MpmParticleView particles;
    MpmRawView raw;
    MpmBaselineFieldBuilder builder;

    Fixture()
        : grid_velocity(context.device, WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst), size_t(RES.x) * RES.y * RES.z * 4, "test grid velocity")
        , column_floor(context.device, WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst), size_t(RES.x) * RES.y, "test column floor")
        , builder(context.ctx)
    {
        builder.min_mass = MIN_MASS;
        particles.domain_origin = glm::vec2(1000.0f, 2000.0f);
        particles.dx = DX;
        particles.columns = glm::uvec2(RES.x, RES.y);
        particles.world_from_metres = glm::dmat3(glm::dvec3(1.25, 0.0, 0.0), glm::dvec3(0.0, 1.25, 0.0), glm::dvec3(10.0, 20.0, 1.0));
        particles.reset_count = 1;
        raw.particles = &particles;
        raw.grid_velocity = &grid_velocity;
        raw.column_floor = &column_floor;
        raw.grid_resolution = RES;
        raw.kg_per_mass_unit = KG_PER_UNIT;
        raw.snow_density = DENSITY;
        raw.generation = 1;
        raw.resource_generation = 1;
    }

    void upload(const Grid& grid)
    {
        static_assert(sizeof(glm::vec4) == 4 * sizeof(uint32_t));
        grid_velocity.write(context.queue, reinterpret_cast<const uint32_t*>(grid.nodes.data()), grid.nodes.size() * 4, 0);
    }

    void encode()
    {
        const webgpu::raii::CommandEncoder encoder(context.device, {});
        builder.encode(encoder.handle(), RawSource(&raw));
        WGPUCommandBufferDescriptor desc {};
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &desc);
        wgpuQueueSubmit(context.queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    std::vector<uint32_t> read(const webgpu::raii::RawBuffer<uint32_t>& buffer)
    {
        std::vector<uint32_t> words;
        // read_back_sync is not const, though a field is handed out as const.
        auto& readable = const_cast<webgpu::raii::RawBuffer<uint32_t>&>(buffer);
        REQUIRE(readable.read_back_sync(context.instance, context.device, words, 10000) == WGPUMapAsyncStatus_Success);
        REQUIRE(words.size() == size_t(RES.x) * RES.y * VECTOR_FIELD_STRIDE_WORDS);
        return words;
    }
};

float word_as_float(const std::vector<uint32_t>& words, size_t index)
{
    float value;
    std::memcpy(&value, &words[index], sizeof(float));
    return value;
}

// Every column of `words` against the reference; returns the largest relative error.
double check_records(const std::vector<uint32_t>& words, const Grid& grid, float kg_per_unit)
{
    double worst = 0.0;
    const double scale = 1e-5;
    for (uint32_t column = 0; column < RES.x * RES.y; ++column) {
        const Expected e = reference(grid, column, kg_per_unit);
        const size_t base = size_t(column) * VECTOR_FIELD_STRIDE_WORDS;
        REQUIRE(words[base + 7] == e.flags);
        // Channels not flagged valid are zero.
        const glm::dvec3 velocity(word_as_float(words, base), word_as_float(words, base + 1), word_as_float(words, base + 2));
        const double depth = word_as_float(words, base + 3);
        const double mass = word_as_float(words, base + 4);
        REQUIRE(word_as_float(words, base + 5) == 0.0f); // pressure: not provided
        REQUIRE(word_as_float(words, base + 6) == 0.0f); // coherence: not provided
        const auto close = [&](double got, double want, double tolerance_scale) {
            const double error = std::abs(got - want) / std::max(std::abs(want), tolerance_scale);
            worst = std::max(worst, error);
            return error <= scale * 10.0;
        };
        REQUIRE(close(velocity.x, e.velocity.x, 1.0));
        REQUIRE(close(velocity.y, e.velocity.y, 1.0));
        REQUIRE(close(velocity.z, e.velocity.z, 1.0));
        REQUIRE(close(depth, e.depth, 1e-3));
        REQUIRE(close(mass, e.mass, 1.0));
    }
    return worst;
}

} // namespace

TEST_CASE("MPM baseline field: records match the float64 reference")
{
    Fixture f;
    const Grid grid = make_grid(7, 1.0f);
    f.upload(grid);
    REQUIRE(f.builder.accepts(RawSource(&f.raw)));
    REQUIRE(f.builder.field() == nullptr); // nothing encoded yet

    f.encode();
    const VectorField* field = f.builder.field();
    REQUIRE(field != nullptr);
    CHECK(field->resolution == glm::uvec2(RES.x, RES.y));
    CHECK(field->cell_size_m == DX);
    CHECK(field->origin_metres == f.particles.domain_origin);
    CHECK(field->world_from_metres == f.particles.world_from_metres);
    CHECK(field->stride_words == VECTOR_FIELD_STRIDE_WORDS);
    REQUIRE(field->channels.size() == baseline_channels().size());
    CHECK(field->channels[0].name == "velocity");
    CHECK(field->previous == field->current); // the first update has nothing before it

    const auto words = f.read(*field->current);
    check_records(words, grid, KG_PER_UNIT);

    // The grid has the three kinds of column, so the test exercises every branch.
    size_t flow = 0, empty = 0, fringe = 0;
    for (const Kind kind : grid.kinds) {
        flow += kind == Kind::Flow;
        empty += kind == Kind::Empty;
        fringe += kind == Kind::Fringe;
    }
    CHECK(flow > 20);
    CHECK(empty > 20);
    CHECK(fringe > 10);
}

TEST_CASE("MPM baseline field: before the particle volume is known only the velocity is valid")
{
    Fixture f;
    f.raw.kg_per_mass_unit = 0.0f;
    const Grid grid = make_grid(11, 1.0f);
    f.upload(grid);
    f.encode();
    REQUIRE(f.builder.field() != nullptr);
    check_records(f.read(*f.builder.field()->current), grid, 0.0f);
}

TEST_CASE("MPM baseline field: update protocol")
{
    Fixture f;
    const Grid first = make_grid(3, 1.0f);
    f.upload(first);
    f.encode();
    const VectorField* field = f.builder.field();
    REQUIRE(field != nullptr);
    const auto* first_buffer = field->current;
    const uint64_t first_generation = field->generation;

    // A new chunk: written to the other buffer, the first one becomes `previous` and stays intact.
    const Grid second = make_grid(4, 2.0f);
    f.upload(second);
    f.raw.generation = 2;
    f.raw.interpolation = 0.25f;
    f.encode();
    REQUIRE(f.builder.field() != nullptr);
    CHECK(field->current != first_buffer);
    CHECK(field->previous == first_buffer);
    CHECK(field->generation == first_generation + 1);
    CHECK(field->interpolation == 0.25f);
    check_records(f.read(*field->current), second, KG_PER_UNIT);
    check_records(f.read(*field->previous), first, KG_PER_UNIT);

    // The same chunk again: nothing is encoded, only the interpolation follows.
    const auto* second_buffer = field->current;
    f.raw.interpolation = 0.75f;
    f.encode();
    CHECK(field->current == second_buffer);
    CHECK(field->previous == first_buffer);
    CHECK(field->generation == first_generation + 1);
    CHECK(field->interpolation == 0.75f);

    // A third chunk goes back to the first buffer.
    const Grid third = make_grid(5, 1.5f);
    f.upload(third);
    f.raw.generation = 3;
    f.encode();
    CHECK(field->current == first_buffer);
    CHECK(field->previous == second_buffer);
    check_records(f.read(*field->current), third, KG_PER_UNIT);

    // Reseeded: the field before belongs to another flow.
    f.raw.generation = 4;
    f.particles.reset_count = 2;
    f.encode();
    CHECK(field->previous == field->current);

    // No grid yet (the solver has not run): no field.
    f.raw.grid_velocity = nullptr;
    f.raw.generation = 5;
    f.encode();
    CHECK(f.builder.field() == nullptr);
}
