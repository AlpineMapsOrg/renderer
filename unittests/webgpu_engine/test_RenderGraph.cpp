/*****************************************************************************
 * Copyright (C) 2026 Matthias Huerbe
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


#include "UnittestWebgpuContext.h"
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <type_traits>
#include <vector>
#include <webgpu/base/RenderGraph.h>
#include <webgpu/base/RenderGraph_internal.h>
#include <webgpu/base/raii/BindGroup.h>
#include <webgpu/base/raii/RawBuffer.h>

using namespace webgpu;
using namespace webgpu::rg;
using webgpu::rg::Internal::AccessType;
using webgpu::rg::Internal::PassNode;
using webgpu::rg::Internal::ResourceAccess;
using webgpu::rg::Internal::storage;


static_assert(!std::is_default_constructible_v<RenderGraph>);
static_assert(!std::is_default_constructible_v<PassBuilder>);
static_assert(!std::is_default_constructible_v<PassContext>);
static_assert(!std::is_copy_constructible_v<RenderGraph> && !std::is_move_constructible_v<RenderGraph>);
static_assert(!std::is_copy_constructible_v<PassBuilder> && !std::is_move_constructible_v<PassBuilder>);
static_assert(!std::is_copy_constructible_v<PassContext> && !std::is_move_constructible_v<PassContext>);

// handles stay cheap values, and a typed handle converts to the base but never back
static_assert(std::is_trivially_copyable_v<TextureHandle>);
static_assert(std::is_convertible_v<TextureHandle, ResourceHandle>);
static_assert(!std::is_convertible_v<ResourceHandle, TextureHandle>);
static_assert(!std::is_convertible_v<BufferHandle, TextureHandle>);
static_assert(!std::is_convertible_v<ResourceHandle, bool>);
static_assert(!static_cast<bool>(ResourceHandle {}));
static_assert(!static_cast<bool>(TextureHandle {}));
static_assert(static_cast<bool>(ResourceHandle { 1, ResourceKind::Texture, 7 }));
static_assert(ResourceHandle { 1, ResourceKind::Texture, 7 } == ResourceHandle { 1, ResourceKind::Texture, 7 });
static_assert(!(ResourceHandle { 1, ResourceKind::Texture, 7 } == ResourceHandle { 1, ResourceKind::Texture, 8 }));
static_assert(!(ResourceHandle { 1, ResourceKind::Texture, 7 } == ResourceHandle { 1, ResourceKind::Buffer, 7 }));

struct TestGraph {
    GraphAllocator* allocator = create_allocator();
    RenderGraph* rg;

    TestGraph()
    {
        begin_frame(allocator);
        rg = start_recording(allocator);
    }
    ~TestGraph() { destroy_allocator(allocator); }

    TestGraph(const TestGraph&) = delete;
    TestGraph& operator=(const TestGraph&) = delete;

    TextureHandle transient(std::string_view id, uint32_t mipLevelCount = 1)
    {
        TextureDesc desc {};
        desc.dimension = WGPUTextureDimension_2D;
        desc.format = WGPUTextureFormat_RGBA8Unorm;
        desc.absolute = { 16, 16, 1 };
        desc.mipLevelCount = mipLevelCount;
        return rg->create_transient_texture(id, desc);
    }

    BufferHandle transient_buffer(std::string_view id, uint64_t size = 64)
    {
        BufferDesc desc {};
        desc.size = size;
        return rg->create_transient_buffer(id, desc);
    }
};

// declare a compute producer for buf so a later copy_buffer src is not a read-before-write error
static void add_buffer_producer(RenderGraph* rg, BufferHandle buf)
{
    rg->add_pass(
        "produce", PassKind::Compute, [&](PassBuilder& b) { b.storage_write(buf); }, [](PassContext&) {});
}

// 16x16 RGBA8Unorm 2D, matching TestGraph::transient
static TextureDesc tex2d()
{
    TextureDesc d {};
    d.dimension = WGPUTextureDimension_2D;
    d.format = WGPUTextureFormat_RGBA8Unorm;
    d.absolute = { 16, 16, 1 };
    return d;
}

// fake non-null view. compile() only reads the `imported` flag, and these tests stop at compile().
// import_buffer would not do: it calls wgpuBufferGetSize.
static TextureHandle import_tex(RenderGraph* rg, std::string_view id)
{
    return rg->import_texture(id, { .view = (WGPUTextureView)0x1, .size = { 16, 16, 1 }, .format = WGPUTextureFormat_RGBA8Unorm });
}

// surviving passes in execution order after compile()
static std::vector<std::string> pass_order(RenderGraph* rg)
{
    std::vector<std::string> v;
    for (PassNode* p = storage(rg)->m_passes; p; p = p->next)
        v.emplace_back(p->id.name.data, p->id.name.length);
    return v;
}
static int idx_of(const std::vector<std::string>& v, const char* n)
{
    for (int i = 0; i < static_cast<int>(v.size()); ++i)
        if (v[i] == n)
            return i;
    return -1;
}

// ---------------------------------------------------------------------------------------------------
// ViewRange presets and cube-view validation.

// pins the specific diagnostic, not just "some error fired"
static bool error_mentions(RenderGraph* rg, std::string_view needle)
{
    for (ErrorMessage* e = rg->get_errors(); e; e = e->next)
        if (std::string_view(e->message.data, e->message.length).find(needle) != std::string_view::npos)
            return true;
    return false;
}


TEST_CASE("rg::cube preset builds a 6-layer cube view", "[RenderGraph]")
{
    constexpr ViewRange c = cube();
    STATIC_REQUIRE(c.dim == WGPUTextureViewDimension_Cube);
    STATIC_REQUIRE(c.layerCount == 6); // the whole point: the bare ViewRange default (1) is an invalid cube
    STATIC_REQUIRE(c.mipCount == 1); // default stays single-mip
    STATIC_REQUIRE(c.aspect == WGPUTextureAspect_All);

    // aspect passes through, for a depth shadow cube
    STATIC_REQUIRE(cube(WGPUTextureAspect_DepthOnly).aspect == WGPUTextureAspect_DepthOnly);
    // mipCount 0 == all remaining, for a prefiltered IBL env cube spanning its whole mip chain
    STATIC_REQUIRE(cube(WGPUTextureAspect_All, 0).mipCount == 0);
}

TEST_CASE("rg::cube_array preset builds a 6*N-layer cube-array view", "[RenderGraph]")
{
    STATIC_REQUIRE(cube_array(1).layerCount == 6);
    STATIC_REQUIRE(cube_array(3).layerCount == 18);
    STATIC_REQUIRE(cube_array(3).dim == WGPUTextureViewDimension_CubeArray);
    STATIC_REQUIRE(cube_array(3).mipCount == 1);
    STATIC_REQUIRE(cube_array(2, WGPUTextureAspect_All, 0).mipCount == 0);
}

TEST_CASE("rg::whole preset is all mips and all layers with an inferred dimension", "[RenderGraph]")
{
    constexpr ViewRange w = whole();
    STATIC_REQUIRE(w.dim == WGPUTextureViewDimension_Undefined); // inferred from the texture
    STATIC_REQUIRE(w.mipCount == 0); // 0 == all remaining
    STATIC_REQUIRE(w.layerCount == 0);
}

// a `layers`-layer 2D array, written by a producer so the sampled consumer is not a read-before-write,
// then sampled through `range`.
static TextureHandle cube_source(TestGraph& g, uint32_t layers, WGPUTextureDimension dim = WGPUTextureDimension_2D)
{
    TextureDesc desc {};
    desc.dimension = dim;
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.absolute = { 16, 16, layers };
    auto h = g.rg->create_transient_texture("cubesrc", desc);
    g.rg->add_pass(
        "produce", PassKind::Graphics,
        [h, layers, dim](PassBuilder& b) {
            // a 3D texture has no array layers to attach per-layer, so one attachment defines it
            uint32_t n = (dim == WGPUTextureDimension_2D) ? layers : 1;
            for (uint32_t l = 0; l < n; ++l)
                b.color(h, l, { .clear = { 0, 0, 0, 1 }, .sub = { .layer = l } });
        },
        [](PassContext&) {});
    return h;
}

TEST_CASE("compile - a valid 6-layer cube view passes validation", "[RenderGraph]")
{
    TestGraph g;
    auto h = cube_source(g, 6);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, cube());
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

TEST_CASE("compile - a cube view without exactly 6 layers is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = cube_source(g, 4);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, ViewRange { .mipCount = 1, .layerCount = 4, .dim = WGPUTextureViewDimension_Cube });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "needs exactly 6")); // named error, not an opaque device error
}

TEST_CASE("compile - a cube-array view whose layer count is not a multiple of 6 is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = cube_source(g, 8);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, ViewRange { .mipCount = 1, .layerCount = 8, .dim = WGPUTextureViewDimension_CubeArray });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "positive multiple of 6"));
}

// the overrun check sits after the exactly-6 / multiple-of-6 arms, so reaching it needs a view those
// accept: a well-formed 6-layer cube starting too late in an 8-layer texture. 8 is the widest source
// here, since cube_source declares one color() per layer and kMaxColorAttachments caps that.
TEST_CASE("compile - a well-formed cube view starting past the layer count is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = cube_source(g, 8);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, cube().at(0, 4)); // 4 + 6 > 8
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "exceeds the texture's 8 layer(s)"));
    REQUIRE_FALSE(error_mentions(g.rg, "needs exactly 6")); // the layer count itself is fine
}

// cube validation reads the layer count off the import's declared size, so a caller-owned cube view
// registered as the 1-layer default is rejected however cube-shaped the real view is. documented in
// docs/rendergraph.md, the IBL example: sample a view-only import with a plain sampled(), no range.
TEST_CASE("compile - a cube view on a view-only import is rejected on the declared layer count", "[RenderGraph]")
{
    TestGraph g;
    auto env = import_tex(g.rg, "ibl.env"); // size { 16, 16, 1 }, so one layer
    g.rg->add_pass(
        "read", PassKind::Compute,
        [env](PassBuilder& b) {
            b.sampled(env, cube());
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    // the overrun arm, not the covers-N one: cube() sets exactly 6 layers, so the count is well formed
    // and it is the 1-layer import it does not fit
    REQUIRE(error_mentions(g.rg, "exceeds the texture's 1 layer(s)"));
    REQUIRE_FALSE(error_mentions(g.rg, "needs exactly 6"));
}

// the same import declaring its real layer count compiles: the check is on the declared size, not on
// imports as such.
TEST_CASE("compile - a cube view on an import declaring 6 layers passes", "[RenderGraph]")
{
    TestGraph g;
    auto env = g.rg->import_texture(
        "ibl.env", { .view = (WGPUTextureView)0x1, .size = { 16, 16, 6 }, .format = WGPUTextureFormat_RGBA8Unorm });
    g.rg->add_pass(
        "read", PassKind::Compute,
        [env](PassBuilder& b) {
            b.sampled(env, cube());
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

// the prefiltered specular cube from docs/rendergraph.md: a graph-owned persistent cube baked one
// (mip, layer) at a time via color(), every bake pass gating on initialize() of the same target, then
// sampled as a whole-chain cube. proves the doc's three load-bearing claims compile together: many
// initialize() passes share one target, a per-layer color() bake coexists with a cube() read of the
// same resource, and cube(All, 0) accepts the full mip chain.
TEST_CASE("compile - a prefiltered specular cube bakes per (mip, layer) and samples as a full-chain cube", "[RenderGraph]")
{
    constexpr uint32_t kFaces = 6;
    constexpr uint32_t kRoughnessMips = 3;

    TestGraph g;
    TextureDesc desc {};
    desc.dimension = WGPUTextureDimension_2D;
    desc.format = WGPUTextureFormat_RGBA16Float;
    desc.absolute = { 128, 128, kFaces };
    desc.mipLevelCount = kRoughnessMips;
    auto prefiltered = g.rg->create_persistent_texture("ibl.prefiltered", desc);

    for (uint32_t mip = 0; mip < kRoughnessMips; ++mip)
        for (uint32_t face = 0; face < kFaces; ++face)
            g.rg->add_pass(
                "prefilter.bake", PassKind::Graphics,
                [prefiltered, mip, face](PassBuilder& b) {
                    b.color(prefiltered, 0, { .sub = { .mip = mip, .layer = face } });
                    b.initialize(prefiltered, /*hash*/ 42); // one env identity, whole set bakes together
                },
                [](PassContext&) {});

    auto lit = g.transient("lit");
    g.rg->add_pass(
        "IBL", PassKind::Graphics,
        [prefiltered, lit](PassBuilder& b) {
            b.sampled(prefiltered, cube(WGPUTextureAspect_All, /*mipCount*/ 0)); // every roughness level
            b.color(lit, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

TEST_CASE("compile - a cube view on a storage access is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = cube_source(g, 6);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.storage_read(h, cube());
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "cannot be cube"));
}

TEST_CASE("compile - a cube view on a non-2D texture is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = cube_source(g, 6, WGPUTextureDimension_3D);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, cube());
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "not a 2D texture"));
}

// the IBL environment path. import_texture defaults dimension to 2D precisely so node_layers sees the
// imported extent's 6 layers, not the Undefined default's 1.
TEST_CASE("compile - an imported 6-layer cube view passes validation", "[RenderGraph]")
{
    TestGraph g;
    auto h = g.rg->import_texture("envcube", { .view = (WGPUTextureView)0x1, .size = { 16, 16, 6 }, .format = WGPUTextureFormat_RGBA8Unorm });
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, cube());
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

// an imported texture's layer count is its extent's, not 1, which only holds if the node knows it is 2D
TEST_CASE("compile - an imported texture reports its real layer count to cube validation", "[RenderGraph]")
{
    TestGraph g;
    auto h = g.rg->import_texture("arr", { .view = (WGPUTextureView)0x1, .size = { 16, 16, 4 }, .format = WGPUTextureFormat_RGBA8Unorm });
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, ViewRange { .mipCount = 1, .layerCount = 0, .dim = WGPUTextureViewDimension_Cube }); // 0 == all remaining
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "covers 4 layer(s)")); // not "1 layer(s)"
}

// a cube view of an imported 3D texture is rejected as non-2D
TEST_CASE("compile - an imported 3D texture is rejected for a cube view", "[RenderGraph]")
{
    TestGraph g;
    auto h = g.rg->import_texture("vol", { .view = (WGPUTextureView)0x1, .size = { 16, 16, 6 }, .format = WGPUTextureFormat_RGBA8Unorm, .dimension = WGPUTextureDimension_3D });
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, cube());
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "not a 2D texture"));
}

// ---------------------------------------------------------------------------------------------------
// access range validation. ranges are fully declared in setup, so compile() rejects one that does not
// fit its resource.

TEST_CASE("compile - an access past the texture's mip count is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = g.transient("tex"); // 1 mip
    g.rg->add_pass(
        "write", PassKind::Graphics,
        [h](PassBuilder& b) {
            b.color(h, 0, { .sub = { .mip = 3 } });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "accessed at mip 3 but has 1 mip(s)"));
}

TEST_CASE("compile - an access past the texture's layer count is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = cube_source(g, 6);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, { .baseLayer = 9 });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "accessed at layer 9 but has 6 layer(s)"));
}

TEST_CASE("compile - a view whose mip range ends past the chain is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = g.transient("chain", 2);
    g.rg->add_pass(
        "write", PassKind::Compute,
        [h](PassBuilder& b) {
            b.storage_write(h, ViewRange { .mipCount = 3, .layerCount = 1 });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "view of 3 mip(s) from mip 0 but has 2 mip(s)"));
}

// the layer half. a non-cube viewDim keeps this out of cube validation, so the range check must catch it.
TEST_CASE("compile - a view whose layer range ends past the array is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto h = cube_source(g, 2);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [h](PassBuilder& b) {
            b.sampled(h, ViewRange { .mipCount = 1, .layerCount = 3 }); // 0 + 3 > 2
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "view of 3 layer(s) from layer 0 but has 2 layer(s)"));
}

// 0 means all remaining, so rg::whole() stays clean on any mip chain
TEST_CASE("compile - a whole() view over a mip chain passes range validation", "[RenderGraph]")
{
    TestGraph g;
    auto h = g.transient("chain", 4);
    g.rg->add_pass(
        "write", PassKind::Compute,
        [h](PassBuilder& b) {
            b.storage_write(h, whole());
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

TEST_CASE("compile - a copy_texture past the dst's layer count is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto src = g.transient("src");
    auto dst = g.transient("dst"); // 1 layer
    g.rg->add_pass(
        "produce", PassKind::Graphics, [src](PassBuilder& b) { b.color(src, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [src, dst](PassBuilder& b) {
            b.copy_texture(src, dst, {}, { .layer = 2 });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "accessed at layer 2 but has 1 layer(s)"));
}

// copy_buffer resolves the size at declare and records it on both sides, so the dst range is checked
// against the dst buffer
TEST_CASE("compile - a copy_buffer range that overruns the dst is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto bufA = g.transient_buffer("bufA", 64);
    auto bufB = g.transient_buffer("bufB", 64);

    add_buffer_producer(g.rg, bufA);
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [bufA, bufB](PassBuilder& b) {
            b.copy_buffer(bufA, bufB, 0, /*dstOffset*/ 32, /*size*/ 64); // 32 + 64 > 64
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "covers bytes [32, 96) but the buffer is 64 byte(s)"));
}

// size 0 expands against the src at declare time, so an offset src copy lands a smaller resolved size
// on both accesses and stays inside a same-sized dst
TEST_CASE("compile - a whole copy_buffer from an offset src fits a same-sized dst", "[RenderGraph]")
{
    TestGraph g;
    auto bufA = g.transient_buffer("bufA", 64);
    auto bufB = g.transient_buffer("bufB", 64);

    add_buffer_producer(g.rg, bufA);
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [bufA, bufB](PassBuilder& b) {
            b.copy_buffer(bufA, bufB, /*srcOffset*/ 32, 0, /*size*/ 0); // resolves to 32 bytes
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

// a declared bind range shares the copy range's bounds check, so an overrun is caught at compile
TEST_CASE("compile - a bind range that overruns the buffer is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto buf = g.transient_buffer("buf", 64);

    add_buffer_producer(g.rg, buf);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [buf](PassBuilder& b) {
            b.storage_read(buf, { .offset = 32, .size = 64 }); // 32 + 64 > 64
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "covers bytes [32, 96) but the buffer is 64 byte(s)"));
}

// bind offsets must hit the spec-default 256-byte uniform/storage offset alignment, or Dawn rejects
// the bind group at execute with a far less local message
TEST_CASE("compile - a bind range with a misaligned offset is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto buf = g.transient_buffer("buf", 64);

    add_buffer_producer(g.rg, buf);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [buf](PassBuilder& b) {
            b.uniform(buf, { .offset = 16, .size = 32 });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "not 256-byte aligned"));
}

// an offset at the end resolves to a zero-byte range, which the > bounds check alone would miss
TEST_CASE("compile - a bind range starting at the buffer's end is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto buf = g.transient_buffer("buf", 512);

    add_buffer_producer(g.rg, buf);
    g.rg->add_pass(
        "read", PassKind::Compute,
        [buf](PassBuilder& b) {
            b.storage_read(buf, { .offset = 512 });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "starts at byte 512 but the buffer is 512 byte(s)"));
}

// ---------------------------------------------------------------------------------------------------
// compile() reports through the error chain rather than asserting, so these are observable in any build.

TEST_CASE("compile - a cyclic relativeTo chain is rejected", "[RenderGraph]")
{
    TestGraph g;
    // the public API cannot author a cycle, relativeTo only ever points backwards. build the loop on the
    // nodes to reach resolve_size's recursion guard.
    auto a = g.transient("a");
    TextureDesc rel {};
    rel.dimension = WGPUTextureDimension_2D;
    rel.format = WGPUTextureFormat_RGBA8Unorm;
    rel.sizeKind = SizeKind::Relative;
    rel.relativeTo = a;
    auto b = g.rg->create_transient_texture("b", rel);

    Internal::ResourceNode* an = Internal::find_node(g.rg, a);
    an->sizeKind = SizeKind::Relative; // close the loop: a sizes against b, b sizes against a
    an->relativeToHandle = b;

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "cyclic relativeTo chain"));
}

// ---------------------------------------------------------------------------------------------------
// foreign/stale handle backstops, not authoring diagnostics: an out-of-range id would index compile()'s
// phase-1/2 scratch tables out of bounds. the builder's Q_ASSERTs stop such a handle being declared at
// all, hence the white-box stamping. a null byId witnesses that compile() bailed before building the
// tables the bad id would have indexed.

TEST_CASE("compile - an out-of-range access handle is rejected before the scratch tables are built", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");
    g.rg->add_pass(
        "write", PassKind::Graphics,
        [tex](PassBuilder& b) {
            b.color(tex, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    // an id past next_resource_id is the case that would read byId[] out of bounds
    storage(g.rg)->m_passes->accesses[0].handle.id = 9999;

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "uses an invalid resource handle (id 9999)"));
    REQUIRE(storage(g.rg)->byId == nullptr); // bailed before byId[9999] could be written
}

TEST_CASE("compile - an access handle from another graph is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");
    g.rg->add_pass(
        "write", PassKind::Graphics,
        [tex](PassBuilder& b) {
            b.color(tex, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    // in range, but stamped with another graph's generation: the id would resolve to the wrong node
    storage(g.rg)->m_passes->accesses[0].handle.generation = storage(g.rg)->generation + 1;

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "uses an invalid resource handle"));
    REQUIRE(storage(g.rg)->byId == nullptr);
}

TEST_CASE("compile - a foreign initialize() target handle is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto pers = g.rg->create_persistent_texture("pers", tex2d());
    g.rg->add_pass(
        "bake", PassKind::Graphics,
        [pers](PassBuilder& b) {
            b.color(pers, 0);
            b.initialize(pers);
            b.force_keep();
        },
        [](PassContext&) {});

    // only the init target goes stale, so the access loop passes and the init loop must catch this
    storage(g.rg)->m_passes->initTargets[0].target.generation = storage(g.rg)->generation + 1;

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "initialize() target handle"));
    REQUIRE(storage(g.rg)->byId == nullptr);
}

TEST_CASE("compile - a foreign relativeTo handle is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto base = g.transient("base");
    TextureDesc rel {};
    rel.dimension = WGPUTextureDimension_2D;
    rel.format = WGPUTextureFormat_RGBA8Unorm;
    rel.sizeKind = SizeKind::Relative;
    rel.relativeTo = base;
    auto scaled = g.rg->create_transient_texture("scaled", rel);

    // resolve_size() indexes relativeTo unguarded, so the prologue is the only thing stopping an
    // out-of-bounds read
    Internal::find_node(g.rg, scaled)->relativeToHandle.generation = storage(g.rg)->generation + 1;

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "relativeTo"));
    REQUIRE(error_mentions(g.rg, "from another graph or a previous frame"));
    REQUIRE(storage(g.rg)->byId == nullptr);
}

TEST_CASE("compile - a second compile() on the same graph is rejected", "[RenderGraph]")
{
    TestGraph g;
    REQUIRE(g.rg->compile() == nullptr);

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "already compiled"));
}

// ---------------------------------------------------------------------------------------------------
// explicit color-attachment slots

// the ColorAttachment access for `h` in `p`, or null when the pass declares none
static const ResourceAccess* color_access(PassNode* p, ResourceHandle h)
{
    for (uint32_t i = 0; i < p->accessCount; ++i)
        if (p->accesses[i].type == AccessType::ColorAttachment && p->accesses[i].handle.id == h.id)
            return &p->accesses[i];
    return nullptr;
}

static uint32_t count_access(PassNode* p, AccessType t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < p->accessCount; ++i)
        if (p->accesses[i].type == t)
            ++n;
    return n;
}

TEST_CASE("PassBuilder::color - the declared slot is carried on the access", "[RenderGraph]")
{
    TestGraph g;
    auto a = g.transient("a");
    auto b = g.transient("b");

    g.rg->add_pass(
        "mrt", PassKind::Graphics,
        [a, b](PassBuilder& pb) {
            pb.color(a, 3);
            pb.color(b, 1);
            pb.force_keep();
        },
        [](PassContext&) {});

    PassNode* p = storage(g.rg)->m_passes;
    REQUIRE(color_access(p, a)->colorIndex == 3);
    REQUIRE(color_access(p, b)->colorIndex == 1); // declared second, but slot 1
}

// release-only: these feed color()/resolve() an author error the builder guards with Q_ASSERT. in debug
// the abort IS the contract, so only the release backstop is observable here.
#ifdef QT_NO_DEBUG
TEST_CASE("PassBuilder::color - a slot declared twice in one pass is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto a = g.transient("a");
    auto b = g.transient("b");

    g.rg->add_pass(
        "dup", PassKind::Graphics,
        [a, b](PassBuilder& pb) {
            pb.color(a, 0);
            pb.color(b, 0); // same slot -> rejected, access not recorded
            pb.force_keep();
        },
        [](PassContext&) {});

    PassNode* p = storage(g.rg)->m_passes;
    REQUIRE(count_access(p, AccessType::ColorAttachment) == 1);
    REQUIRE(color_access(p, a) != nullptr);
    REQUIRE(color_access(p, b) == nullptr); // the duplicate never bound
}
#endif

#ifdef QT_NO_DEBUG
TEST_CASE("PassBuilder::color - a slot at or past kMaxColorAttachments is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto a = g.transient("a");

    g.rg->add_pass(
        "oob", PassKind::Graphics,
        [a](PassBuilder& pb) {
            pb.color(a, webgpu::rg::Internal::kMaxColorAttachments);
            pb.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(count_access(storage(g.rg)->m_passes, AccessType::ColorAttachment) == 0);
}
#endif

TEST_CASE("PassBuilder::resolve - the target carries its src color slot", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc msaaDesc = tex2d();
    msaaDesc.sampleCount = 4;
    auto msaa = g.rg->create_transient_texture("msaa", msaaDesc);
    auto target = g.transient("target");

    g.rg->add_pass(
        "fwd", PassKind::Graphics,
        [msaa, target](PassBuilder& b) {
            b.color(msaa, 2);
            b.resolve(msaa, target);
            b.force_keep();
        },
        [](PassContext&) {});

    PassNode* p = storage(g.rg)->m_passes;
    bool found = false;
    for (uint32_t i = 0; i < p->accessCount; ++i)
        if (p->accesses[i].type == AccessType::ResolveAttachment && p->accesses[i].handle.id == target.id) {
            REQUIRE(p->accesses[i].colorIndex == 2); // the slot of the color() it resolves, not 0
            found = true;
        }
    REQUIRE(found);
}

// pairing does not depend on declaration adjacency: an unrelated color() may sit between the two
TEST_CASE("PassBuilder::resolve - src pairing does not depend on declaration adjacency", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc msaaDesc = tex2d();
    msaaDesc.sampleCount = 4;
    auto msaa = g.rg->create_transient_texture("msaa", msaaDesc);
    auto other = g.transient("other");
    auto target = g.transient("target");

    g.rg->add_pass(
        "fwd", PassKind::Graphics,
        [msaa, other, target](PassBuilder& b) {
            b.color(msaa, 0);
            b.color(other, 1); // declared between the color() and its resolve()
            b.resolve(msaa, target);
            b.force_keep();
        },
        [](PassContext&) {});

    PassNode* p = storage(g.rg)->m_passes;
    bool found = false;
    for (uint32_t i = 0; i < p->accessCount; ++i)
        if (p->accesses[i].type == AccessType::ResolveAttachment && p->accesses[i].handle.id == target.id) {
            REQUIRE(p->accesses[i].colorIndex == 0); // msaa's slot, not the adjacent other's
            found = true;
        }
    REQUIRE(found);
}

#ifdef QT_NO_DEBUG
TEST_CASE("PassBuilder::resolve - a src that is not a color() in this pass is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto notAnAttachment = g.transient("nope");
    auto target = g.transient("target");

    g.rg->add_pass(
        "fwd", PassKind::Graphics,
        [notAnAttachment, target](PassBuilder& b) {
            b.resolve(notAnAttachment, target); // src was never declared via color()
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(count_access(storage(g.rg)->m_passes, AccessType::ResolveAttachment) == 0);
}
#endif

#ifdef QT_NO_DEBUG
TEST_CASE("PassBuilder::resolve - two resolve targets for one color slot are rejected", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc msaaDesc = tex2d();
    msaaDesc.sampleCount = 4;
    auto msaa = g.rg->create_transient_texture("msaa", msaaDesc);
    auto t1 = g.transient("t1");
    auto t2 = g.transient("t2");

    g.rg->add_pass(
        "fwd", PassKind::Graphics,
        [msaa, t1, t2](PassBuilder& b) {
            b.color(msaa, 0);
            b.resolve(msaa, t1);
            b.resolve(msaa, t2); // same slot resolved twice -> rejected
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(count_access(storage(g.rg)->m_passes, AccessType::ResolveAttachment) == 1);
}
#endif

// the remaining builder guards, same release-only reasoning. unlike color()/resolve() these do NOT drop
// the declaration, so the release behavior worth pinning is that the access lands anyway.
#ifdef QT_NO_DEBUG
TEST_CASE("PassBuilder - reading and writing one resource in a pass is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");

    g.rg->add_pass(
        "rw", PassKind::Graphics,
        [tex](PassBuilder& b) {
            b.sampled(tex); // same subresource, read and written in one pass: the graph cannot order these
            b.color(tex, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    // both accesses are recorded: the guard traps, it does not filter
    PassNode* p = storage(g.rg)->m_passes;
    REQUIRE(count_access(p, AccessType::Sampled) == 1);
    REQUIRE(count_access(p, AccessType::ColorAttachment) == 1);
}
#endif

// the Transfer exemption is not release-only: each copy is its own usage scope, so this stays silent in
// debug too. pins the access count that exemption produces.
TEST_CASE("PassBuilder - a Transfer pass may read and write one resource", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex", /*mipLevelCount*/ 2);

    g.rg->add_pass(
        "produce", PassKind::Graphics, [tex](PassBuilder& b) { b.color(tex, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [tex](PassBuilder& b) {
            b.copy_texture(tex, tex, {}, { .mip = 1 }); // src and dst are the same handle, no conflict trap
            b.force_keep();
        },
        [](PassContext&) {});

    PassNode* copyPass = storage(g.rg)->m_passes->next;
    REQUIRE(count_access(copyPass, AccessType::CopySrc) == 1);
    REQUIRE(count_access(copyPass, AccessType::CopyDst) == 1);
}

#ifdef QT_NO_DEBUG
TEST_CASE("PassBuilder::depth_stencil - a second depth-stencil attachment is rejected", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc dsDesc = tex2d();
    dsDesc.format = WGPUTextureFormat_Depth32Float;
    auto d1 = g.rg->create_transient_texture("d1", dsDesc);
    auto d2 = g.rg->create_transient_texture("d2", dsDesc);

    g.rg->add_pass(
        "shadow", PassKind::Graphics,
        [d1, d2](PassBuilder& b) {
            b.depth_stencil(d1);
            b.depth_stencil(d2); // a render pass has one depth-stencil slot
            b.force_keep();
        },
        [](PassContext&) {});

    // no early return, so both land and execute() would silently let the last one win
    REQUIRE(count_access(storage(g.rg)->m_passes, AccessType::DepthStencilAttachment) == 2);
}
#endif

#ifdef QT_NO_DEBUG
TEST_CASE("PassBuilder::initialize - the same target twice in one pass is rejected", "[RenderGraph]")
{
    TestGraph g;
    BufferDesc bd {};
    bd.size = 64;
    auto p = g.rg->create_persistent_buffer("pers", bd);

    g.rg->add_pass(
        "bake", PassKind::Compute,
        [p](PassBuilder& b) {
            b.storage_write(p);
            b.initialize(p);
            b.initialize(p); // duplicate: asserted, but not dropped
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(storage(g.rg)->m_passes->initCount == 2);
}
#endif

#ifdef QT_NO_DEBUG
TEST_CASE("PassBuilder::initialize - a target past kMaxInitTargets is dropped", "[RenderGraph]")
{
    TestGraph g;
    BufferDesc bd {};
    bd.size = 64;
    // buffers, not textures: kMaxColorAttachments would cap a multi-target Graphics pass at 8 first
    constexpr uint32_t kOver = Internal::PassNode::kMaxInitTargets + 1;
    std::vector<BufferHandle> targets;
    std::vector<std::string> names(kOver);
    for (uint32_t i = 0; i < kOver; ++i) {
        names[i] = "pers" + std::to_string(i);
        targets.push_back(g.rg->create_persistent_buffer(names[i], bd));
    }

    g.rg->add_pass(
        "bake", PassKind::Compute,
        [&targets](PassBuilder& b) {
            for (BufferHandle t : targets) {
                b.storage_write(t);
                b.initialize(t);
            }
            b.force_keep();
        },
        [](PassContext&) {});

    // the 9th target is dropped rather than overrunning the fixed array
    REQUIRE(storage(g.rg)->m_passes->initCount == Internal::PassNode::kMaxInitTargets);
}
#endif

TEST_CASE("RenderGraph - valid graph compiles clean", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");

    g.rg->add_pass(
        "write", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(tex, 0);
            b.force_keep(); // nothing reads tex; keep the pass past culling
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    // keeps the foreign-handle tests honest: they assert byId stays null when compile() bails, so a clean
    // compile must be what populates it
    REQUIRE(storage(g.rg)->byId != nullptr);
}

TEST_CASE("RenderGraph - read before write is reported as an error", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");

    // reads a transient no pass ever writes -> compile() must poison the graph
    g.rg->add_pass(
        "read", PassKind::Compute,
        [&](PassBuilder& b) {
            b.sampled(tex);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    // nobody declared a write anywhere, which is the first of the three read-before-write diagnoses
    REQUIRE(error_mentions(g.rg, "that no pass ever writes"));
}

// the third read-before-write diagnosis: a writer exists, but the reader is declared first, so no RAW
// edge forms and the writer is culled instead of being pulled ahead of it.
TEST_CASE("RenderGraph - reading before a later-declared writer names the ordering diagnosis", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");

    g.rg->add_pass( // declared first, so at sweep time tex has no producer to depend on
        "read", PassKind::Compute,
        [&](PassBuilder& b) {
            b.sampled(tex);
            b.force_keep();
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "write", PassKind::Graphics, [&](PassBuilder& b) { b.color(tex, 0); }, [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "before any pass writes it"));
    REQUIRE_FALSE(error_mentions(g.rg, "that no pass ever writes")); // a writer does exist
}


TEST_CASE("RenderGraph - single clear+discard attachment is inferred transient", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");

    g.rg->add_pass(
        "write", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(tex, 0, { .store = WGPUStoreOp_Discard });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(storage(g.rg)->transientCount == 1);
}


TEST_CASE("RenderGraph - sampled attachment is not transient", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");
    auto out = g.transient("out");

    g.rg->add_pass(
        "write", PassKind::Graphics,
        [&](PassBuilder& b) { b.color(tex, 0); },
        [](PassContext&) {});
    g.rg->add_pass(
        "read", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(tex);
            b.color(out, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(storage(g.rg)->transientCount == 0);
}

TEST_CASE("RenderGraph - consumer runs after producer across a deduped edge", "[RenderGraph]")
{
    TestGraph g;
    auto a = g.transient("a");
    auto b = g.transient("b");
    auto out = g.transient("out");

    g.rg->add_pass(
        "producer", PassKind::Graphics,
        [&](PassBuilder& pb) {
            pb.color(a, 0);
            pb.color(b, 1);
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "consumer", PassKind::Graphics,
        [&](PassBuilder& pb) {
            pb.sampled(a);
            pb.sampled(b);
            pb.color(out, 0);
            pb.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    // m_passes is the post-cull execution order, so the producer must precede the consumer
    int producerPos = -1, consumerPos = -1, pos = 0;
    for (PassNode* p = storage(g.rg)->m_passes; p; p = p->next, ++pos) {
        if (p->id == make_resource_id("producer"))
            producerPos = pos;
        else if (p->id == make_resource_id("consumer"))
            consumerPos = pos;
    }
    REQUIRE(producerPos >= 0);
    REQUIRE(consumerPos >= 0);
    REQUIRE(producerPos < consumerPos);
}

// record + compile an N-pass linear chain. pass i samples tex[i-1] and writes tex[i], so every edge is a
// real RAW hazard: stresses hazard detection, topo sort and lifetime packing. execute() needs a device
// and is out of scope.
TEST_CASE("RenderGraph - compile perf (linear chain)", "[RenderGraph][!benchmark]")
{
    constexpr int N = 256;

    std::vector<std::string> passNames(N), texNames(N);
    for (int i = 0; i < N; ++i) {
        passNames[i] = "pass" + std::to_string(i);
        texNames[i] = "tex" + std::to_string(i);
    }

    GraphAllocator* allocator = create_allocator();

    auto build_chain = [&](RenderGraph* rg) {
        TextureDesc desc {};
        desc.dimension = WGPUTextureDimension_2D;
        desc.format = WGPUTextureFormat_RGBA8Unorm;
        desc.absolute = { 16, 16, 1 };

        TextureHandle prev {};
        for (int i = 0; i < N; ++i) {
            auto tex = rg->create_transient_texture(texNames[i], desc);
            const bool last = (i == N - 1);
            rg->add_pass(
                passNames[i], PassKind::Graphics,
                [&, i, tex, prev, last](PassBuilder& b) {
                    if (i > 0)
                        b.sampled(prev);
                    b.color(tex, 0);
                    if (last)
                        b.force_keep();
                },
                [](PassContext&) {});
            prev = tex;
        }
    };


    {
        begin_frame(allocator);
        RenderGraph* rg = start_recording(allocator);
        build_chain(rg);
        REQUIRE(rg->compile() == nullptr);
    }

    BENCHMARK("record + compile " + std::to_string(N) + "-pass chain")
    {
        begin_frame(allocator); // per-frame arena reset
        RenderGraph* rg = start_recording(allocator);
        build_chain(rg);
        return rg->compile();
    };

    destroy_allocator(allocator);
}

TEST_CASE("RenderGraph - copy_buffer compiles clean and records the byte range", "[RenderGraph]")
{
    using webgpu::rg::Internal::AccessType;
    using webgpu::rg::Internal::ResourceAccess;

    TestGraph g;
    auto bufA = g.transient_buffer("bufA");
    auto bufB = g.transient_buffer("bufB");

    add_buffer_producer(g.rg, bufA);
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_buffer(bufA, bufB, 0, 4, 4);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    // second declared pass is the transfer pass, check both recorded accesses
    PassNode* copyPass = storage(g.rg)->m_passes->next;
    REQUIRE(copyPass != nullptr);
    const ResourceAccess* src = nullptr;
    const ResourceAccess* dst = nullptr;
    for (uint32_t i = 0; i < copyPass->accessCount; ++i) {
        const ResourceAccess& a = copyPass->accesses[i];
        if (a.type == AccessType::CopySrc)
            src = &a;
        if (a.type == AccessType::CopyDst)
            dst = &a;
    }
    REQUIRE(src != nullptr);
    REQUIRE(dst != nullptr);
    REQUIRE(src->handle.id == bufA.id);
    REQUIRE(src->bufOffset == 0);
    REQUIRE(src->bufSize == 4);
    REQUIRE(dst->handle.id == bufB.id);
    REQUIRE(dst->bufOffset == 4);
    REQUIRE(dst->bufSize == 4);
}

TEST_CASE("RenderGraph - copy_buffer from an unwritten transient is an error", "[RenderGraph]")
{
    TestGraph g;
    auto bufA = g.transient_buffer("bufA");
    auto bufB = g.transient_buffer("bufB");

    // no producer for bufA -> read before write must poison the graph
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_buffer(bufA, bufB);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "that no pass ever writes"));
}

TEST_CASE("RenderGraph - partial copy_buffer dst is not a first-define", "[RenderGraph]")
{
    TestGraph g;
    auto bufA = g.transient_buffer("bufA");
    auto bufB = g.transient_buffer("bufB");

    add_buffer_producer(g.rg, bufA);
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_buffer(bufA, bufB, 0, /*dstOffset*/ 4, /*size*/ 4);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    // bytes outside [4,8) are untouched, so bufB must not be eligible as a full first-define for aliasing
    REQUIRE(webgpu::rg::Internal::find_node(g.rg, bufB)->firstDefines == false);
}

TEST_CASE("RenderGraph - copy_texture compiles clean and records the subresources", "[RenderGraph]")
{
    using webgpu::rg::Internal::AccessType;
    using webgpu::rg::Internal::ResourceAccess;

    TestGraph g;
    auto texA = g.transient("texA");
    auto texB = g.transient("texB", /*mipLevelCount*/ 2);

    g.rg->add_pass(
        "produce", PassKind::Graphics, [&](PassBuilder& b) { b.color(texA, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_texture(texA, texB, {}, { .mip = 1 });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    PassNode* copyPass = storage(g.rg)->m_passes->next;
    REQUIRE(copyPass != nullptr);
    const ResourceAccess* src = nullptr;
    const ResourceAccess* dst = nullptr;
    for (uint32_t i = 0; i < copyPass->accessCount; ++i) {
        const ResourceAccess& a = copyPass->accesses[i];
        if (a.type == AccessType::CopySrc)
            src = &a;
        if (a.type == AccessType::CopyDst)
            dst = &a;
    }
    REQUIRE(src != nullptr);
    REQUIRE(dst != nullptr);
    REQUIRE(src->handle.id == texA.id);
    REQUIRE(src->baseMip == 0);
    REQUIRE(dst->handle.id == texB.id);
    REQUIRE(dst->baseMip == 1);
}

TEST_CASE("RenderGraph - copy_texture between subresources of one texture is legal", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex", /*mipLevelCount*/ 2);

    g.rg->add_pass(
        "produce", PassKind::Graphics, [&](PassBuilder& b) { b.color(tex, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_texture(tex, tex, {}, { .mip = 1 });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

TEST_CASE("RenderGraph - copy_texture_to_buffer dst is never a first-define", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");
    auto buf = g.transient_buffer("buf");

    g.rg->add_pass(
        "produce", PassKind::Graphics, [&](PassBuilder& b) { b.color(tex, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "readback", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_texture_to_buffer(tex, buf);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    // coverage depends on the body's layout, so the aliaser must not treat the dst as fully defined
    REQUIRE(webgpu::rg::Internal::find_node(g.rg, buf)->firstDefines == false);
}

TEST_CASE("RenderGraph - copy_buffer_to_texture write satisfies a later read", "[RenderGraph]")
{
    TestGraph g;
    auto buf = g.transient_buffer("buf");
    auto tex = g.transient("tex");
    auto out = g.transient("out");

    g.rg->add_pass(
        "upload", PassKind::Transfer, [&](PassBuilder& b) { b.host_write(buf); }, [](PassContext&) {});
    g.rg->add_pass(
        "stage", PassKind::Transfer, [&](PassBuilder& b) { b.copy_buffer_to_texture(buf, tex); }, [](PassContext&) {});
    // the copy's CopyDst is tex's writer, so sampling it afterwards is not a read-before-write
    g.rg->add_pass(
        "consume", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(tex);
            b.color(out, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

TEST_CASE("RenderGraph - explicit full-size copy_buffer dst is a first-define", "[RenderGraph]")
{
    TestGraph g;
    auto bufA = g.transient_buffer("bufA", 64);
    auto bufB = g.transient_buffer("bufB", 64);

    add_buffer_producer(g.rg, bufA);
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_buffer(bufA, bufB, 0, 0, /*size*/ 64); // explicit size spanning the whole dst
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(webgpu::rg::Internal::find_node(g.rg, bufB)->firstDefines == true);
}

TEST_CASE("RenderGraph - whole copy_buffer dst is a first-define", "[RenderGraph]")
{
    TestGraph g;
    auto bufA = g.transient_buffer("bufA");
    auto bufB = g.transient_buffer("bufB");

    add_buffer_producer(g.rg, bufA);
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_buffer(bufA, bufB); // offsets 0, size 0 = whole buffer
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(webgpu::rg::Internal::find_node(g.rg, bufB)->firstDefines == true);
}

// no reader, not imported/persistent, no force_keep -> dead
TEST_CASE("RenderGraph - dead pass with no reader is culled", "[RenderGraph]")
{
    TestGraph g;
    auto sink = import_tex(g.rg, "sink");
    auto mid = g.transient("mid");
    auto orphan = g.transient("orphan");

    g.rg->add_pass(
        "writer", PassKind::Graphics, [&](PassBuilder& b) { b.color(mid, 0); }, [](PassContext&) {});
    g.rg->add_pass( // writes orphan, read by nobody -> dead
        "dead", PassKind::Graphics, [&](PassBuilder& b) { b.color(orphan, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "reader", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(mid);
            b.color(sink, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    auto order = pass_order(g.rg);
    REQUIRE(order.size() == 2);
    REQUIRE(idx_of(order, "dead") < 0); // dropped
    REQUIRE(idx_of(order, "writer") >= 0);
    REQUIRE(idx_of(order, "reader") >= 0);
}

// force_keep() rescues a reader-less, sink-less side-effect pass
TEST_CASE("RenderGraph - force_keep rescues an otherwise-dead pass", "[RenderGraph]")
{
    { // baseline: no force_keep -> the pass is dead and culled to nothing
        TestGraph g;
        auto buf = g.transient_buffer("scratch", 256);
        g.rg->add_pass(
            "effect", PassKind::Compute, [&](PassBuilder& b) { b.storage_write(buf); }, [](PassContext&) {});
        REQUIRE(g.rg->compile() == nullptr);
        REQUIRE(pass_order(g.rg).empty());
    }
    { // same pass + force_keep() -> survives
        TestGraph g;
        auto buf = g.transient_buffer("scratch", 256);
        g.rg->add_pass(
            "effect", PassKind::Compute,
            [&](PassBuilder& b) {
                b.storage_write(buf);
                b.force_keep();
            },
            [](PassContext&) {});
        REQUIRE(g.rg->compile() == nullptr);
        REQUIRE(pass_order(g.rg).size() == 1);
    }
}

// WAW is an ordering edge, so the earlier writer survives cull and is scheduled first
TEST_CASE("RenderGraph - write-after-write orders both writers before the reader", "[RenderGraph]")
{
    TestGraph g;
    auto sink = import_tex(g.rg, "sink");
    auto x = g.transient("x");

    g.rg->add_pass(
        "w1", PassKind::Graphics, [&](PassBuilder& b) { b.color(x, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "w2", PassKind::Graphics, [&](PassBuilder& b) { b.color(x, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "reader", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(x);
            b.color(sink, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    auto order = pass_order(g.rg);
    REQUIRE(order.size() == 3);
    REQUIRE(idx_of(order, "w1") < idx_of(order, "w2"));
    REQUIRE(idx_of(order, "w2") < idx_of(order, "reader"));
}

// legal, an imported value comes from outside the frame. unlike a transient read-before-write.
TEST_CASE("RenderGraph - reading an imported resource with no writer is legal", "[RenderGraph]")
{
    TestGraph g;
    auto src = import_tex(g.rg, "src"); // imported, never written by any pass
    auto sink = import_tex(g.rg, "sink");

    g.rg->add_pass(
        "blit", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(src);
            b.color(sink, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(pass_order(g.rg).size() == 1);
}

// a history .curr producer is a cull root, so it survives with no same-frame reader
TEST_CASE("RenderGraph - history .curr write is a cull sink", "[RenderGraph]")
{
    TestGraph g;
    auto hist = g.rg->create_history_texture("hist", tex2d());

    g.rg->add_pass(
        "temporal", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(hist.prev);
            b.color(hist.curr, 0);
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    auto order = pass_order(g.rg);
    REQUIRE(order.size() == 1);
    REQUIRE(idx_of(order, "temporal") >= 0);
}

// a plain persistent write is NOT a sink, it is pool-cached and dependency-driven. contrast
// imported/history.
TEST_CASE("RenderGraph - persistent write with no reader is culled", "[RenderGraph]")
{
    TestGraph g;
    auto pers = g.rg->create_persistent_texture("pers", tex2d());

    g.rg->add_pass(
        "bake", PassKind::Graphics, [&](PassBuilder& b) { b.color(pers, 0); }, [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(pass_order(g.rg).empty());
}

// disjoint lifetimes + identical signatures -> one physical slot
TEST_CASE("RenderGraph - disjoint transients share one physical slot", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;
    using webgpu::rg::Internal::ResourceNode;

    TestGraph g;
    auto sink1 = import_tex(g.rg, "sink1");
    auto sink2 = import_tex(g.rg, "sink2");
    auto t1 = g.transient("t1");
    auto t2 = g.transient("t2");

    g.rg->add_pass(
        "a", PassKind::Graphics, [&](PassBuilder& b) { b.color(t1, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "b", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(t1);
            b.color(sink1, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});
    g.rg->add_pass( // t1 is dead by here, so t2 can reuse its storage
        "c", PassKind::Graphics, [&](PassBuilder& b) { b.color(t2, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "d", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(t2);
            b.color(sink2, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile(true) == nullptr); // enableAlias
    REQUIRE(pass_order(g.rg).size() == 4);
    REQUIRE(storage(g.rg)->m_slotCount == 1);
    ResourceNode* r1 = find_node(g.rg, t1);
    ResourceNode* r2 = find_node(g.rg, t2);
    REQUIRE(r1->aliasSlot != ResourceNode::kNoSlot);
    REQUIRE(r1->aliasSlot == r2->aliasSlot);
}

// aliasing off: no slots built, every transient keeps its own object
TEST_CASE("RenderGraph - aliasing disabled builds no slots", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;
    using webgpu::rg::Internal::ResourceNode;

    TestGraph g;
    auto sink1 = import_tex(g.rg, "sink1");
    auto sink2 = import_tex(g.rg, "sink2");
    auto t1 = g.transient("t1");
    auto t2 = g.transient("t2");

    g.rg->add_pass(
        "a", PassKind::Graphics, [&](PassBuilder& b) { b.color(t1, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "b", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(t1);
            b.color(sink1, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "c", PassKind::Graphics, [&](PassBuilder& b) { b.color(t2, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "d", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(t2);
            b.color(sink2, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile(false) == nullptr); // aliasing OFF
    REQUIRE(storage(g.rg)->m_slotCount == 0);
    REQUIRE(find_node(g.rg, t1)->aliasSlot == ResourceNode::kNoSlot);
    REQUIRE(find_node(g.rg, t2)->aliasSlot == ResourceNode::kNoSlot);
}

// both alive at the consumer -> distinct slots. the packer respects lifetime, not just signature.
TEST_CASE("RenderGraph - overlapping-lifetime transients do not share a slot", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;
    using webgpu::rg::Internal::ResourceNode;

    TestGraph g;
    auto sink = import_tex(g.rg, "sink");
    auto t1 = g.transient("t1");
    auto t2 = g.transient("t2");

    g.rg->add_pass(
        "a", PassKind::Graphics, [&](PassBuilder& b) { b.color(t1, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "b", PassKind::Graphics, [&](PassBuilder& b) { b.color(t2, 0); }, [](PassContext&) {});
    g.rg->add_pass( // reads both -> t1 and t2 alive together here
        "c", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(t1);
            b.sampled(t2);
            b.color(sink, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile(true) == nullptr);
    REQUIRE(storage(g.rg)->m_slotCount == 2);
    ResourceNode* r1 = find_node(g.rg, t1);
    ResourceNode* r2 = find_node(g.rg, t2);
    REQUIRE(r1->aliasSlot != ResourceNode::kNoSlot);
    REQUIRE(r2->aliasSlot != ResourceNode::kNoSlot);
    REQUIRE(r1->aliasSlot != r2->aliasSlot);
}

// greedy reuse past two: three back-to-back disjoint transients collapse onto one slot. force_keep
// gives each a one-pass life.
TEST_CASE("RenderGraph - three disjoint transients collapse onto one slot", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;
    using webgpu::rg::Internal::ResourceNode;

    TestGraph g;
    auto t1 = g.transient("t1");
    auto t2 = g.transient("t2");
    auto t3 = g.transient("t3");

    g.rg->add_pass(
        "k1", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(t1, 0);
            b.force_keep();
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "k2", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(t2, 0);
            b.force_keep();
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "k3", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(t3, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile(true) == nullptr);
    REQUIRE(pass_order(g.rg).size() == 3);
    REQUIRE(storage(g.rg)->m_slotCount == 1); // one physical slot for three transients
    ResourceNode* r1 = find_node(g.rg, t1);
    ResourceNode* r2 = find_node(g.rg, t2);
    ResourceNode* r3 = find_node(g.rg, t3);
    REQUIRE(r1->aliasSlot != ResourceNode::kNoSlot);
    REQUIRE(r1->aliasSlot == r2->aliasSlot);
    REQUIRE(r2->aliasSlot == r3->aliasSlot);
}

// ---------------------------------------------------------------------------------------------------

// sampleCount 4 color + single-sample resolve target. resolve() is a write, so a later
// sampled(resolved) orders after it.
TEST_CASE("RenderGraph - msaa resolve is a write that satisfies a later read", "[RenderGraph]")
{
    using webgpu::rg::Internal::AccessType;
    using webgpu::rg::Internal::find_node;

    TestGraph g;
    TextureDesc msaaDesc {};
    msaaDesc.dimension = WGPUTextureDimension_2D;
    msaaDesc.format = WGPUTextureFormat_RGBA8Unorm;
    msaaDesc.absolute = { 16, 16, 1 };
    msaaDesc.sampleCount = 4;
    auto msaaColor = g.rg->create_transient_texture("msaa.color", msaaDesc);
    auto resolved = g.transient("resolved"); // single-sample, same format+size
    auto out = g.transient("out");

    g.rg->add_pass(
        "forward.msaa", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(msaaColor, 0);
            b.resolve(msaaColor, resolved);
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "present", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(resolved);
            b.color(out, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(find_node(g.rg, msaaColor)->sampleCount == 4);

    // recorded as a ResolveAttachment write on `resolved`
    PassNode* msaaPass = storage(g.rg)->m_passes;
    bool foundResolve = false;
    for (uint32_t i = 0; i < msaaPass->accessCount; ++i)
        if (msaaPass->accesses[i].type == AccessType::ResolveAttachment && msaaPass->accesses[i].handle.id == resolved.id)
            foundResolve = true;
    REQUIRE(foundResolve);

    // present ordered after the resolve
    int msaaPos = -1, presentPos = -1, pos = 0;
    for (PassNode* p = storage(g.rg)->m_passes; p; p = p->next, ++pos) {
        if (p->id == make_resource_id("forward.msaa"))
            msaaPos = pos;
        else if (p->id == make_resource_id("present"))
            presentPos = pos;
    }
    REQUIRE(msaaPos >= 0);
    REQUIRE(presentPos >= 0);
    REQUIRE(msaaPos < presentPos);
}

// the stencil load/store/clear reaches the access, and a read-only reader orders after the mask writer
TEST_CASE("RenderGraph - stencil mask write then read-only test pass", "[RenderGraph]")
{
    using webgpu::rg::Internal::AccessType;

    TestGraph g;
    TextureDesc dsDesc {};
    dsDesc.dimension = WGPUTextureDimension_2D;
    dsDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
    dsDesc.absolute = { 16, 16, 1 };
    auto ds = g.rg->create_transient_texture("mask.ds", dsDesc);
    auto out = g.transient("out");

    g.rg->add_pass(
        "stencil.mask", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.depth_stencil(ds, { .stencilLoad = WGPULoadOp_Clear, .stencilStore = WGPUStoreOp_Store, .stencilClear = 7 });
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "stencil.effect", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.depth_stencil_read_only(ds);
            b.color(out, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    PassNode* maskPass = storage(g.rg)->m_passes;
    const webgpu::rg::Internal::ResourceAccess* dsWrite = nullptr;
    for (uint32_t i = 0; i < maskPass->accessCount; ++i)
        if (maskPass->accesses[i].type == AccessType::DepthStencilAttachment)
            dsWrite = &maskPass->accesses[i];
    REQUIRE(dsWrite != nullptr);
    REQUIRE(dsWrite->stencilLoadOp == WGPULoadOp_Clear);
    REQUIRE(dsWrite->stencilStoreOp == WGPUStoreOp_Store);
    REQUIRE(dsWrite->stencilClear == 7);

    // effect pass reads via DepthStencilReadOnly and runs after the mask write
    int maskPos = -1, effectPos = -1, pos = 0;
    bool foundReadOnly = false;
    for (PassNode* p = storage(g.rg)->m_passes; p; p = p->next, ++pos) {
        if (p->id == make_resource_id("stencil.mask"))
            maskPos = pos;
        if (p->id == make_resource_id("stencil.effect")) {
            effectPos = pos;
            for (uint32_t i = 0; i < p->accessCount; ++i)
                if (p->accesses[i].type == AccessType::DepthStencilReadOnly)
                    foundReadOnly = true;
        }
    }
    REQUIRE(foundReadOnly);
    REQUIRE(maskPos >= 0);
    REQUIRE(effectPos >= 0);
    REQUIRE(maskPos < effectPos);
}

// the declared view shape reaches the access, so ctx.view()/ctx.bind() hand back exactly it
TEST_CASE("RenderGraph - sampled ViewRange is recorded on the access", "[RenderGraph]")
{
    using webgpu::rg::Internal::AccessType;

    TestGraph g;
    TextureDesc desc {};
    desc.dimension = WGPUTextureDimension_2D;
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.absolute = { 16, 16, 6 };
    desc.mipLevelCount = 4;
    auto env = g.rg->create_transient_texture("env", desc);
    auto out = g.transient("out");

    g.rg->add_pass(
        "produce", PassKind::Compute, [&](PassBuilder& b) { b.storage_write(env); }, [](PassContext&) {});
    g.rg->add_pass(
        "consume", PassKind::Graphics,
        [&](PassBuilder& b) {
            // cube view over all 6 layers, mips 1..2, from rendergraph.md Views
            b.sampled(env, { .baseMip = 1, .mipCount = 2, .layerCount = 6, .dim = WGPUTextureViewDimension_Cube });
            b.color(out, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    PassNode* consume = storage(g.rg)->m_passes->next;
    REQUIRE(consume != nullptr);
    const webgpu::rg::Internal::ResourceAccess* read = nullptr;
    for (uint32_t i = 0; i < consume->accessCount; ++i)
        if (consume->accesses[i].type == AccessType::Sampled)
            read = &consume->accesses[i];
    REQUIRE(read != nullptr);
    REQUIRE(read->viewDim == WGPUTextureViewDimension_Cube);
    REQUIRE(read->baseMip == 1);
    REQUIRE(read->mipCount == 2);
    REQUIRE(read->baseLayer == 0);
    REQUIRE(read->layerCount == 6);
}

// per level, sample mip i-1 and render into mip i of the SAME handle. whole-resource hazards serialize
// the chain in level order.
TEST_CASE("RenderGraph - mip chain blit on one handle compiles and serializes", "[RenderGraph]")
{
    constexpr uint32_t kMips = 4;
    TestGraph g;
    auto tex = g.transient("chain", kMips);

    g.rg->add_pass(
        "mip0", PassKind::Graphics, [&](PassBuilder& b) { b.color(tex, 0); },
        [](PassContext&) {});

    std::string names[kMips];
    for (uint32_t mip = 1; mip < kMips; ++mip) {
        names[mip] = "mip" + std::to_string(mip);
        const bool last = (mip == kMips - 1);
        g.rg->add_pass(
            names[mip], PassKind::Graphics,
            [&](PassBuilder& b) {
                b.sampled(tex, { .baseMip = mip - 1 });
                b.color(tex, 0, { .sub = { .mip = mip } });
                if (last)
                    b.force_keep();
            },
            [](PassContext&) {});
    }

    REQUIRE(g.rg->compile() == nullptr);

    // execution order preserves the level order: mip0, mip1, mip2, mip3
    uint32_t pos = 0;
    for (PassNode* p = storage(g.rg)->m_passes; p; p = p->next, ++pos) {
        REQUIRE(pos < kMips);
        std::string expected = "mip" + std::to_string(pos);
        REQUIRE(std::string_view(p->id.name.data, p->id.name.length) == expected);
    }
    REQUIRE(pos == kMips); // all passes survived culling
}

// ---------------------------------------------------------------------------------------------------
// TransientResourcePool supersede-eviction, device-free. destroy() null-guards every GPU release, so
// hand-populated null entries drive end_frame() directly.

using webgpu::rg::Internal::Arena;
using webgpu::rg::Internal::StringInterner;
using webgpu::rg::Internal::SubviewPool;
using webgpu::rg::Internal::TexSignature;
using webgpu::rg::Internal::TransientResourcePool;

// ---------------------------------------------------------------------------------------------------
// StringInterner. a stack arena stands in for the allocator's persist, nothing here needs a device.

struct InternerFixture {
    Arena arena {};
    StringInterner interner {};

    InternerFixture() { interner.arena = &arena; }
    ~InternerFixture() { arena.free_all(); }
};

TEST_CASE("StringInterner - equal strings share one canonical copy", "[RenderGraph]")
{
    InternerFixture f;

    // distinct std::string objects, so a pointer match cannot come from the caller's own storage
    std::string a = "gbuffer.albedo";
    std::string b = "gbuffer.albedo";
    REQUIRE(a.data() != b.data());

    ResourceId ia = f.interner.intern(a);
    ResourceId ib = f.interner.intern(b);

    REQUIRE(ia.name.data == ib.name.data); // one canonical copy
    REQUIRE(ia.value == ib.value);
    REQUIRE(ia.value == webgpu::rg::fnv1a("gbuffer.albedo")); // the id hash is still fnv1a of the name
    REQUIRE(std::string_view(ia.name.data, ia.name.length) == "gbuffer.albedo");
}

TEST_CASE("StringInterner - distinct strings get distinct canonical copies", "[RenderGraph]")
{
    InternerFixture f;
    ResourceId a = f.interner.intern("gbuffer.albedo");
    ResourceId b = f.interner.intern("gbuffer.normal");

    REQUIRE(a.name.data != b.name.data);
    REQUIRE(a.value != b.value);
}

// the pre-fix pool compared names under a 63-char truncation. the interner stores each in full.
TEST_CASE("StringInterner - names differing only past 63 chars stay distinct", "[RenderGraph]")
{
    InternerFixture f;
    const std::string prefix(63, 'x');
    ResourceId a = f.interner.intern(prefix + "alpha");
    ResourceId b = f.interner.intern(prefix + "beta");

    REQUIRE(a.name.data != b.name.data);
    REQUIRE(a.value != b.value);
    REQUIRE(a.name.length == 68);
    REQUIRE(std::string_view(a.name.data, a.name.length) == prefix + "alpha"); // stored in full, not truncated
    REQUIRE(std::string_view(b.name.data, b.name.length) == prefix + "beta");
}

// a default-constructed string_view's null data must not reach memcmp
TEST_CASE("StringInterner - an empty name interns without dereferencing null", "[RenderGraph]")
{
    InternerFixture f;
    ResourceId a = f.interner.intern(std::string_view {});
    ResourceId b = f.interner.intern(""); // same content, different data pointer on the way in

    REQUIRE(a.name.data == b.name.data); // still dedups to one canonical copy
    REQUIRE(a.name.length == 0);
    REQUIRE(a.name.data[0] == '\0');
    REQUIRE(a.name.data != f.interner.intern("nonempty").name.data);
}

TEST_CASE("StringInterner - a name longer than the old inline caps is stored in full", "[RenderGraph]")
{
    InternerFixture f;
    const std::string longName(200, 'q'); // past both the old 64-char pool cap and 48-char profiler cap
    ResourceId id = f.interner.intern(longName);

    REQUIRE(id.name.length == 200);
    REQUIRE(std::string_view(id.name.data, id.name.length) == longName);
    REQUIRE(id.name.data[200] == '\0'); // NUL-terminated, so it is usable as a WebGPU label
}

// the interner never rehashes, which is what makes a canonical pointer safe to keep. fill past kBuckets
// so every bucket chains.
TEST_CASE("StringInterner - canonical pointers stay stable across many interns", "[RenderGraph]")
{
    InternerFixture f;
    ResourceId first = f.interner.intern("first.name");
    const char* firstData = first.name.data;

    std::vector<ResourceId> ids;
    for (int i = 0; i < StringInterner::kBuckets * 8; ++i)
        ids.push_back(f.interner.intern("filler." + std::to_string(i)));

    REQUIRE(f.interner.intern("first.name").name.data == firstData); // same node, no relocation
    REQUIRE(std::string_view(firstData, first.name.length) == "first.name"); // bytes still intact

    // every filler is still its own canonical copy and re-interns to the same pointer
    for (size_t i = 0; i < ids.size(); ++i) {
        ResourceId again = f.interner.intern("filler." + std::to_string(i));
        REQUIRE(again.name.data == ids[i].name.data);
    }
}

// live-cell count for either pool's intrusive entry list
template <class E> static uint32_t list_count(const E* head)
{
    uint32_t n = 0;
    for (const E* e = head; e; e = e->next)
        ++n;
    return n;
}

// the minimum end_frame()/superseded_by need without a device: cells from `arena`, and a `subviewPool`
// for destroy() to recycle the (always null here) subview list into.
struct PoolFixture {
    Arena arena {};
    SubviewPool subviewPool {};
    TransientResourcePool pool {};

    PoolFixture()
    {
        subviewPool.arena = &arena;
        pool.arena = &arena;
        pool.subviewPool = &subviewPool;
    }
    // drain the pool BEFORE freeing the arena its cells live in. a destructor body runs before its members
    // are destroyed, so a bare free_all() would leave ~TransientResourcePool() walking freed memory.
    ~PoolFixture()
    {
        pool.destroy_all();
        arena.free_all();
    }

    // appends rather than prepends, so list order matches declaration order for the positional reads
    TransientResourcePool::Entry* push(const TransientResourcePool::Entry& proto)
    {
        TransientResourcePool::Entry* e = pool.alloc_entry();
        TransientResourcePool::Entry* next = e->next;
        *e = proto;
        e->next = next;
        TransientResourcePool::Entry** pp = &pool.entries;
        while (*pp)
            pp = &(*pp)->next;
        *pp = e;
        e->next = nullptr;
        return e;
    }

    uint32_t count() const { return list_count(pool.entries); }

    // nth live entry, for the positional checks
    const TransientResourcePool::Entry* at(uint32_t i) const
    {
        const TransientResourcePool::Entry* e = pool.entries;
        for (uint32_t k = 0; e && k < i; ++k)
            e = e->next;
        return e;
    }
};

// stand-ins for the interned name pointers acquire passes as an entry's supersede identity. only the
// address matters, superseded_by compares pointers.
static const char kIdA[] = "resourceA";
static const char kIdB[] = "resourceB";

// GPU handles left null so destroy() is a no-op. identity defaults to kIdA, so a test that does not care
// gets two entries of the SAME logical resource, i.e. the resize case.
static TransientResourcePool::Entry tex_entry(
    WGPUExtent3D size, uint64_t createdFrame, uint64_t lastUsedFrame, const void* identity = kIdA)
{
    TransientResourcePool::Entry e {};
    e.kind = ResourceKind::Texture;
    e.sig.size = size;
    e.sig.format = WGPUTextureFormat_RGBA8Unorm;
    e.sig.dim = WGPUTextureDimension_2D;
    e.sig.mipLevelCount = 1;
    e.sig.sampleCount = 1;
    e.usage = (WGPUTextureUsage)(WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding);
    e.createdFrame = createdFrame;
    e.lastUsedFrame = lastUsedFrame;
    e.identity = identity;
    return e;
}

TEST_CASE("TransientResourcePool - old-size idle texture is superseded by a new-size sibling", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 9)); // old size, idle this frame
    f.push(tex_entry({ 1024, 768, 1 }, /*created*/ 10, /*lastUsed*/ 10)); // new size, created this frame

    f.pool.end_frame();

    // the old-size entry is evicted at end_frame, before kRetain (frame - 9 == 1 < 4) would fire.
    REQUIRE(f.count() == 1);
    REQUIRE(f.at(0)->sig.size.width == 1024);
}

TEST_CASE("TransientResourcePool - texture claimed this frame is not superseded", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 10)); // used THIS frame -> live
    f.push(tex_entry({ 1024, 768, 1 }, /*created*/ 10, /*lastUsed*/ 10));

    f.pool.end_frame();

    REQUIRE(f.count() == 2); // both kept
}

TEST_CASE("TransientResourcePool - descriptor mismatch does not supersede", "[RenderGraph]")
{
    auto survives = [](auto mutate) {
        PoolFixture f;
        f.pool.frame = 10;
        f.pool.createdThisFrame = 1;
        f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 9)); // old, idle
        TransientResourcePool::Entry sibling = tex_entry({ 1024, 768, 1 }, /*created*/ 10, /*lastUsed*/ 10);
        mutate(sibling);
        f.push(sibling);
        f.pool.end_frame();
        // old entry not superseded -> both survive (frame - 9 == 1 < kRetain, so kRetain keeps it too)
        return f.count() == 2;
    };

    REQUIRE(survives([](TransientResourcePool::Entry& s) { s.sig.format = WGPUTextureFormat_BGRA8Unorm; }));
    REQUIRE(survives([](TransientResourcePool::Entry& s) { s.sig.dim = WGPUTextureDimension_3D; }));
    REQUIRE(survives([](TransientResourcePool::Entry& s) { s.sig.mipLevelCount = 4; }));
    REQUIRE(survives([](TransientResourcePool::Entry& s) { s.sig.sampleCount = 4; }));
    // a differently-purposed sibling must not sweep the old entry: STORAGE does not cover SAMPLED+ATTACHMENT
    REQUIRE(survives([](TransientResourcePool::Entry& s) { s.usage = WGPUTextureUsage_StorageBinding; }));
}

TEST_CASE("TransientResourcePool - sibling created in an earlier frame does not supersede", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 9)); // old, idle
    f.push(tex_entry({ 1024, 768, 1 }, /*created*/ 8, /*lastUsed*/ 9)); // different size but NOT this frame

    f.pool.end_frame();

    REQUIRE(f.count() == 2); // no supersede; kRetain keeps both (idle only 1 frame)
}

// a resize that also flips a usage bit is still a resize. usage matches by superset, so the added
// STORAGE bit does not hide the supersede.
TEST_CASE("TransientResourcePool - a wider-usage new-size sibling supersedes the old extent", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 9)); // old size, idle
    TransientResourcePool::Entry sibling = tex_entry({ 1024, 768, 1 }, /*created*/ 10, /*lastUsed*/ 10);
    sibling.usage = (WGPUTextureUsage)(sibling.usage | WGPUTextureUsage_StorageBinding); // superset of the old entry's
    f.push(sibling);

    f.pool.end_frame();

    REQUIRE(f.count() == 1);
    REQUIRE(f.at(0)->sig.size.width == 1024);
}

// GPU handles null so destroy() is a no-op. identity defaults to kIdA, see tex_entry.
static TransientResourcePool::Entry buf_entry(
    uint64_t size, WGPUBufferUsage usage, uint64_t createdFrame, uint64_t lastUsedFrame, const void* identity = kIdA)
{
    TransientResourcePool::Entry e {};
    e.kind = ResourceKind::Buffer;
    e.bufferSize = size;
    e.bufUsage = usage;
    e.createdFrame = createdFrame;
    e.lastUsedFrame = lastUsedFrame;
    e.identity = identity;
    return e;
}

// the buffer arm. pre-patch this short-circuited on non-textures, so a resize drag stacked up to
// kRetain generations of screen-derived buffers.
TEST_CASE("TransientResourcePool - a resized idle buffer is superseded by its new-size sibling", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(buf_entry(256, WGPUBufferUsage_Storage, /*created*/ 3, /*lastUsed*/ 9)); // old size, idle
    f.push(buf_entry(512, (WGPUBufferUsage)(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst), /*created*/ 10, /*lastUsed*/ 10));

    f.pool.end_frame();

    REQUIRE(f.count() == 1); // collapsed same-frame, not after kRetain
    REQUIRE(f.at(0)->bufferSize == 512);
}

TEST_CASE("TransientResourcePool - a buffer whose usage the sibling does not cover is not superseded", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(buf_entry(256, (WGPUBufferUsage)(WGPUBufferUsage_Storage | WGPUBufferUsage_Indirect), /*created*/ 3, /*lastUsed*/ 9));
    f.push(buf_entry(512, WGPUBufferUsage_Storage, /*created*/ 10, /*lastUsed*/ 10)); // misses Indirect

    f.pool.end_frame();

    REQUIRE(f.count() == 2); // differently-purposed, so not swept
}

TEST_CASE("TransientResourcePool - a buffer claimed this frame is not superseded", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(buf_entry(256, WGPUBufferUsage_Storage, /*created*/ 3, /*lastUsed*/ 10)); // used THIS frame -> live
    f.push(buf_entry(512, WGPUBufferUsage_Storage, /*created*/ 10, /*lastUsed*/ 10));

    f.pool.end_frame();

    REQUIRE(f.count() == 2); // the lastUsedFrame >= frame guard holds for buffers too
}

TEST_CASE("TransientResourcePool - a buffer sibling created in an earlier frame does not supersede", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(buf_entry(256, WGPUBufferUsage_Storage, /*created*/ 3, /*lastUsed*/ 9));
    f.push(buf_entry(512, WGPUBufferUsage_Storage, /*created*/ 8, /*lastUsed*/ 9)); // not this frame's replacement

    f.pool.end_frame();

    REQUIRE(f.count() == 2);
}

// a texture and a buffer never supersede each other, whatever their sizes
TEST_CASE("TransientResourcePool - kinds do not supersede across each other", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 9));
    f.push(buf_entry(512, WGPUBufferUsage_Storage, /*created*/ 10, /*lastUsed*/ 10));

    f.pool.end_frame();

    REQUIRE(f.count() == 2);
}

TEST_CASE("TransientResourcePool - same-size buffers are not superseded", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;

    f.push(buf_entry(256, WGPUBufferUsage_Storage, /*created*/ 3, /*lastUsed*/ 9));
    // same size -> not a resize, so not a supersede
    f.push(buf_entry(256, (WGPUBufferUsage)(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst), /*created*/ 10, /*lastUsed*/ 10));

    f.pool.end_frame();

    REQUIRE(f.count() == 2);
}

TEST_CASE("TransientResourcePool - drag resize keeps only the newest generation each frame", "[RenderGraph]")
{
    PoolFixture f;

    // simulate three consecutive frames, each acquiring a fresh, larger size while the previous
    // generation goes idle. after each end_frame() only the size just created must remain.
    const WGPUExtent3D sizes[3] = { { 800, 600, 1 }, { 900, 675, 1 }, { 1000, 750, 1 } };
    for (int i = 0; i < 3; ++i) {
        // this frame's new-size entry, claimed this frame
        f.push(tex_entry(sizes[i], /*created*/ f.pool.frame, /*lastUsed*/ f.pool.frame));
        f.pool.createdThisFrame = 1;

        f.pool.end_frame(); // supersede-evicts any older-size idle generation, then advances frame

        REQUIRE(f.count() == 1);
        REQUIRE(f.at(0)->sig.size.width == sizes[i].width);

        // going into the next frame the surviving entry is now idle (not touched next frame until acquired)
    }
}

TEST_CASE("TransientResourcePool - two idle generations are both swept by one new-size sibling", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    // two older generations idle at once, which the drag-resize test never produces. both well inside
    // kRetain, so only supersede can take them: end_frame's scan clears the whole backlog in one pass.
    f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 8));
    f.push(tex_entry({ 900, 675, 1 }, /*created*/ 8, /*lastUsed*/ 9));
    f.push(tex_entry({ 1024, 768, 1 }, /*created*/ 10, /*lastUsed*/ 10));

    f.pool.end_frame();

    REQUIRE(f.count() == 1);
    REQUIRE(f.at(0)->sig.size.width == 1024);
}

TEST_CASE("TransientResourcePool - the event log records the supersede evict", "[RenderGraph]")
{
    PoolFixture f;
    f.pool.frame = 10;
    f.pool.createdThisFrame = 1;
    f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 9));
    f.push(tex_entry({ 1024, 768, 1 }, /*created*/ 10, /*lastUsed*/ 10));
    f.pool.log_reset();

    f.pool.end_frame();

    // the surviving counts cannot say WHEN an eviction fired, the log can. only Evict is visible here,
    // Create is logged by acquire, which needs a device.
    REQUIRE(f.pool.eventCount == 1);
    REQUIRE(f.pool.eventLog[0].event == TransientResourcePool::Event::Evict);
    REQUIRE(f.pool.eventLog[0].kind == ResourceKind::Texture);
    REQUIRE(f.pool.eventLog[0].frame == 10); // logged before end_frame advances the clock
    REQUIRE(f.pool.eventLog[0].size.width == 800); // the old generation, not the survivor
    REQUIRE(f.pool.eventLog[0].size.height == 600);
}

// ---------------------------------------------------------------------------------------------------
// supersede must not fire across two DIFFERENT resources that merely differ in size. the pool matches by
// descriptor, so without an identity key an idle resource looks like a stale generation of any
// same-shaped sibling created this frame.

// claim the pooled buffer matching size+usage, else create what acquire's miss path would. identity
// tracks the LAST claimant, since an entry may legitimately change hands. true if this created one.
static bool claim_or_create_buf(PoolFixture& f, uint64_t size, WGPUBufferUsage usage, const void* identity)
{
    for (TransientResourcePool::Entry* e = f.pool.entries; e; e = e->next)
        if (e->kind == ResourceKind::Buffer && !e->inUse && e->bufferSize == size && e->bufUsage == usage) {
            e->lastUsedFrame = f.pool.frame;
            e->identity = identity;
            return false;
        }
    f.push(buf_entry(size, usage, /*created*/ f.pool.frame, /*lastUsed*/ f.pool.frame, identity));
    ++f.pool.createdThisFrame;
    return true;
}

// tex_entry's counterpart to claim_or_create_buf
static bool claim_or_create_tex(PoolFixture& f, WGPUExtent3D size, const void* identity)
{
    TransientResourcePool::Entry want = tex_entry(size, 0, 0, identity);
    for (TransientResourcePool::Entry* e = f.pool.entries; e; e = e->next)
        if (e->kind == ResourceKind::Texture && !e->inUse && e->sig == want.sig && e->usage == want.usage) {
            e->lastUsedFrame = f.pool.frame;
            e->identity = identity;
            return false;
        }
    f.push(tex_entry(size, /*created*/ f.pool.frame, /*lastUsed*/ f.pool.frame, identity));
    ++f.pool.createdThisFrame;
    return true;
}

TEST_CASE("TransientResourcePool - alternately claimed buffers of different sizes do not churn", "[RenderGraph]")
{
    PoolFixture f;

    // two unrelated buffers, same usage, different sizes, each claimed every OTHER frame. neither is a
    // resize of the other, and each is idle only 1 frame at a time, so both survive untouched.
    uint32_t creates = 0;
    for (uint64_t i = 0; i < 6; ++i) {
        if (i % 2 == 0)
            creates += claim_or_create_buf(f, 256, WGPUBufferUsage_Storage, kIdA);
        else
            creates += claim_or_create_buf(f, 512, WGPUBufferUsage_Storage, kIdB);
        f.pool.release_claims();
        f.pool.end_frame();
    }

    REQUIRE(creates == 2); // one per resource, on its first claim, never recreated
    REQUIRE(f.count() == 2);
    REQUIRE(f.pool.eventCount == 0); // no evict ever logged
}

TEST_CASE("TransientResourcePool - alternately claimed textures of different extents do not churn", "[RenderGraph]")
{
    PoolFixture f;

    // the texture arm of the same hole, differing only in extent
    uint32_t creates = 0;
    for (uint64_t i = 0; i < 6; ++i) {
        if (i % 2 == 0)
            creates += claim_or_create_tex(f, { 800, 600, 1 }, kIdA);
        else
            creates += claim_or_create_tex(f, { 400, 300, 1 }, kIdB);
        f.pool.release_claims();
        f.pool.end_frame();
    }

    REQUIRE(creates == 2);
    REQUIRE(f.count() == 2);
    REQUIRE(f.pool.eventCount == 0);
}

TEST_CASE("TransientResourcePool - a size-only difference across identities does not supersede", "[RenderGraph]")
{
    // the identity bail alone, without the alternating-frame timing above. same shape as "a resized idle
    // buffer is superseded by its new-size sibling", only the identity differs.
    {
        PoolFixture f;
        f.pool.frame = 10;
        f.pool.createdThisFrame = 1;
        f.push(buf_entry(256, WGPUBufferUsage_Storage, /*created*/ 3, /*lastUsed*/ 9, kIdA));
        f.push(buf_entry(512, WGPUBufferUsage_Storage, /*created*/ 10, /*lastUsed*/ 10, kIdB));

        f.pool.end_frame();

        REQUIRE(f.count() == 2);
    }
    {
        PoolFixture f;
        f.pool.frame = 10;
        f.pool.createdThisFrame = 1;
        f.push(tex_entry({ 800, 600, 1 }, /*created*/ 3, /*lastUsed*/ 9, kIdA));
        f.push(tex_entry({ 1024, 768, 1 }, /*created*/ 10, /*lastUsed*/ 10, kIdB));

        f.pool.end_frame();

        REQUIRE(f.count() == 2);
    }
}

TEST_CASE("TransientResourcePool - different-size buffers claimed every frame are never superseded", "[RenderGraph]")
{
    PoolFixture f;

    // the common case, and the guard that the identity key does not regress it. the lastUsedFrame >=
    // frame bail already covers this, with or without identity.
    uint32_t creates = 0;
    for (uint64_t i = 0; i < 6; ++i) {
        creates += claim_or_create_buf(f, 256, WGPUBufferUsage_Storage, kIdA);
        creates += claim_or_create_buf(f, 512, WGPUBufferUsage_Storage, kIdB);
        f.pool.release_claims();
        f.pool.end_frame();
    }

    REQUIRE(creates == 2);
    REQUIRE(f.count() == 2);
}

// ---------------------------------------------------------------------------------------------------
// transient buffer acquire matching. a real device, so the miss branch actually creates and
// reused-vs-created is observable as an entry count.

TEST_CASE("TransientResourcePool - a usage-superset idle buffer is reused", "[RenderGraph][gpu]")
{
    UnittestWebgpuContext gpu;
    PoolFixture f;

    // seed the pool the way a previous frame would have: one idle Storage|CopySrc buffer
    WGPUBuffer first = nullptr;
    f.pool.acquire(gpu.device, 256, (WGPUBufferUsage)(WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc), kIdA, first);
    REQUIRE(first != nullptr);
    REQUIRE(f.count() == 1);
    f.pool.release_claims();

    // a narrower request is covered by that buffer's usage -> reuse the same object, no second entry
    WGPUBuffer second = nullptr;
    f.pool.acquire(gpu.device, 256, WGPUBufferUsage_Storage, kIdA, second);
    REQUIRE(second == first);
    REQUIRE(f.count() == 1);

    f.pool.destroy_all();
}

TEST_CASE("TransientResourcePool - a buffer whose usage misses a requested bit is not reused", "[RenderGraph][gpu]")
{
    UnittestWebgpuContext gpu;
    PoolFixture f;

    WGPUBuffer first = nullptr;
    f.pool.acquire(gpu.device, 256, WGPUBufferUsage_Storage, kIdA, first);
    f.pool.release_claims();

    // storage does not cover Storage|Indirect -> a fresh object, since usage matches by superset
    WGPUBuffer second = nullptr;
    f.pool.acquire(gpu.device, 256, (WGPUBufferUsage)(WGPUBufferUsage_Storage | WGPUBufferUsage_Indirect), kIdA, second);
    REQUIRE(second != first);
    REQUIRE(f.count() == 2);

    f.pool.destroy_all();
}

// identity must follow the claimant, or the original creator's resize sweeps an object that now belongs
// to someone else. needs a real acquire, the device-free fixtures stamp identity themselves.

TEST_CASE("TransientResourcePool - a rehomed buffer is not swept by its creator's resize", "[RenderGraph][gpu]")
{
    UnittestWebgpuContext gpu;
    PoolFixture f;

    // frame 0: A creates a 256B object
    WGPUBuffer a0 = nullptr;
    f.pool.acquire(gpu.device, 256, WGPUBufferUsage_Storage, kIdA, a0);
    f.pool.release_claims();
    f.pool.end_frame();

    // frame 1: A does not record, B asks for the same descriptor and is handed A's object
    WGPUBuffer b1 = nullptr;
    f.pool.acquire(gpu.device, 256, WGPUBufferUsage_Storage, kIdB, b1);
    REQUIRE(b1 == a0); // descriptor match -> reused, the object now serves B
    f.pool.release_claims();
    f.pool.end_frame();

    // frame 2: B is idle, A returns resized. a's new object must not sweep the one B is still using.
    WGPUBuffer a2 = nullptr;
    f.pool.acquire(gpu.device, 512, WGPUBufferUsage_Storage, kIdA, a2);
    f.pool.release_claims();
    f.pool.end_frame();

    REQUIRE(f.count() == 2); // b's object survived A's resize
}

TEST_CASE("TransientResourcePool - a rehomed texture is not swept by its creator's resize", "[RenderGraph][gpu]")
{
    UnittestWebgpuContext gpu;
    PoolFixture f;

    const TransientResourcePool::Entry small = tex_entry({ 16, 16, 1 }, 0, 0);
    const TransientResourcePool::Entry large = tex_entry({ 32, 32, 1 }, 0, 0);

    WGPUTexture a0 = nullptr, a2 = nullptr, b1 = nullptr;
    WGPUTextureView v = nullptr;

    f.pool.acquire(gpu.device, small.sig, small.usage, kIdA, a0, v);
    f.pool.release_claims();
    f.pool.end_frame();

    f.pool.acquire(gpu.device, small.sig, small.usage, kIdB, b1, v);
    REQUIRE(b1 == a0);
    f.pool.release_claims();
    f.pool.end_frame();

    f.pool.acquire(gpu.device, large.sig, large.usage, kIdA, a2, v);
    f.pool.release_claims();
    f.pool.end_frame();

    REQUIRE(f.count() == 2);
}

// the removed MapRead|MapWrite exact-match reservation. the graph never gives a transient buffer those
// bits, so it only ever blocked reuse of a state the graph cannot produce.
TEST_CASE("TransientResourcePool - a map-bit-carrying idle buffer no longer blocks superset reuse", "[RenderGraph][gpu]")
{
    UnittestWebgpuContext gpu;
    PoolFixture f;

    WGPUBuffer seeded = nullptr;
    f.pool.acquire(gpu.device, 256, (WGPUBufferUsage)(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst), kIdA, seeded);
    f.pool.release_claims();
    // stamp the map bit on rather than asking WebGPU for the invalid Storage|MapRead combination. only
    // the flags matter, acquire compares bufUsage and never the object.
    f.pool.entries->bufUsage
        = (WGPUBufferUsage)(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead);

    // pre-patch the MapRead bit made this an exact-match miss and forced a second object
    WGPUBuffer plain = nullptr;
    f.pool.acquire(gpu.device, 256, WGPUBufferUsage_Storage, kIdA, plain);
    REQUIRE(plain == seeded);
    REQUIRE(f.count() == 1);

    f.pool.destroy_all();
}

// ===================================================================================================
// execute-level tests. everything above stops at compile(), these run realize_graph() + execute() against
// a real Dawn device and drive the cross-frame pools over whole frames on one long-lived allocator.
//
// NOTE: release_resources() nulls a transient's realized handles at the end of execute(), so white-box
// handle checks must capture inside a pass body via ctx.*. the pools persist and are read after frame().

using webgpu::rg::Internal::PersistentResourcePool;
using webgpu::rg::Internal::ResourceNode;
using webgpu::rg::Internal::find_node;

struct ExecGraph {
    UnittestWebgpuContext gpu;
    GraphAllocator* allocator = create_allocator();

    ~ExecGraph() { destroy_allocator(allocator); }
    ExecGraph(const ExecGraph&) = delete;
    ExecGraph& operator=(const ExecGraph&) = delete;
    ExecGraph() = default;

    // block until the queue drains, so a readback or pool inspection sees finished work
    void wait_idle()
    {
        bool done = false;
        WGPUQueueWorkDoneCallbackInfo cb {
            .nextInChain = nullptr,
            .mode = WGPUCallbackMode_AllowProcessEvents,
            .callback = [](WGPUQueueWorkDoneStatus, WGPUStringView, void* d, void*) { *reinterpret_cast<bool*>(d) = true; },
            .userdata1 = &done,
            .userdata2 = nullptr,
        };
        WGPUFuture f = wgpuQueueOnSubmittedWorkDone(gpu.queue, cb);
        WGPUFutureWaitInfo wi { .future = f, .completed = false };
        REQUIRE(wgpuInstanceWaitAny(gpu.instance, 1, &wi, 1000ull * 1000 * 1000) == WGPUWaitStatus_Success);
    }

    // compile + execute + submit one already-recorded graph on its own command buffer
    void submit_graph(RenderGraph* rg)
    {
        REQUIRE(rg->compile() == nullptr);

        WGPUCommandEncoderDescriptor ed {};
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, &ed);
        rg->execute(gpu.device, gpu.queue, enc);
        WGPUCommandBufferDescriptor cd {};
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, &cd);
        wgpuQueueSubmit(gpu.queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(enc);
    }

    // one full frame: record -> compile -> execute -> submit -> wait -> inspect -> end_frame
    template <class Record, class Inspect>
    void frame(Record&& record, Inspect&& inspect)
    {
        begin_frame(allocator);
        RenderGraph* rg = start_recording(allocator);
        record(rg);
        submit_graph(rg);
        wait_idle();
        inspect(rg);
        end_frame(allocator);
    }

    template <class Record>
    void frame(Record&& record)
    {
        frame(std::forward<Record>(record), [](RenderGraph*) {});
    }
};

// texture->buffer readback into an imported `dst`. rows are 256-aligned, so buf size is alignedRow*H.
static void encode_readback(PassContext& ctx, TextureHandle src, BufferHandle dst, uint32_t w, uint32_t h)
{
    WGPUTexelCopyBufferLayout layout { .offset = 0, .bytesPerRow = webgpu::rg::aligned_bytes_per_row(w, 4), .rowsPerImage = h };
    auto s = ctx.copy_src_info(src);
    auto d = ctx.copy_dst_buffer(dst, layout);
    auto sz = ctx.copy_extent_src(src);
    wgpuCommandEncoderCopyTextureToBuffer(ctx.encoder, &s, &d, &sz);
}

// smoke: clear a transient, copy it into an imported mappable buffer, read the pixel back. exercises
// transient acquire, attachment build, pass ordering and the copy family end to end.
TEST_CASE("RenderGraph exec - clear then readback returns the clear color", "[RenderGraph][gpu]")
{
    constexpr uint32_t W = 16, H = 16;
    const uint32_t alignedRow = webgpu::rg::aligned_bytes_per_row(W, 4);

    ExecGraph g;
    // imported readback target, MapRead so the host can read it directly after submit
    webgpu::raii::RawBuffer<uint8_t> readback(g.gpu.device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, alignedRow * H, "rg readback");

    g.frame([&](RenderGraph* rg) {
        auto tex = rg->create_transient_texture("smoke.tex", [] {
            TextureDesc d {};
            d.dimension = WGPUTextureDimension_2D;
            d.format = WGPUTextureFormat_RGBA8Unorm;
            d.absolute = { W, H, 1 };
            return d;
        }());
        auto buf = rg->import_buffer("smoke.readback", readback.handle());

        rg->add_pass(
            "clear", PassKind::Graphics,
            [&](PassBuilder& b) { b.color(tex, 0, { .clear = { 1.0, 0.0, 0.0, 1.0 } }); }, [](PassContext&) {});
        rg->add_pass(
            "readback", PassKind::Transfer,
            [&](PassBuilder& b) {
                b.copy_texture_to_buffer(tex, buf);
                b.force_keep();
            },
            [=](PassContext& ctx) { encode_readback(ctx, tex, buf, W, H); });
    });

    std::vector<uint8_t> pixels;
    REQUIRE(readback.read_back_sync(g.gpu.instance, g.gpu.device, pixels) == WGPUMapAsyncStatus_Success);
    REQUIRE(pixels.size() == alignedRow * H);
    // pixel (0,0) = first RGBA8 texel = the clear color (1,0,0,1) -> 255,0,0,255
    REQUIRE(pixels[0] == 255);
    REQUIRE(pixels[1] == 0);
    REQUIRE(pixels[2] == 0);
    REQUIRE(pixels[3] == 255);
}

// the ping-pong pass shared by the history tests. .curr is a cull sink, so it survives without
// force_keep. `body` rides the arena exec slot, so it must be trivially destructible.
template <class Body>
static void add_temporal_pass(RenderGraph* rg, HistoryTexture h, Body body)
{
    rg->add_pass(
        "temporal", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(h.prev);
            b.color(h.curr, 0);
        },
        body);
}

// over two frames the pair ping-pongs its two physical textures, and history_valid(.prev) is false on
// the first frame but true afterwards
TEST_CASE("RenderGraph exec - history ping-pongs and history_valid trips only after the first frame", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTexture curr[2] = {}, prev[2] = {};
    bool validPrev[2] = {};

    for (int f = 0; f < 2; ++f) {
        WGPUTexture* co = &curr[f];
        WGPUTexture* po = &prev[f];
        bool* vo = &validPrev[f];
        g.frame([&](RenderGraph* rg) {
            auto h = rg->create_history_texture("hist", tex2d());
            add_temporal_pass(rg, h, [h, co, po, vo](PassContext& ctx) {
                *co = ctx.texture(h.curr);
                *po = ctx.texture(h.prev);
                *vo = ctx.history_valid(h);
            });
        });
    }

    REQUIRE(curr[0] != nullptr);
    REQUIRE(prev[0] != nullptr);
    REQUIRE(curr[0] != prev[0]); // two distinct physical textures
    REQUIRE(curr[1] == prev[0]); // ping-pong: last frame's curr is this frame's prev
    REQUIRE(prev[1] == curr[0]);
    REQUIRE(validPrev[0] == false); // frame 0: .prev never written
    REQUIRE(validPrev[1] == true); // frame 1: .prev holds frame 0's .curr
}

// a changed hash recreates the entry, zeroing .prev, so history_valid drops to false that frame
TEST_CASE("RenderGraph exec - a changed history hash invalidates .prev", "[RenderGraph][gpu]")
{
    ExecGraph g;
    bool validPrev[3] = {};
    const uint64_t hashes[3] = { 111, 111, 222 }; // frame 2 changes the settings hash

    for (int f = 0; f < 3; ++f) {
        bool* vo = &validPrev[f];
        const uint64_t hash = hashes[f];
        g.frame([&](RenderGraph* rg) {
            auto h = rg->create_history_texture("hist", tex2d(), hash);
            add_temporal_pass(rg, h, [h, vo](PassContext& ctx) { *vo = ctx.history_valid(h); });
        });
    }

    REQUIRE(validPrev[0] == false); // first use
    REQUIRE(validPrev[1] == true); // steady state
    REQUIRE(validPrev[2] == false); // hash change -> recreate -> prev invalid
}

// an untouched entry is freed after kRetain frames, so its textures do not leak for the process
// lifetime once a feature goes inactive
TEST_CASE("RenderGraph exec - an untouched history entry is evicted after kRetain frames", "[RenderGraph][gpu]")
{
    ExecGraph g;
    g.frame([&](RenderGraph* rg) {
        auto h = rg->create_history_texture("hist", tex2d());
        add_temporal_pass(rg, h, [](PassContext&) {});
    });
    REQUIRE(list_count(g.allocator->pool.entries) == 1);

    for (uint32_t i = 0; i < PersistentResourcePool::kRetain; ++i)
        g.frame([](RenderGraph*) {}); // empty graph: entry goes untouched
    REQUIRE((g.allocator->pool.entries == nullptr));
}

// the entry holds the interned view, not an inline char[64], so a name past the old 63-char cap
// survives whole
TEST_CASE("RenderGraph exec - a pool entry keeps a long name in full", "[RenderGraph][gpu]")
{
    ExecGraph g;
    const std::string longName(120, 'z');

    g.frame([&](RenderGraph* rg) {
        auto h = rg->create_history_texture(longName, tex2d());
        add_temporal_pass(rg, h, [](PassContext&) {});
    });

    PersistentResourcePool::Entry* e = g.allocator->pool.entries;
    REQUIRE(e != nullptr);
    REQUIRE(e->name_view().length == 120);
    REQUIRE(std::string_view(e->name_view().data, e->name_view().length) == longName);
}

// the entry stores the canonical pointer, which find() keys on, so a re-declared name resolves to the
// same entry rather than a second one
TEST_CASE("RenderGraph exec - a re-declared name reuses its pool entry across frames", "[RenderGraph][gpu]")
{
    ExecGraph g;
    auto record = [](RenderGraph* rg) {
        auto h = rg->create_history_texture("hist.reused", tex2d());
        add_temporal_pass(rg, h, [](PassContext&) {});
    };

    g.frame(record);
    REQUIRE(list_count(g.allocator->pool.entries) == 1);
    const char* firstName = g.allocator->pool.entries->name_view().data;

    g.frame(record); // same name next frame -> the same canonical pointer -> the same entry
    REQUIRE(list_count(g.allocator->pool.entries) == 1);
    REQUIRE(g.allocator->pool.entries->name_view().data == firstName);
}

// ---------------------------------------------------------------------------------------------------
// TransientResourcePool, device-backed. drives acquire()/release_claims across real frames, where the
// tests above are device-free.

using webgpu::rg::Internal::TransientResourcePool;

// StoreOp_Store keeps it out of the memoryless path, so its usage bits are stable frame to frame
TEST_CASE("RenderGraph exec - a same-descriptor transient is reused across frames", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTexture seen[2] = {};

    for (int f = 0; f < 2; ++f) {
        WGPUTexture* out = &seen[f];
        g.frame([&](RenderGraph* rg) {
            auto tex = rg->create_transient_texture("t", tex2d());
            rg->add_pass(
                "w", PassKind::Graphics,
                [&](PassBuilder& b) {
                    b.color(tex, 0);
                    b.force_keep();
                },
                [tex, out](PassContext& ctx) { *out = ctx.texture(tex); });
        });
    }

    REQUIRE(seen[0] != nullptr);
    REQUIRE(seen[0] == seen[1]); // same pooled object handed back
    REQUIRE(list_count(g.allocator->transient.entries) == 1); // not reallocated
}

// superset-reuse: frame 0 gives RenderAttachment|CopySrc, frame 1 asks for RenderAttachment alone
TEST_CASE("RenderGraph exec - a wider-usage pooled transient satisfies a narrower request", "[RenderGraph][gpu]")
{
    constexpr uint32_t W = 16, H = 16;
    const uint32_t alignedRow = webgpu::rg::aligned_bytes_per_row(W, 4);

    ExecGraph g;
    webgpu::raii::RawBuffer<uint8_t> readback(g.gpu.device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, alignedRow * H, "superset readback");
    WGPUTexture seen[2] = {};

    // frame 0: color() + copy_texture_to_buffer -> usage RenderAttachment|CopySrc
    g.frame([&](RenderGraph* rg) {
        auto tex = rg->create_transient_texture("t", tex2d());
        auto buf = rg->import_buffer("rb", readback.handle());
        rg->add_pass(
            "w", PassKind::Graphics, [&](PassBuilder& b) { b.color(tex, 0); },
            [tex, out = &seen[0]](PassContext& ctx) { *out = ctx.texture(tex); });
        rg->add_pass(
            "rb", PassKind::Transfer,
            [&](PassBuilder& b) {
                b.copy_texture_to_buffer(tex, buf);
                b.force_keep();
            },
            [tex, buf](PassContext& ctx) { encode_readback(ctx, tex, buf, W, H); });
    });

    // frame 1: color() only -> usage RenderAttachment (a subset) -> must reuse the wider pooled object
    g.frame([&](RenderGraph* rg) {
        auto tex = rg->create_transient_texture("t", tex2d());
        rg->add_pass(
            "w", PassKind::Graphics,
            [&](PassBuilder& b) {
                b.color(tex, 0);
                b.force_keep();
            },
            [tex, out = &seen[1]](PassContext& ctx) { *out = ctx.texture(tex); });
    });

    REQUIRE(seen[0] != nullptr);
    REQUIRE(seen[0] == seen[1]); // narrower request reused the wider object
    REQUIRE(list_count(g.allocator->transient.entries) == 1);
}

// an object unclaimed for kRetain frames is destroyed by end_frame
TEST_CASE("RenderGraph exec - an idle transient is evicted after kRetain frames", "[RenderGraph][gpu]")
{
    ExecGraph g;
    g.frame([&](RenderGraph* rg) {
        auto tex = rg->create_transient_texture("t", tex2d());
        rg->add_pass(
            "w", PassKind::Graphics,
            [&](PassBuilder& b) {
                b.color(tex, 0);
                b.force_keep();
            },
            [](PassContext&) {});
    });
    REQUIRE(list_count(g.allocator->transient.entries) == 1);

    for (uint32_t i = 0; i < TransientResourcePool::kRetain; ++i)
        g.frame([](RenderGraph*) {}); // empty graph: object goes unclaimed
    REQUIRE((g.allocator->transient.entries == nullptr));
}

// ---------------------------------------------------------------------------------------------------
// initialize() gating. an init pass is a cull root only while armed: once its target is baked with the
// matching hash compile() sets skipInit and drops it. the counter proves the body runs exactly on the
// frames the pass is armed.
TEST_CASE("RenderGraph exec - initialize() bakes once, skips while the hash holds, re-arms on change", "[RenderGraph][gpu]")
{
    ExecGraph g;
    int bakes = 0;
    int snap[3] = {};
    const uint64_t hashes[3] = { 7, 7, 9 }; // stable for two frames, then changes

    for (int f = 0; f < 3; ++f) {
        const uint64_t hash = hashes[f];
        int* counter = &bakes;
        g.frame(
            [&](RenderGraph* rg) {
                auto p = rg->create_persistent_texture("baked", tex2d());
                rg->add_pass(
                    "bake", PassKind::Graphics,
                    [&](PassBuilder& b) {
                        b.color(p, 0); // declare the write
                        b.initialize(p, hash); // gate: run only when stale
                        b.force_keep(); // keep the armed bake (no same-frame reader)
                    },
                    [counter](PassContext&) { (*counter)++; });
            },
            [&, f](RenderGraph*) { snap[f] = bakes; });
    }

    REQUIRE(snap[0] == 1); // frame 0: first bake
    REQUIRE(snap[1] == 1); // frame 1: same hash -> skipInit, body did not run
    REQUIRE(snap[2] == 2); // frame 2: hash changed -> re-armed
}

// two graphs in one begin_frame/end_frame. release_resources() at the end of A's execute() frees its
// claim, so B's realize hands back the very same texture. the "execute order on one queue" contract.
TEST_CASE("RenderGraph exec - a second graph in one frame reuses the first graph's released transient", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTexture a = nullptr, b = nullptr;

    auto record_one = [](RenderGraph* rg, WGPUTexture* out) {
        auto tex = rg->create_transient_texture("t", tex2d());
        rg->add_pass(
            "w", PassKind::Graphics,
            [&](PassBuilder& bb) {
                bb.color(tex, 0);
                bb.force_keep();
            },
            [tex, out](PassContext& ctx) { *out = ctx.texture(tex); });
    };

    begin_frame(g.allocator);
    {
        RenderGraph* rg = start_recording(g.allocator);
        record_one(rg, &a);
        g.submit_graph(rg); // execute() releases the transient claim on finish
    }
    {
        RenderGraph* rg = start_recording(g.allocator); // same frame, same allocator
        record_one(rg, &b);
        g.submit_graph(rg);
    }
    g.wait_idle();
    end_frame(g.allocator);

    REQUIRE(a != nullptr);
    REQUIRE(a == b); // graph B reused graph A's now-idle transient
    REQUIRE(list_count(g.allocator->transient.entries) == 1); // one physical object served both graphs
}

// the aging clock ticks once per begin_frame/end_frame, not per execute(): two graphs per frame here,
// yet the transient still survives exactly kRetain idle frames. release_claims() only frees claims for
// same-frame reuse, only end_frame() advances `frame`.
TEST_CASE("RenderGraph exec - transient eviction clock is per-frame, not per-graph", "[RenderGraph][gpu]")
{
    using webgpu::rg::Internal::TransientResourcePool;
    ExecGraph g;

    // the filler below uses a different format, so it neither reuses this object nor trips the
    // same-format supersede path, leaving only the kRetain idle clock to evict it
    auto count_tracked = [&] {
        size_t n = 0;
        for (const TransientResourcePool::Entry* e = g.allocator->transient.entries; e; e = e->next)
            if (e->kind == ResourceKind::Texture && e->sig.size.width == 16 && e->sig.format == WGPUTextureFormat_RGBA8Unorm)
                ++n;
        return n;
    };

    auto clear_transient = [](RenderGraph* rg, const char* id, uint32_t wh, WGPUTextureFormat fmt) {
        TextureDesc d = tex2d();
        d.absolute = { wh, wh, 1 };
        d.format = fmt;
        auto t = rg->create_transient_texture(id, d);
        rg->add_pass(
            "w", PassKind::Graphics,
            [&](PassBuilder& b) {
                b.color(t, 0);
                b.force_keep(); // nothing reads it; keep the pass so the transient is realized
            },
            [](PassContext&) {});
    };

    // frame 0: one graph creates the transient we track
    g.frame([&](RenderGraph* rg) { clear_transient(rg, "tracked", 16, WGPUTextureFormat_RGBA8Unorm); });
    REQUIRE(count_tracked() == 1);

    // aging frames: TWO graphs each, on a distinct-format filler so nothing refreshes the tracked
    // object's recency. per-execute aging would kill it twice as fast.
    auto aging_frame = [&] {
        begin_frame(g.allocator);
        { RenderGraph* rg = start_recording(g.allocator); clear_transient(rg, "filler", 8, WGPUTextureFormat_R8Unorm); g.submit_graph(rg); }
        { RenderGraph* rg = start_recording(g.allocator); clear_transient(rg, "filler", 8, WGPUTextureFormat_R8Unorm); g.submit_graph(rg); }
        g.wait_idle();
        end_frame(g.allocator);
    };

    for (uint64_t i = 1; i < TransientResourcePool::kRetain; ++i) {
        aging_frame();
        REQUIRE(count_tracked() == 1); // idle < kRetain frames -> retained despite 2 graphs/frame
    }
    aging_frame();
    REQUIRE(count_tracked() == 0); // idle == kRetain frames -> evicted
}

// two graphs, one frame, same transient at different sizes. acquire() is exact-size create-on-miss and
// destruction defers to end_frame(), so B never gets A's wrong-sized object, both live through the
// frame, and both being used this frame keeps supersede from firing on either.
TEST_CASE("RenderGraph exec - two sizes used in one frame coexist and neither is evicted", "[RenderGraph][gpu]")
{
    using webgpu::rg::Internal::TransientResourcePool;
    ExecGraph g;
    WGPUTexture big = nullptr, small = nullptr;

    auto clear_sized = [](RenderGraph* rg, uint32_t wh, WGPUTexture* out) {
        TextureDesc d = tex2d(); // same format; only the size differs between the two graphs
        d.absolute = { wh, wh, 1 };
        auto t = rg->create_transient_texture("t", d);
        rg->add_pass(
            "w", PassKind::Graphics,
            [&](PassBuilder& b) {
                b.color(t, 0);
                b.force_keep();
            },
            [t, out](PassContext& ctx) { *out = ctx.texture(t); });
    };

    auto count = [&](uint32_t w) {
        size_t n = 0;
        for (const TransientResourcePool::Entry* e = g.allocator->transient.entries; e; e = e->next)
            if (e->kind == ResourceKind::Texture && e->sig.size.width == w)
                ++n;
        return n;
    };

    begin_frame(g.allocator);
    { RenderGraph* rg = start_recording(g.allocator); clear_sized(rg, 16, &big); g.submit_graph(rg); }
    { RenderGraph* rg = start_recording(g.allocator); clear_sized(rg, 8, &small); g.submit_graph(rg); }
    g.wait_idle();

    // mid-frame: both live at once, and B got a fresh 8x8 rather than A's 16x16
    REQUIRE(big != nullptr);
    REQUIRE(small != nullptr);
    REQUIRE(big != small); // exact-size match -> distinct objects, no wrong-size reuse
    REQUIRE(list_count(g.allocator->transient.entries) == 2); // no mid-frame destruction

    end_frame(g.allocator);

    // both touched this frame -> superseded_by bails on lastUsedFrame >= frame
    REQUIRE(count(16) == 1);
    REQUIRE(count(8) == 1);
}

// ---------------------------------------------------------------------------------------------------
// resize end to end. the device-free pool tests drive end_frame() on hand-built entries, these are the
// only proof the real realize -> acquire -> end_frame path resizes as designed, identity and all.

// RGBA8 2D at a caller-chosen size, for the resize and relative-sizing tests
static TextureDesc tex2d_sized(uint32_t w, uint32_t h)
{
    TextureDesc d = tex2d();
    d.absolute = { w, h, 1 };
    return d;
}

TEST_CASE("RenderGraph exec - a resize destroys the old-size transient", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTexture before = nullptr, after = nullptr;

    // one transient, same name every frame: a screen-sized target across a window resize
    auto frame_at = [](RenderGraph* rg, uint32_t w, WGPUTexture* out) {
        auto t = rg->create_transient_texture("screen", tex2d_sized(w, w));
        rg->add_pass(
            "w", PassKind::Graphics,
            [&](PassBuilder& b) {
                b.color(t, 0);
                b.force_keep();
            },
            [t, out](PassContext& ctx) { *out = ctx.texture(t); });
    };

    uint32_t slotCount = 99;
    g.frame([&](RenderGraph* rg) { frame_at(rg, 16, &before); },
        [&](RenderGraph* rg) { slotCount = storage(rg)->m_slotCount; });
    // a lone clear+store attachment is aliasing-eligible, so this rides an alias slot and the identity
    // under test is PhysicalResource::identity. the per-resource arm is the alias-excluded test below.
    REQUIRE(slotCount == 1);

    g.frame([&](RenderGraph* rg) { frame_at(rg, 16, &before); }); // steady state: pooled, not recreated
    REQUIRE(list_count(g.allocator->transient.entries) == 1);

    g.frame([&](RenderGraph* rg) { frame_at(rg, 32, &after); });

    // the 16x16 is gone the frame the 32x32 appeared, well inside kRetain, so only supersede took it
    REQUIRE(after != nullptr);
    REQUIRE(after != before);
    REQUIRE(list_count(g.allocator->transient.entries) == 1);
    REQUIRE(g.allocator->transient.entries->sig.size.width == 32);
}

TEST_CASE("RenderGraph exec - a non-aliased transient resizes without leaking a generation", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTexture before = nullptr, after = nullptr;

    // a mip>1 transient is excluded from aliasing, so this drives the per-resource arm, whose identity
    // comes off the resource name. without it the supersede silently stops firing.
    auto frame_at = [](RenderGraph* rg, uint32_t w, WGPUTexture* out) {
        TextureDesc d = tex2d_sized(w, w);
        d.mipLevelCount = 2;
        auto t = rg->create_transient_texture("chain", d);
        rg->add_pass(
            "w", PassKind::Graphics,
            [&](PassBuilder& b) {
                b.color(t, 0);
                b.force_keep();
            },
            [t, out](PassContext& ctx) { *out = ctx.texture(t); });
    };

    uint32_t slotCount = 99;
    g.frame([&](RenderGraph* rg) { frame_at(rg, 16, &before); },
        [&](RenderGraph* rg) { slotCount = storage(rg)->m_slotCount; });
    REQUIRE(slotCount == 0); // not aliased -> the per-resource arm

    g.frame([&](RenderGraph* rg) { frame_at(rg, 32, &after); });

    REQUIRE(after != before);
    REQUIRE(list_count(g.allocator->transient.entries) == 1);
    REQUIRE(g.allocator->transient.entries->sig.size.width == 32);
}

TEST_CASE("RenderGraph exec - an aliased slot resizes without leaking a generation", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTexture a = nullptr, b = nullptr;

    // two disjoint transients on one alias slot, resized. aliasing must not multiply generations.
    auto frame_at = [](RenderGraph* rg, uint32_t w, WGPUTexture* o1, WGPUTexture* o2) {
        auto t1 = rg->create_transient_texture("t1", tex2d_sized(w, w));
        auto t2 = rg->create_transient_texture("t2", tex2d_sized(w, w));
        rg->add_pass(
            "k1", PassKind::Graphics,
            [&](PassBuilder& bb) {
                bb.color(t1, 0);
                bb.force_keep();
            },
            [t1, o1](PassContext& ctx) { *o1 = ctx.texture(t1); });
        rg->add_pass(
            "k2", PassKind::Graphics,
            [&](PassBuilder& bb) {
                bb.color(t2, 0);
                bb.force_keep();
            },
            [t2, o2](PassContext& ctx) { *o2 = ctx.texture(t2); });
    };

    uint32_t slotsBefore = 0;
    g.frame([&](RenderGraph* rg) { frame_at(rg, 16, &a, &b); },
        [&](RenderGraph* rg) { slotsBefore = storage(rg)->m_slotCount; });
    REQUIRE(slotsBefore == 1); // both transients share one slot
    REQUIRE(a == b);
    REQUIRE(list_count(g.allocator->transient.entries) == 1);

    uint32_t slotsAfter = 0;
    g.frame([&](RenderGraph* rg) { frame_at(rg, 32, &a, &b); },
        [&](RenderGraph* rg) { slotsAfter = storage(rg)->m_slotCount; });

    REQUIRE(slotsAfter == 1);
    REQUIRE(a == b);
    REQUIRE(list_count(g.allocator->transient.entries) == 1); // the 16x16 slot object did not survive
    REQUIRE(g.allocator->transient.entries->sig.size.width == 32);
}

// ===================================================================================================
// gap-analysis additions. compile-only unless tagged [gpu]. relative sizing, history-layer validation,
// storage RMW, buffer/excluded aliasing, the buffer read access types, the initialized/history
// constructors.

// declaring only slots 0 and 2 must make execute() emit a 3-entry array whose slot 1 is a null
// attachment. a bare render pass would not prove it: with no pipeline bound Dawn barely validates the
// color array. a pipeline writing @location(0) and @location(2) forces it to match 3 targets.
TEST_CASE("RenderGraph exec - a sparse color slot is emitted as a valid null attachment", "[RenderGraph][gpu]")
{
    ExecGraph g;

    static const char* kWgsl = R"(
@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
    var p = array<vec2f, 3>(vec2f(-1.0, -3.0), vec2f(-1.0, 1.0), vec2f(3.0, 1.0));
    return vec4f(p[i], 0.0, 1.0);
}
struct Out {
    @location(0) a: vec4f,
    @location(2) c: vec4f,
}
@fragment fn fs() -> Out {
    return Out(vec4f(1.0, 0.0, 0.0, 1.0), vec4f(0.0, 0.0, 1.0, 1.0));
}
)";
    WGPUShaderSourceWGSL wgsl {};
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl.code = WGPUStringView { .data = kWgsl, .length = WGPU_STRLEN };
    WGPUShaderModuleDescriptor smd {};
    smd.nextInChain = &wgsl.chain;
    WGPUShaderModule sm = wgpuDeviceCreateShaderModule(g.gpu.device, &smd);
    REQUIRE(sm != nullptr);

    // slot 1 is deliberately an unused target, so the pipeline expects a null attachment exactly where
    // the graph must emit one
    WGPUColorTargetState targets[3] {};
    targets[0].format = WGPUTextureFormat_RGBA8Unorm;
    targets[0].writeMask = WGPUColorWriteMask_All;
    targets[1].format = WGPUTextureFormat_Undefined;
    targets[2].format = WGPUTextureFormat_RGBA8Unorm;
    targets[2].writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fs {};
    fs.module = sm;
    fs.entryPoint = WGPUStringView { .data = "fs", .length = WGPU_STRLEN };
    fs.targetCount = 3;
    fs.targets = targets;

    WGPURenderPipelineDescriptor rpd {};
    rpd.vertex.module = sm;
    rpd.vertex.entryPoint = WGPUStringView { .data = "vs", .length = WGPU_STRLEN };
    rpd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rpd.multisample.count = 1;
    rpd.multisample.mask = 0xFFFFFFFF;
    rpd.fragment = &fs;
    WGPURenderPipeline pipe = wgpuDeviceCreateRenderPipeline(g.gpu.device, &rpd);
    REQUIRE(pipe != nullptr);

    wgpuDevicePushErrorScope(g.gpu.device, WGPUErrorFilter_Validation);

    g.frame([pipe](RenderGraph* rg) {
        auto s0 = rg->create_transient_texture("slot0", tex2d_sized(16, 16));
        auto s2 = rg->create_transient_texture("slot2", tex2d_sized(16, 16));
        rg->add_pass(
            "sparse", PassKind::Graphics,
            [s0, s2](PassBuilder& b) {
                b.color(s0, 0);
                b.color(s2, 2); // slot 1 left unused -> null attachment, colorAttachmentCount == 3
                b.force_keep();
            },
            [pipe](PassContext& c) {
                wgpuRenderPassEncoderSetPipeline(c.render_pass, pipe);
                wgpuRenderPassEncoderDraw(c.render_pass, 3, 1, 0, 0);
            });
    });

    struct ScopeResult {
        bool done = false;
        bool failed = false;
        std::string message;
    } res;
    WGPUPopErrorScopeCallbackInfo ci {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = [](WGPUPopErrorScopeStatus, WGPUErrorType type, WGPUStringView msg, void* d, void*) {
            ScopeResult* r = reinterpret_cast<ScopeResult*>(d);
            r->done = true;
            r->failed = (type != WGPUErrorType_NoError);
            if (msg.data)
                r->message.assign(msg.data, msg.length);
        },
        .userdata1 = &res,
        .userdata2 = nullptr,
    };
    WGPUFuture f = wgpuDevicePopErrorScope(g.gpu.device, ci);
    WGPUFutureWaitInfo wi { .future = f, .completed = false };
    REQUIRE(wgpuInstanceWaitAny(g.gpu.instance, 1, &wi, 1000ull * 1000 * 1000) == WGPUWaitStatus_Success);
    REQUIRE(res.done);
    INFO(res.message);
    REQUIRE_FALSE(res.failed);

    wgpuRenderPipelineRelease(pipe);
    wgpuShaderModuleRelease(sm);
}


// a relativeTo child is width*scale, rounded not truncated. 10 x 0.25 = 2.5 -> 3.
TEST_CASE("RenderGraph - relative-sized transient rounds against its base", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;

    TestGraph g;
    auto base = g.rg->create_transient_texture("base", tex2d_sized(10, 10));

    TextureDesc rd {};
    rd.dimension = WGPUTextureDimension_2D;
    rd.format = WGPUTextureFormat_RGBA8Unorm;
    rd.sizeKind = SizeKind::Relative;
    rd.scaleX = 0.25f;
    rd.scaleY = 0.25f;
    rd.relativeTo = base;
    rd.absolute = { 0, 0, 1 }; // width/height come from the base; depthOrArrayLayers stays 1
    auto child = g.rg->create_transient_texture("child", rd);

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(child, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    // 10 * 0.25 = 2.5 -> round to 3 (truncation would give 2)
    REQUIRE(find_node(g.rg, child)->resolved.width == 3);
    REQUIRE(find_node(g.rg, child)->resolved.height == 3);
}

// a two-deep chain resolves recursively: 40 -> mid 0.5 -> leaf 0.5
TEST_CASE("RenderGraph - relative-size chain resolves each level", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;

    TestGraph g;
    auto base = g.rg->create_transient_texture("base", tex2d_sized(40, 40));

    auto half_of = [&](std::string_view id, TextureHandle parent) {
        TextureDesc d {};
        d.dimension = WGPUTextureDimension_2D;
        d.format = WGPUTextureFormat_RGBA8Unorm;
        d.sizeKind = SizeKind::Relative;
        d.scaleX = 0.5f;
        d.scaleY = 0.5f;
        d.relativeTo = parent;
        d.absolute = { 0, 0, 1 };
        return g.rg->create_transient_texture(id, d);
    };
    auto mid = half_of("mid", base);
    auto leaf = half_of("leaf", mid);

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(leaf, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(find_node(g.rg, mid)->resolved.width == 20);
    REQUIRE(find_node(g.rg, leaf)->resolved.width == 10);
}

// ---------------------------------------------------------------------------------------------------
// render-pass descriptor validation. each is a rule WebGPU enforces on the descriptor execute() builds,
// caught here so compile() can name the pass and resources instead of leaving a device error that only
// describes the descriptor. each pass needs force_keep(), or culling drops it before the validator runs.

static TextureDesc tex2d_fmt(WGPUTextureFormat fmt, uint32_t w = 16, uint32_t h = 16, uint32_t samples = 1)
{
    TextureDesc d {};
    d.dimension = WGPUTextureDimension_2D;
    d.format = fmt;
    d.absolute = { w, h, 1 };
    d.sampleCount = samples;
    return d;
}

TEST_CASE("compile - a Graphics pass with no attachments is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");
    // the writer must come first, or the read-before-write check returns before render-pass validation
    g.rg->add_pass(
        "produce", PassKind::Graphics, [&](PassBuilder& b) { b.color(tex, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(tex); // reads only, so BeginRenderPass would have nothing to attach
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "Graphics pass with no attachments"));
}

// depth-only is legal, it is what a shadow map looks like
TEST_CASE("compile - a depth-only Graphics pass is valid", "[RenderGraph]")
{
    TestGraph g;
    auto depth = g.rg->create_transient_texture("shadow", tex2d_fmt(WGPUTextureFormat_Depth32Float));
    g.rg->add_pass(
        "shadow", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.depth_stencil(depth);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

TEST_CASE("compile - attachments of different sizes in one pass are rejected", "[RenderGraph]")
{
    TestGraph g;
    auto full = g.rg->create_transient_texture("full", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16));
    auto half = g.rg->create_transient_texture("half", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 8, 8));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(full, 0);
            b.color(half, 1);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "must be the same size"));
}

TEST_CASE("compile - attachments of different sample counts in one pass are rejected", "[RenderGraph]")
{
    TestGraph g;
    auto msaa = g.rg->create_transient_texture("msaa", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 4));
    auto single = g.rg->create_transient_texture("single", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 1));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(msaa, 0);
            b.color(single, 1);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "must have the same sample count"));
}

TEST_CASE("compile - a well-formed MSAA resolve is valid", "[RenderGraph]")
{
    TestGraph g;
    auto msaa = g.rg->create_transient_texture("msaa", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 4));
    auto target = g.rg->create_transient_texture("target", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 1));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(msaa, 0);
            b.resolve(msaa, target);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

TEST_CASE("compile - resolving a single-sampled source is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto src = g.rg->create_transient_texture("src", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 1));
    auto target = g.rg->create_transient_texture("target", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 1));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(src, 0);
            b.resolve(src, target);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "must be multisampled"));
}

TEST_CASE("compile - a multisampled resolve target is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto msaa = g.rg->create_transient_texture("msaa", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 4));
    auto target = g.rg->create_transient_texture("target", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 4));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(msaa, 0);
            b.resolve(msaa, target);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "must be single-sampled"));
}

TEST_CASE("compile - a resolve target with a different format is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto msaa = g.rg->create_transient_texture("msaa", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 4));
    auto target = g.rg->create_transient_texture("target", tex2d_fmt(WGPUTextureFormat_R32Float, 16, 16, 1));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(msaa, 0);
            b.resolve(msaa, target);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "must have the same format"));
}

// a resolve target is excluded from the shared-extent check, so the resolve arm must catch this
TEST_CASE("compile - a resolve target of a different size is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto msaa = g.rg->create_transient_texture("msaa", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 16, 16, 4));
    auto target = g.rg->create_transient_texture("target", tex2d_fmt(WGPUTextureFormat_RGBA8Unorm, 8, 8, 1));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(msaa, 0);
            b.resolve(msaa, target);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "they must be the same size"));
}

TEST_CASE("compile - a colour format bound as depth-stencil is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto notDepth = g.transient("notDepth"); // RGBA8Unorm
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.depth_stencil(notDepth);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "format is not a depth format"));
}

TEST_CASE("compile - stencil ops on a format without a stencil aspect are rejected", "[RenderGraph]")
{
    TestGraph g;
    auto depth = g.rg->create_transient_texture("depth", tex2d_fmt(WGPUTextureFormat_Depth32Float));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.depth_stencil(depth, { .stencilLoad = WGPULoadOp_Clear, .stencilStore = WGPUStoreOp_Store });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "has no stencil aspect"));
}

// the inverse: WebGPU requires the ops when the format has a stencil aspect, and depth_stencil() leaves
// them Undefined, so the default is wrong for a combined format
TEST_CASE("compile - a stencil format without stencil ops is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto ds = g.rg->create_transient_texture("ds", tex2d_fmt(WGPUTextureFormat_Depth24PlusStencil8));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.depth_stencil(ds); // stencil ops default to Undefined
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "requires stencil load/store ops"));
}

TEST_CASE("compile - a stencil format with stencil ops is valid", "[RenderGraph]")
{
    TestGraph g;
    auto ds = g.rg->create_transient_texture("ds", tex2d_fmt(WGPUTextureFormat_Depth24PlusStencil8));
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.depth_stencil(ds, { .stencilLoad = WGPULoadOp_Clear, .stencilStore = WGPUStoreOp_Store });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

// execute() never sets depthSlice, which WebGPU requires for a 3D color target
TEST_CASE("compile - a 3D texture used as a render target is rejected", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc d = tex2d_fmt(WGPUTextureFormat_RGBA8Unorm);
    d.dimension = WGPUTextureDimension_3D;
    d.absolute = { 16, 16, 4 };
    auto vol = g.rg->create_transient_texture("vol", d);
    g.rg->add_pass(
        "draw", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(vol, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "3D texture used as a render target"));
}

// ---------------------------------------------------------------------------------------------------
// copy_extent_* describes the declared subresource, not everything the texture holds. copy_*_info()
// offsets origin.z by baseLayer, so a whole-array extent would run the copy off the end. resolves from
// the node and access alone, so the pass is built by hand and no device is needed.

static PassNode* pass_named(RenderGraph* rg, std::string_view name)
{
    for (PassNode* p = storage(rg)->m_passes; p; p = p->next)
        if (std::string_view(p->id.name.data, p->id.name.length) == name)
            return p;
    return nullptr;
}

// no comma in the name, Catch2 treats one as a filter separator
TEST_CASE("PassContext::copy_extent - an array copy covers one layer rather than the whole array", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc d = tex2d();
    d.absolute = { 16, 16, 4 }; // 4 array layers
    auto src = g.rg->create_transient_texture("src", d);
    auto dst = g.rg->create_transient_texture("dst", d);

    g.rg->add_pass(
        "produce", PassKind::Graphics,
        [&](PassBuilder& b) { b.color(src, 0, { .sub = { .layer = 2 } }); }, [](PassContext&) {});
    g.rg->add_pass(
        "copy", PassKind::Transfer,
        [&](PassBuilder& b) {
            b.copy_texture(src, dst, { .layer = 2 }, { .layer = 1 }); // layer 2 -> layer 1, one layer
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    PassNode* copyPass = pass_named(g.rg, "copy");
    REQUIRE(copyPass != nullptr);
    Internal::PassContextAccess access(g.rg, copyPass);
    PassContext& ctx = access.ctx;
    REQUIRE(ctx.copy_extent_src(src).depthOrArrayLayers == 1); // not 4
    REQUIRE(ctx.copy_extent_dst(dst).depthOrArrayLayers == 1);
    REQUIRE(ctx.copy_extent_src(src).width == 16);
}

// 3D depth slices halve with the mip like width and height. only an import can carry both, since
// validate_texture_desc holds graph-created 3D textures to one mip.
TEST_CASE("PassContext::copy_extent - a 3D copy shrinks its depth with the mip", "[RenderGraph]")
{
    TestGraph g;
    auto import3d = [&](std::string_view id) {
        return g.rg->import_texture(id, { .view = (WGPUTextureView)0x1, .size = { 16, 16, 8 }, .format = WGPUTextureFormat_RGBA8Unorm, .mipCount = 2, .dimension = WGPUTextureDimension_3D });
    };
    auto src = import3d("src3d");
    auto dst = import3d("dst3d");

    g.rg->add_pass(
        "copy", PassKind::Transfer, [&](PassBuilder& b) { b.copy_texture(src, dst, { .mip = 1 }, { .mip = 1 }); }, [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    PassNode* copyPass = pass_named(g.rg, "copy");
    REQUIRE(copyPass != nullptr);
    Internal::PassContextAccess access(g.rg, copyPass);
    PassContext& ctx = access.ctx;
    WGPUExtent3D e = ctx.copy_extent_src(src);
    REQUIRE(e.width == 8); // 16 >> 1
    REQUIRE(e.height == 8);
    REQUIRE(e.depthOrArrayLayers == 4); // 8 >> 1, not the base depth of 8
}

// ---------------------------------------------------------------------------------------------------
// zero extents. a 0-sized texture fails device validation with no link back to the descriptor, so
// compile() names it first. rounding down to 0 clamps instead, it happens legitimately mid-resize.

// a child small enough to round to 0 clamps to 1 rather than failing the frame
TEST_CASE("RenderGraph - a relative size that rounds to zero clamps to 1", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;

    TestGraph g;
    auto base = g.rg->create_transient_texture("base", tex2d_sized(1, 1));

    TextureDesc rd {};
    rd.dimension = WGPUTextureDimension_2D;
    rd.format = WGPUTextureFormat_RGBA8Unorm;
    rd.sizeKind = SizeKind::Relative;
    rd.scaleX = 0.25f; // 1 * 0.25 = 0.25 -> rounds to 0
    rd.scaleY = 0.25f;
    rd.relativeTo = base;
    rd.absolute = { 0, 0, 1 };
    auto child = g.rg->create_transient_texture("child", rd);

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(base, 0);
            b.color(child, 1);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(find_node(g.rg, child)->resolved.width == 1);
    REQUIRE(find_node(g.rg, child)->resolved.height == 1);
}

// an absolute extent left at its default is an authoring error, not a rounding artifact
TEST_CASE("RenderGraph - a live texture with a zero absolute extent is rejected", "[RenderGraph]")
{
    TestGraph g;
    // the declare-time assert covers this in a debug build, so reach the compile backstop the same
    // white-box way the cyclic relativeTo test does
    auto zero = g.rg->create_transient_texture("zero", tex2d());
    Internal::find_node(g.rg, zero)->absolute = { 0, 0, 1 };
    Internal::find_node(g.rg, zero)->resolved = {};

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(zero, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "\"zero\" resolves to a zero extent (0 x 0)"));
}

// the minimized-window case. the error must name the import, not the relative children, which clamp to
// 1 and are not themselves wrong. the height is deliberately non-zero: were the memo guard to test width
// alone this would fall through to the Absolute arm and report 0 x 0, since an import never fills
// `absolute`.
TEST_CASE("RenderGraph - a zero-sized import is rejected and its children are not", "[RenderGraph]")
{
    TestGraph g;
    auto surface = g.rg->import_texture("surface", { .view = (WGPUTextureView)0x1, .size = { 0, 16, 1 }, .format = WGPUTextureFormat_RGBA8Unorm });

    TextureDesc rd {};
    rd.dimension = WGPUTextureDimension_2D;
    rd.format = WGPUTextureFormat_RGBA8Unorm;
    rd.sizeKind = SizeKind::Relative;
    rd.scaleX = 0.5f;
    rd.scaleY = 0.5f;
    rd.relativeTo = surface;
    rd.absolute = { 0, 0, 1 };
    auto child = g.rg->create_transient_texture("half", rd);

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(child, 0);
            b.color(surface, 1);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "\"surface\" resolves to a zero extent (0 x 16)")); // not 0 x 0
    REQUIRE_FALSE(error_mentions(g.rg, "\"half\" resolves to a zero extent"));
}

// relative with no relativeTo would clamp to a silent 1x1, so it is reported instead
TEST_CASE("RenderGraph - a relative size with no relativeTo is rejected", "[RenderGraph]")
{
    TestGraph g;
    auto orphan = g.rg->create_transient_texture("orphan", tex2d());
    Internal::find_node(g.rg, orphan)->sizeKind = SizeKind::Relative; // never names a base
    Internal::find_node(g.rg, orphan)->resolved = {};

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(orphan, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "names no resource to size against"));
}

// depthOrArrayLayers == 0 means one layer everywhere else, so the resolver normalizes rather than
// treating it as a zero extent
TEST_CASE("RenderGraph - a zero depthOrArrayLayers normalizes to one layer", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;

    TestGraph g;
    TextureDesc d = tex2d();
    d.absolute = { 16, 16, 0 };
    auto tex = g.rg->create_transient_texture("tex", d);

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(tex, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(find_node(g.rg, tex)->resolved.depthOrArrayLayers == 1);
}

// release-only: validate_texture_desc asserts both at the create call, and in debug the abort IS the
// contract. these reach the release backstop through the public API rather than by patching the node.
#ifdef QT_NO_DEBUG
TEST_CASE("RenderGraph - a zero absolute extent is caught through the public API", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc d = tex2d();
    d.absolute = { 0, 0, 1 };
    auto zero = g.rg->create_transient_texture("zero", d);

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(zero, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "resolves to a zero extent"));
}
#endif

#ifdef QT_NO_DEBUG
TEST_CASE("RenderGraph - a missing relativeTo is caught through the public API", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc d = tex2d();
    d.sizeKind = SizeKind::Relative; // relativeTo left at {}
    auto orphan = g.rg->create_transient_texture("orphan", d);

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(orphan, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "names no resource to size against"));
}
#endif

// only live textures are created, so a zero-sized one nothing touches must not fail the frame
TEST_CASE("RenderGraph - an unused zero-sized texture compiles clean", "[RenderGraph]")
{
    TestGraph g;
    auto used = g.transient("used");
    auto dead = g.rg->create_transient_texture("dead", tex2d());
    Internal::find_node(g.rg, dead)->absolute = { 0, 0, 1 }; // declared, never accessed
    Internal::find_node(g.rg, dead)->resolved = {};

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(used, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

// writing .curr while nothing reads .prev culls the layers asymmetrically, which is unrealizable
TEST_CASE("RenderGraph - history with a curr writer but no prev reader is an error", "[RenderGraph]")
{
    TestGraph g;
    auto hist = g.rg->create_history_texture("hist", tex2d());

    g.rg->add_pass(
        "writeCurr", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(hist.curr, 0); // .curr used, .prev never read
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "read .prev and write .curr in one pass"));
}

// only .curr is writable
TEST_CASE("RenderGraph - writing a history .prev layer is an error", "[RenderGraph]")
{
    TestGraph g;
    auto hist = g.rg->create_history_texture("hist", tex2d());

    g.rg->add_pass(
        "writePrev", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(hist.prev, 0); // layer 1 is read-only this frame
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "only layer 0, the .curr handle, is writable"));
}

// the same-pass write cannot seed the read, dispatch invocations are unordered
TEST_CASE("RenderGraph - storage_read_write on an unproduced transient is an error", "[RenderGraph]")
{
    TestGraph g;
    auto buf = g.transient_buffer("rmw");

    g.rg->add_pass(
        "rmw", PassKind::Compute,
        [&](PassBuilder& b) {
            b.storage_read_write(buf);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    // the read-modify-write diagnosis, not "no pass ever writes": a writer exists, it just cannot seed
    // this read
    REQUIRE(error_mentions(g.rg, "can't seed the read"));
}

// legal once an earlier pass produced the buffer
TEST_CASE("RenderGraph - storage_read_write after a producer compiles clean", "[RenderGraph]")
{
    TestGraph g;
    auto buf = g.transient_buffer("rmw");

    add_buffer_producer(g.rg, buf); // storage_write producer, satisfies the RMW read
    g.rg->add_pass(
        "rmw", PassKind::Compute,
        [&](PassBuilder& b) {
            b.storage_read_write(buf);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(idx_of(pass_order(g.rg), "produce") < idx_of(pass_order(g.rg), "rmw"));
}

// the isBuf path the texture alias tests never reach. storage_write fully defines.
TEST_CASE("RenderGraph - disjoint transient buffers share one physical slot", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;
    using webgpu::rg::Internal::ResourceNode;

    TestGraph g;
    auto b1 = g.transient_buffer("b1", 64);
    auto b2 = g.transient_buffer("b2", 64);

    g.rg->add_pass(
        "k1", PassKind::Compute,
        [&](PassBuilder& b) {
            b.storage_write(b1);
            b.force_keep();
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "k2", PassKind::Compute,
        [&](PassBuilder& b) {
            b.storage_write(b2);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile(true) == nullptr);
    REQUIRE(storage(g.rg)->m_slotCount == 1);
    ResourceNode* r1 = find_node(g.rg, b1);
    ResourceNode* r2 = find_node(g.rg, b2);
    REQUIRE(r1->aliasSlot != ResourceNode::kNoSlot);
    REQUIRE(r1->aliasSlot == r2->aliasSlot);
}

// mip-chain transients are excluded, a slot's default view would not fit
TEST_CASE("RenderGraph - mip-chain transients are excluded from aliasing", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;
    using webgpu::rg::Internal::ResourceNode;

    TestGraph g;
    auto t1 = g.transient("t1", /*mipLevelCount*/ 2);
    auto t2 = g.transient("t2", /*mipLevelCount*/ 2);

    g.rg->add_pass(
        "k1", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(t1, 0);
            b.force_keep();
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "k2", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(t2, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile(true) == nullptr);
    REQUIRE(storage(g.rg)->m_slotCount == 0);
    REQUIRE(find_node(g.rg, t1)->aliasSlot == ResourceNode::kNoSlot);
    REQUIRE(find_node(g.rg, t2)->aliasSlot == ResourceNode::kNoSlot);
}

// a non-Clear first touch does not fully define the storage, so it is ineligible even though a pass
// writes it
TEST_CASE("RenderGraph - a load-first transient is excluded from aliasing", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;
    using webgpu::rg::Internal::ResourceNode;

    TestGraph g;
    auto t1 = g.transient("t1");
    auto t2 = g.transient("t2");

    g.rg->add_pass(
        "k1", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(t1, 0, { .load = WGPULoadOp_Load }); // load-first: not a full define
            b.force_keep();
        },
        [](PassContext&) {});
    g.rg->add_pass(
        "k2", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(t2, 0, { .load = WGPULoadOp_Load });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile(true) == nullptr);
    REQUIRE(storage(g.rg)->m_slotCount == 0);
    REQUIRE(find_node(g.rg, t1)->aliasSlot == ResourceNode::kNoSlot);
    REQUIRE(find_node(g.rg, t2)->aliasSlot == ResourceNode::kNoSlot);
}

// the depth analogue of the color clear+discard test
TEST_CASE("RenderGraph - single clear+discard depth attachment is inferred transient", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc dd {};
    dd.dimension = WGPUTextureDimension_2D;
    dd.format = WGPUTextureFormat_Depth32Float;
    dd.absolute = { 16, 16, 1 };
    auto ds = g.rg->create_transient_texture("ds", dd);

    g.rg->add_pass(
        "depth", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.depth_stencil(ds, { .store = WGPUStoreOp_Discard });
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(storage(g.rg)->transientCount == 1);
}

// uniform/vertex/index/indirect accumulate their usage onto the node. read against persistent buffers,
// which are external, so no read-before-write.
TEST_CASE("RenderGraph - buffer read accesses accumulate their usage bits", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;

    TestGraph g;
    BufferDesc bd {};
    bd.size = 64;
    auto ubo = g.rg->create_persistent_buffer("ubo", bd);
    auto vbo = g.rg->create_persistent_buffer("vbo", bd);
    auto ibo = g.rg->create_persistent_buffer("ibo", bd);
    auto indirect = g.rg->create_persistent_buffer("indirect", bd);
    auto target = g.transient("target"); // vertex/index/indirect are graphics inputs, so this
                                                   // stays a Graphics pass and needs a real attachment

    g.rg->add_pass(
        "read", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(target, 0);
            b.uniform(ubo);
            b.vertex_buffer(vbo);
            b.index_buffer(ibo);
            b.indirect_buffer(indirect);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE((find_node(g.rg, ubo)->bufUsage & WGPUBufferUsage_Uniform) != 0);
    REQUIRE((find_node(g.rg, vbo)->bufUsage & WGPUBufferUsage_Vertex) != 0);
    REQUIRE((find_node(g.rg, ibo)->bufUsage & WGPUBufferUsage_Index) != 0);
    REQUIRE((find_node(g.rg, indirect)->bufUsage & WGPUBufferUsage_Indirect) != 0);
}

// the rewrite shares no edge with the reader except the WAR one, so reader-before-rewrite proves it
// fired
TEST_CASE("RenderGraph - write-after-read orders the rewriter after the reader", "[RenderGraph]")
{
    TestGraph g;
    auto sink = import_tex(g.rg, "sink");
    auto x = g.transient("x");

    g.rg->add_pass(
        "produce", PassKind::Graphics, [&](PassBuilder& b) { b.color(x, 0); }, [](PassContext&) {});
    g.rg->add_pass(
        "read", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(x);
            b.color(sink, 0, { .load = WGPULoadOp_Load });
        },
        [](PassContext&) {});
    g.rg->add_pass( // clobbers x's version the reader still uses -> WAR edge onto "read"
        "rewrite", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(x, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    auto order = pass_order(g.rg);
    REQUIRE(idx_of(order, "read") < idx_of(order, "rewrite"));
}

// initialize() targets must be pool-backed
TEST_CASE("RenderGraph - initialize() on a transient target is an error", "[RenderGraph]")
{
    TestGraph g;
    auto tex = g.transient("tex");

    g.rg->add_pass(
        "bake", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(tex, 0); // the required write
            b.initialize(tex); // ... but tex is transient, not persistent
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() != nullptr);
    REQUIRE(error_mentions(g.rg, "must be persistent or history"));
}

// the ping-pong pattern for a GPU-authored buffer. .curr is a cull sink and .prev is external, so it
// compiles clean.
TEST_CASE("RenderGraph - history buffer read-prev/write-curr compiles clean", "[RenderGraph]")
{
    TestGraph g;
    BufferDesc bd {};
    bd.size = 64;
    auto h = g.rg->create_history_buffer("hbuf", bd);

    g.rg->add_pass(
        "temporal", PassKind::Compute,
        [&](PassBuilder& b) {
            b.storage_read(h.prev);
            b.storage_write(h.curr);
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(pass_order(g.rg).size() == 1);
}

// synthesizes an initialize()'d bake pass, which a reader pulls in as a dependency
TEST_CASE("RenderGraph - create_initialized_texture bakes a pass a reader can consume", "[RenderGraph]")
{
    TestGraph g;
    auto fallback = g.rg->create_initialized_texture("fallback", tex2d(), { 0, 0, 0, 1 });
    auto out = g.transient("out");

    g.rg->add_pass(
        "use", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(fallback);
            b.color(out, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(pass_order(g.rg).size() == 2); // the synthesized bake + the reader
    REQUIRE(idx_of(pass_order(g.rg), "fallback") != -1); // single layer keeps the bare id, no suffix
}

// one pass per layer, named "<id>.layer<n>" so debug groups and profiler series stay distinct. a missing
// suffix would not merely blur labels, assert_unique_id rejects duplicate pass ids.
TEST_CASE("RenderGraph - create_initialized_texture names one bake pass per layer", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc d = tex2d();
    d.absolute = { 16, 16, 3 };
    auto fallback = g.rg->create_initialized_texture("fallback", d, { 0, 0, 0, 1 });
    auto out = g.transient("out");

    g.rg->add_pass(
        "use", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(fallback, ViewRange { .mipCount = 1, .layerCount = 3, .dim = WGPUTextureViewDimension_2DArray });
            b.color(out, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    std::vector<std::string> order = pass_order(g.rg);
    REQUIRE(order.size() == 4); // three bakes + the reader
    REQUIRE(idx_of(order, "fallback.layer0") != -1);
    REQUIRE(idx_of(order, "fallback.layer1") != -1);
    REQUIRE(idx_of(order, "fallback.layer2") != -1);
    REQUIRE(idx_of(order, "fallback") == -1); // suffixed, not bare
}

// the suffixed name is sized from the id, not a fixed buffer, so a long id comes through whole
TEST_CASE("RenderGraph - a long multi-layer init id is not truncated", "[RenderGraph]")
{
    TestGraph g;
    const std::string longId(200, 'y');
    TextureDesc d = tex2d();
    d.absolute = { 16, 16, 2 };
    auto fallback = g.rg->create_initialized_texture(longId, d, { 0, 0, 0, 1 });
    auto out = g.transient("out");

    g.rg->add_pass(
        "use", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.sampled(fallback, ViewRange { .mipCount = 1, .layerCount = 2, .dim = WGPUTextureViewDimension_2DArray });
            b.color(out, 0);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);

    std::vector<std::string> order = pass_order(g.rg);
    REQUIRE(idx_of(order, (longId + ".layer0").c_str()) != -1);
    REQUIRE(idx_of(order, (longId + ".layer1").c_str()) != -1);
}

// synthesizes a host_write + initialize() bake pass, which a uniform reader pulls in
TEST_CASE("RenderGraph - create_initialized_buffer bakes a pass a reader can consume", "[RenderGraph]")
{
    TestGraph g;
    BufferDesc bd {};
    bd.size = 64;
    auto fallback = g.rg->create_initialized_buffer("fallback", bd, nullptr);

    g.rg->add_pass(
        "use", PassKind::Compute,
        [&](PassBuilder& b) {
            b.uniform(fallback);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(pass_order(g.rg).size() == 2); // the synthesized upload + the reader
}

// a memoryless transient has no storage to pack, so it is excluded from aliasing. the third exclusion
// branch, alongside the mip-chain and load-first tests.
TEST_CASE("RenderGraph - a memoryless attachment is excluded from aliasing", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;
    using webgpu::rg::Internal::ResourceNode;

    TestGraph g;
    auto t = g.transient("t");

    g.rg->add_pass(
        "w", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(t, 0, { .store = WGPUStoreOp_Discard }); // clear+discard -> memoryless
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile(true) == nullptr); // aliasing on: a plain clear+store transient here would be eligible
    REQUIRE(storage(g.rg)->transientCount == 1); // inferred memoryless
    REQUIRE(storage(g.rg)->m_slotCount == 0); // ... so no alias slot was opened for it
    REQUIRE(find_node(g.rg, t)->aliasSlot == ResourceNode::kNoSlot);
}

// the read side of a storage-texture binding, where the suite only exercised storage_write
TEST_CASE("RenderGraph - storage_read on a texture accumulates StorageBinding", "[RenderGraph]")
{
    using webgpu::rg::Internal::find_node;

    TestGraph g;
    auto tex = g.transient("tex");

    g.rg->add_pass(
        "produce", PassKind::Compute, [&](PassBuilder& b) { b.storage_write(tex); }, [](PassContext&) {});
    g.rg->add_pass(
        "read", PassKind::Compute,
        [&](PassBuilder& b) {
            b.storage_read(tex);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE((find_node(g.rg, tex)->texUsage & WGPUTextureUsage_StorageBinding) != 0);
}

// end_pass records one InitTarget per call, and phase 0 arms the whole pass while any target is stale
TEST_CASE("RenderGraph - initialize() records multiple targets in one pass", "[RenderGraph]")
{
    TestGraph g;
    auto p1 = g.rg->create_persistent_texture("p1", tex2d());
    auto p2 = g.rg->create_persistent_texture("p2", tex2d());

    g.rg->add_pass(
        "bake", PassKind::Graphics,
        [&](PassBuilder& b) {
            b.color(p1, 0); // MRT: both targets written
            b.color(p2, 1);
            b.initialize(p1);
            b.initialize(p2);
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
    REQUIRE(pass_order(g.rg).size() == 1); // armed on the first frame, not skipped
    REQUIRE(storage(g.rg)->m_passes->initCount == 2); // both targets recorded
}

// the compile tests above only check slot bookkeeping, this proves the shared object reaches execute()
TEST_CASE("RenderGraph exec - disjoint transients realize onto the same physical texture", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTexture a = nullptr, b = nullptr;

    g.frame([&](RenderGraph* rg) {
        auto t1 = rg->create_transient_texture("t1", tex2d());
        auto t2 = rg->create_transient_texture("t2", tex2d());
        // t1 lives only in k1, t2 only in k2 -> disjoint intervals -> alias onto one slot
        rg->add_pass(
            "k1", PassKind::Graphics,
            [&](PassBuilder& bb) {
                bb.color(t1, 0);
                bb.force_keep();
            },
            [t1, out = &a](PassContext& ctx) { *out = ctx.texture(t1); });
        rg->add_pass(
            "k2", PassKind::Graphics,
            [&](PassBuilder& bb) {
                bb.color(t2, 0);
                bb.force_keep();
            },
            [t2, out = &b](PassContext& ctx) { *out = ctx.texture(t2); });
    });

    REQUIRE(a != nullptr);
    REQUIRE(a == b); // both aliased transients got the one pooled physical texture
    REQUIRE(list_count(g.allocator->transient.entries) == 1); // a single object served both
}

// ---------------------------------------------------------------------------------------------------
// PassContext resolvers. the suite above asserts on the compiled graph with empty bodies, these run
// bodies and assert on what ctx hands back, the half a real renderer touches.

// two passes read one texture through different shapes, so they must not get the same view. also pins
// the resolve_view cache: asking twice in one body returns one object.
TEST_CASE("RenderGraph exec - ctx.view hands back the shape the access declared", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTextureView cubeView = nullptr, cubeAgain = nullptr, plainView = nullptr;

    g.frame([&](RenderGraph* rg) {
        TextureDesc d = tex2d();
        d.absolute = { 16, 16, 6 }; // 6 layers: samplable as a cube, or as a plain 2D layer
        auto env = rg->create_transient_texture("env", d);
        // each reader needs its own attachment, and unlike the compile-only cube tests this executes
        auto outCube = rg->create_transient_texture("outCube", tex2d());
        auto outPlain = rg->create_transient_texture("outPlain", tex2d());

        // one layer per pass, six RGBA8 attachments would exceed maxColorAttachmentBytesPerSample
        static const char* kFaceNames[6] = { "face0", "face1", "face2", "face3", "face4", "face5" };
        for (uint32_t l = 0; l < 6; ++l)
            rg->add_pass(
                kFaceNames[l], PassKind::Graphics,
                [env, l](PassBuilder& b) { b.color(env, 0, { .clear = { 0, 0, 0, 1 }, .sub = { .layer = l } }); },
                [](PassContext&) {});
        rg->add_pass(
            "readCube", PassKind::Graphics,
            [env, outCube](PassBuilder& b) {
                b.sampled(env, cube());
                b.color(outCube, 0);
                b.force_keep();
            },
            [env, cv = &cubeView, ca = &cubeAgain](PassContext& ctx) {
                *cv = ctx.view(env);
                *ca = ctx.view(env);
            });
        rg->add_pass(
            "readPlain", PassKind::Graphics,
            [env, outPlain](PassBuilder& b) {
                b.sampled(env); // default ViewRange: one layer, 2D
                b.color(outPlain, 0);
                b.force_keep();
            },
            [env, pv = &plainView](PassContext& ctx) { *pv = ctx.view(env); });
    });

    REQUIRE(cubeView != nullptr);
    REQUIRE(plainView != nullptr);
    REQUIRE(cubeView == cubeAgain); // one cached view per shape, not one per call
    REQUIRE(cubeView != plainView); // the declared range, not the texture, decides the view
}

// a blit sampling mip N-1 while rendering mip N gets two different views of the one texture
TEST_CASE("RenderGraph exec - ctx.view(mip, layer) resolves per subresource", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUTextureView srcMip = nullptr, dstMip = nullptr;

    g.frame([&](RenderGraph* rg) {
        TextureDesc d = tex2d();
        d.mipLevelCount = 4;
        auto chain = rg->create_transient_texture("chain", d);

        rg->add_pass(
            "mip0", PassKind::Graphics,
            [chain](PassBuilder& b) { b.color(chain, 0); },
            [](PassContext&) {});
        rg->add_pass(
            "mip1", PassKind::Graphics,
            [chain](PassBuilder& b) {
                b.sampled(chain); // read mip 0
                b.color(chain, 0, { .sub = { .mip = 1 } }); // write mip 1
                b.force_keep();
            },
            [chain, s = &srcMip, dm = &dstMip](PassContext& ctx) {
                *s = ctx.view(chain, 0, 0);
                *dm = ctx.view(chain, 1, 0);
            });
    });

    REQUIRE(srcMip != nullptr);
    REQUIRE(dstMip != nullptr);
    REQUIRE(srcMip != dstMip); // one view per subresource, not one per texture
}

// size 0 means the rest of the src, expanded at declare time, so the body sees a resolved non-zero size
TEST_CASE("RenderGraph exec - ctx.buffer_copy_info reports the resolved range", "[RenderGraph][gpu]")
{
    ExecGraph g;
    PassContext::BufferCopyInfo info {};

    g.frame([&](RenderGraph* rg) {
        BufferDesc bd {};
        bd.size = 64;
        auto src = rg->create_transient_buffer("src", bd);
        auto dst = rg->create_transient_buffer("dst", bd);

        add_buffer_producer(rg, src);
        rg->add_pass(
            "copy", PassKind::Transfer,
            [src, dst](PassBuilder& b) {
                b.copy_buffer(src, dst, /*srcOffset*/ 16, /*dstOffset*/ 8, /*size*/ 0); // -> 64 - 16 = 48
                b.force_keep();
            },
            [src, dst, out = &info](PassContext& ctx) { *out = ctx.buffer_copy_info(src, dst); });
    });

    REQUIRE(info.size == 48); // expanded against the src, not left at 0
    REQUIRE(info.srcOffset == 16);
    REQUIRE(info.dstOffset == 8);
    REQUIRE(info.src != nullptr);
    REQUIRE(info.dst != nullptr);
    REQUIRE(info.src != info.dst);
}

// a mip halves a 3D texture's depth but never an array's layer count. texture_size answers from the
// declaration alone, so neither import needs realizing.
TEST_CASE("RenderGraph exec - ctx.texture_size shifts 3D depth but not array layers", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUExtent3D vol0 {}, vol1 {}, arr0 {}, arr1 {};

    g.frame([&](RenderGraph* rg) {
        auto vol = rg->import_texture("vol", { .view = (WGPUTextureView)0x1, .size = { 16, 16, 8 }, .format = WGPUTextureFormat_RGBA8Unorm, .mipCount = 4, .dimension = WGPUTextureDimension_3D });
        auto arr = rg->import_texture("arr", { .view = (WGPUTextureView)0x1, .size = { 16, 16, 8 }, .format = WGPUTextureFormat_RGBA8Unorm, .mipCount = 4 });

        auto t = rg->create_transient_texture("probe", tex2d());
        rg->add_pass(
            "probe", PassKind::Graphics,
            [t](PassBuilder& b) {
                b.color(t, 0);
                b.force_keep();
            },
            [vol, arr, v0 = &vol0, v1 = &vol1, a0 = &arr0, a1 = &arr1](PassContext& ctx) {
                *v0 = ctx.texture_size(vol, 0);
                *v1 = ctx.texture_size(vol, 1);
                *a0 = ctx.texture_size(arr, 0);
                *a1 = ctx.texture_size(arr, 1);
            });
    });

    REQUIRE(vol0.depthOrArrayLayers == 8);
    REQUIRE(vol1.width == 8);
    REQUIRE(vol1.height == 8);
    REQUIRE(vol1.depthOrArrayLayers == 4); // 3D: depth is a spatial axis, it halves

    REQUIRE(arr0.depthOrArrayLayers == 8);
    REQUIRE(arr1.width == 8);
    REQUIRE(arr1.height == 8);
    REQUIRE(arr1.depthOrArrayLayers == 8); // 2D array: every layer keeps its own mip chain
}

// a body uploads through the forwarded queue, a later copy moves it to an imported mappable buffer, the
// host reads it back. nothing else in the suite runs a host_write body or touches ctx.queue.
TEST_CASE("RenderGraph exec - a host_write body uploads through ctx.queue", "[RenderGraph][gpu]")
{
    constexpr uint32_t kMagic = 0xABCD1234u;

    ExecGraph g;
    webgpu::raii::RawBuffer<uint8_t> readback(g.gpu.device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, 4, "hostwrite readback");

    g.frame([&](RenderGraph* rg) {
        BufferDesc bd {};
        bd.size = 4;
        auto staged = rg->create_transient_buffer("staged", bd);
        auto out = rg->import_buffer("hostwrite.readback", readback.handle());

        rg->add_pass(
            "upload", PassKind::Transfer, [staged](PassBuilder& b) { b.host_write(staged); },
            [staged, magic = kMagic](PassContext& ctx) {
                wgpuQueueWriteBuffer(ctx.queue, ctx.buffer(staged), 0, &magic, sizeof(magic));
            });
        rg->add_pass(
            "copy", PassKind::Transfer,
            [staged, out](PassBuilder& b) { b.copy_buffer(staged, out, 0, 0, 4); },
            [staged, out](PassContext& ctx) {
                auto info = ctx.buffer_copy_info(staged, out);
                wgpuCommandEncoderCopyBufferToBuffer(ctx.encoder, info.src, info.srcOffset, info.dst, info.dstOffset, info.size);
            });
    });

    std::vector<uint8_t> bytes;
    REQUIRE(readback.read_back_sync(g.gpu.instance, g.gpu.device, bytes) == WGPUMapAsyncStatus_Success);
    REQUIRE(bytes.size() == 4);
    uint32_t got = 0;
    std::memcpy(&got, bytes.data(), sizeof(got));
    REQUIRE(got == kMagic); // the queue write landed and the copy carried it
}

// ---------------------------------------------------------------------------------------------------
// import_buffer and PassContext::bind. import_buffer takes no BufferDesc, so unlike every other buffer
// factory its size is queried from the imported object rather than declared.

TEST_CASE("RenderGraph exec - an imported buffer reports the size queried from the buffer", "[RenderGraph][gpu]")
{
    constexpr uint64_t kSize = 256;

    ExecGraph g;
    webgpu::raii::RawBuffer<uint8_t> owned(g.gpu.device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, kSize, "imported");

    uint64_t reported = 0;
    WGPUBuffer resolved = nullptr;
    WGPUBindGroupEntry entry {};

    g.frame([&](RenderGraph* rg) {
        auto imported = rg->import_buffer("imported", owned.handle());
        rg->add_pass(
            "read", PassKind::Compute,
            [imported](PassBuilder& b) {
                b.storage_read(imported); // an import needs no writer, so this alone is a legal graph
                b.force_keep();
            },
            [imported, sz = &reported, buf = &resolved, e = &entry](PassContext& ctx) {
                *sz = ctx.buffer_size(imported);
                *buf = ctx.buffer(imported);
                *e = ctx.bind(7, imported);
            });
    });

    REQUIRE(reported == kSize); // wgpuBufferGetSize, not a declared BufferDesc
    REQUIRE(resolved == owned.handle()); // the caller's buffer, not a pooled copy

    // the buffer arm of bind(): whole, at offset 0, per the PassContext contract
    REQUIRE(entry.binding == 7);
    REQUIRE(entry.buffer == resolved);
    REQUIRE(entry.offset == 0);
    REQUIRE(entry.size == kSize);
    REQUIRE(entry.textureView == nullptr);
}

// a declared BufferRange is the shape ctx.bind() hands back, same contract as ViewRange for textures
TEST_CASE("RenderGraph exec - bind applies the declared BufferRange", "[RenderGraph][gpu]")
{
    ExecGraph g;
    webgpu::raii::RawBuffer<uint8_t> owned(g.gpu.device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, 512, "imported");

    WGPUBindGroupEntry explicitRange {};
    WGPUBindGroupEntry restRange {};

    g.frame([&](RenderGraph* rg) {
        auto imported = rg->import_buffer("imported", owned.handle());
        rg->add_pass(
            "explicit", PassKind::Compute,
            [imported](PassBuilder& b) {
                b.storage_read(imported, { .offset = 256, .size = 128 });
                b.force_keep();
            },
            [imported, e = &explicitRange](PassContext& ctx) { *e = ctx.bind(0, imported); });
    });
    g.frame([&](RenderGraph* rg) {
        auto imported = rg->import_buffer("imported", owned.handle());
        rg->add_pass(
            "rest", PassKind::Compute,
            [imported](PassBuilder& b) {
                b.storage_read(imported, { .offset = 256 }); // size 0 = all remaining
                b.force_keep();
            },
            [imported, e = &restRange](PassContext& ctx) { *e = ctx.bind(0, imported); });
    });

    REQUIRE(explicitRange.offset == 256);
    REQUIRE(explicitRange.size == 128);
    REQUIRE(restRange.offset == 256);
    REQUIRE(restRange.size == 256); // resolved at declare time against the queried size
}

// the queried size is not merely reported back: it is the bound compile() range-checks copies against,
// which is the only thing standing between an oversized copy and a device error.
TEST_CASE("compile - a copy past an imported buffer's queried size is rejected", "[RenderGraph][gpu]")
{
    ExecGraph g;
    webgpu::raii::RawBuffer<uint8_t> owned(g.gpu.device, WGPUBufferUsage_CopyDst, 64, "small");

    begin_frame(g.allocator);
    RenderGraph* rg = start_recording(g.allocator);

    BufferDesc bd {};
    bd.size = 128;
    auto src = rg->create_transient_buffer("src", bd);
    auto dst = rg->import_buffer("dst", owned.handle());

    add_buffer_producer(rg, src);
    rg->add_pass(
        "copy", PassKind::Transfer,
        [src, dst](PassBuilder& b) {
            b.copy_buffer(src, dst, 0, 0, 128); // the src fits, the imported dst does not
            b.force_keep();
        },
        [](PassContext&) {});

    REQUIRE(rg->compile() != nullptr);
    REQUIRE(error_mentions(rg, "covers bytes [0, 128) but the buffer is 64 byte(s)"));
}

// bind(binding, ResourceHandle) switches on the handle's kind, so a texture must fill the view arm and
// leave the buffer fields at zero. a wrong arm here is a silently invalid bind group.
TEST_CASE("RenderGraph exec - bind on a texture handle fills the view arm", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUBindGroupEntry entry {};
    WGPUTextureView direct = nullptr;

    g.frame([&](RenderGraph* rg) {
        auto tex = rg->create_transient_texture("tex", tex2d());
        auto out = rg->create_transient_texture("out", tex2d());

        rg->add_pass(
            "produce", PassKind::Graphics, [tex](PassBuilder& b) { b.color(tex, 0); }, [](PassContext&) {});
        rg->add_pass(
            "read", PassKind::Graphics,
            [tex, out](PassBuilder& b) {
                b.sampled(tex);
                b.color(out, 0);
                b.force_keep();
            },
            [tex, e = &entry, d = &direct](PassContext& ctx) {
                *e = ctx.bind(3, tex); // two args, so this is the ResourceHandle overload
                *d = ctx.view(tex);
            });
    });

    REQUIRE(entry.binding == 3);
    REQUIRE(entry.textureView != nullptr);
    REQUIRE(entry.textureView == direct); // the same view the declared access resolves to, not a fresh one
    REQUIRE(entry.buffer == nullptr);
    REQUIRE(entry.size == 0);
}

// the subresource overload must forward to view(mip, layer). binding the whole-texture view instead
// would compile and run, just sample the wrong level.
TEST_CASE("RenderGraph exec - bind(mip, layer) carries the per-subresource view", "[RenderGraph][gpu]")
{
    ExecGraph g;
    WGPUBindGroupEntry mip0 {}, mip1 {};

    g.frame([&](RenderGraph* rg) {
        TextureDesc d = tex2d();
        d.mipLevelCount = 4;
        auto chain = rg->create_transient_texture("chain", d);

        rg->add_pass(
            "mip0", PassKind::Graphics, [chain](PassBuilder& b) { b.color(chain, 0); }, [](PassContext&) {});
        rg->add_pass(
            "mip1", PassKind::Graphics,
            [chain](PassBuilder& b) {
                b.sampled(chain); // mip 0
                b.color(chain, 0, { .sub = { .mip = 1 } }); // mip 1
                b.force_keep();
            },
            [chain, a = &mip0, b = &mip1](PassContext& ctx) {
                *a = ctx.bind(0, chain, 0, 0);
                *b = ctx.bind(1, chain, 1, 0);
            });
    });

    REQUIRE(mip0.binding == 0);
    REQUIRE(mip1.binding == 1);
    REQUIRE(mip0.textureView != nullptr);
    REQUIRE(mip1.textureView != nullptr);
    REQUIRE(mip0.textureView != mip1.textureView); // one view per subresource
    REQUIRE(mip0.buffer == nullptr);
    REQUIRE(mip1.buffer == nullptr);
}

// ctx.view() branches on whether the import supplied a backing texture, not on `imported` itself.
// view-only: the caller owns the view and the graph hands it back untouched, so a declared ViewRange
// selects nothing. texture-backed: the graph builds the view from the range like any owned texture.
// docs/rendergraph.md states both halves, in the Import section and the IBL example.
TEST_CASE("execute - a view-only import returns the registered view, a texture-backed one builds its own", "[RenderGraph]")
{
    ExecGraph g;

    WGPUTextureDescriptor td {};
    td.dimension = WGPUTextureDimension_2D;
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.size = { 16, 16, 6 }; // 6 layers, so a cube view over it is well formed
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding;
    WGPUTexture tex = wgpuDeviceCreateTexture(g.gpu.device, &td);
    REQUIRE(tex != nullptr);

    WGPUTextureViewDescriptor vd {};
    vd.format = WGPUTextureFormat_RGBA8Unorm;
    vd.dimension = WGPUTextureViewDimension_Cube;
    vd.mipLevelCount = 1;
    vd.arrayLayerCount = 6;
    vd.aspect = WGPUTextureAspect_All;
    WGPUTextureView callerView = wgpuTextureCreateView(tex, &vd);
    REQUIRE(callerView != nullptr);

    WGPUTextureView seenViewOnly = nullptr;
    WGPUTextureView seenBacked = nullptr;

    g.frame([&](RenderGraph* rg) {
        // no .texture: the graph has nothing to build a view from
        auto viewOnly = rg->import_texture(
            "ibl.env.view_only", { .view = callerView, .size = { 16, 16, 6 }, .format = WGPUTextureFormat_RGBA8Unorm });
        // same view registered, but the backing texture comes along too
        auto backed = rg->import_texture("ibl.env.backed",
            { .view = callerView, .size = { 16, 16, 6 }, .format = WGPUTextureFormat_RGBA8Unorm, .texture = tex });

        rg->add_pass(
            "read", PassKind::Compute,
            [viewOnly, backed](PassBuilder& b) {
                // a narrowing range on both, to prove only one of them acts on it. base stays 0 on the
                // view-only side: a non-zero base there is an assert, not a silently ignored range.
                b.sampled(viewOnly, ViewRange { .layerCount = 1 });
                // a layer the caller's whole-cube view cannot be, so an equal result would be a real match
                b.sampled(backed, ViewRange { .baseLayer = 2, .layerCount = 1 });
                b.force_keep();
            },
            [viewOnly, backed, a = &seenViewOnly, b = &seenBacked](PassContext& c) {
                *a = c.view(viewOnly);
                *b = c.view(backed);
            });
    });

    REQUIRE(seenViewOnly == callerView); // handed back as registered, the 1-layer range ignored
    REQUIRE(seenBacked != nullptr);
    REQUIRE(seenBacked != callerView); // built by the graph, layer 2 alone, not the caller's whole cube

    wgpuTextureViewRelease(callerView);
    wgpuTextureRelease(tex);
}

// The remaining worked examples from docs/rendergraph.md, transcribed to their graph-level calls with
// app-side pipelines and draws stubbed. Each proves the example is a valid graph program and catches
// API drift like the color() signature change. Bodies stop at declaration; compile() is the check.

TEST_CASE("compile - doc example: deferred shading, G-buffer plus compute lighting", "[RenderGraph]")
{
    TestGraph g;
    const WGPUExtent3D size { 64, 64, 1 };
    auto desc = [&](WGPUTextureFormat fmt) {
        TextureDesc d {};
        d.dimension = WGPUTextureDimension_2D;
        d.format = fmt;
        d.absolute = size;
        return d;
    };
    auto albedo = g.rg->create_transient_texture("gbuffer.albedo", desc(WGPUTextureFormat_RGBA8Unorm));
    auto normal = g.rg->create_transient_texture("gbuffer.normal", desc(WGPUTextureFormat_RGBA16Float));
    auto depth = g.rg->create_transient_texture("gbuffer.depth", desc(WGPUTextureFormat_Depth32Float));
    auto lit = g.rg->create_transient_texture("lit.color", desc(WGPUTextureFormat_RGBA16Float));

    g.rg->add_pass(
        "GBuffer", PassKind::Graphics,
        [albedo, normal, depth](PassBuilder& b) {
            b.color(albedo, 0, { .clear = { 0, 0, 0, 0 } });
            b.color(normal, 1, { .clear = { 0, 0, 0, 0 } });
            b.depth_stencil(depth, { .clearDepth = 0.0f }); // reverse-Z
        },
        [](PassContext&) {});

    g.rg->add_pass(
        "Lighting", PassKind::Compute,
        [albedo, normal, depth, lit](PassBuilder& b) {
            b.sampled(albedo);
            b.sampled(normal);
            b.sampled(depth); // depth-only format, no aspect needed
            b.storage_write(lit); // ordered after GBuffer by the reads above
            b.force_keep(); // stands in for the tonemap/composite consumer the example returns lit to
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr);
}

TEST_CASE("compile - doc example: bloom, relative-sized downsample chain", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc fullDesc {};
    fullDesc.dimension = WGPUTextureDimension_2D;
    fullDesc.format = WGPUTextureFormat_RGBA16Float;
    fullDesc.absolute = { 128, 128, 1 };
    auto hdrColor = g.rg->create_transient_texture("hdr.color", fullDesc);

    // the example assumes hdrColor already exists; give it a producer
    g.rg->add_pass(
        "scene", PassKind::Graphics, [hdrColor](PassBuilder& b) { b.color(hdrColor, 0); }, [](PassContext&) {});

    TextureDesc halfDesc {};
    halfDesc.dimension = WGPUTextureDimension_2D;
    halfDesc.format = WGPUTextureFormat_RGBA16Float;
    halfDesc.sizeKind = SizeKind::Relative;
    halfDesc.scaleX = 0.5f;
    halfDesc.scaleY = 0.5f;
    halfDesc.relativeTo = hdrColor;
    auto bright = g.rg->create_transient_texture("bloom.bright", halfDesc);
    auto blurH = g.rg->create_transient_texture("bloom.blur_h", halfDesc);
    auto blurV = g.rg->create_transient_texture("bloom.blur_v", halfDesc);

    auto blit = [&](const char* name, TextureHandle src, TextureHandle dst) {
        g.rg->add_pass(
            name, PassKind::Graphics,
            [src, dst](PassBuilder& b) {
                b.sampled(src);
                b.color(dst, 0);
            },
            [](PassContext&) {});
    };
    blit("bloom.threshold", hdrColor, bright);
    blit("bloom.blurH", bright, blurH);
    blit("bloom.blurV", blurH, blurV);

    g.rg->add_pass(
        "bloom.composite", PassKind::Graphics,
        [hdrColor, blurV](PassBuilder& b) {
            b.color(hdrColor, 0, { .load = WGPULoadOp_Load }); // load, do not clear
            b.sampled(blurV);
            b.force_keep(); // stands in for the swapchain sink downstream
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr); // Relative sizing resolves against hdrColor
}

TEST_CASE("compile - doc example: TAA, history plus a camera-cut hash", "[RenderGraph]")
{
    TestGraph g;
    TextureDesc colorDesc {};
    colorDesc.dimension = WGPUTextureDimension_2D;
    colorDesc.format = WGPUTextureFormat_RGBA16Float;
    colorDesc.absolute = { 64, 64, 1 };

    auto currentColor = g.rg->create_transient_texture("current.color", colorDesc);
    auto velocity = g.rg->create_transient_texture("velocity", colorDesc);

    g.rg->add_pass(
        "scene", PassKind::Graphics,
        [currentColor, velocity](PassBuilder& b) {
            b.color(currentColor, 0);
            b.color(velocity, 1);
        },
        [](PassContext&) {});

    uint64_t cut = 5; // changes on teleport, projection swap, etc.
    auto taa = g.rg->create_history_texture("taa.color", colorDesc, cut);

    g.rg->add_pass(
        "TAA", PassKind::Graphics,
        [currentColor, velocity, taa](PassBuilder& b) {
            b.sampled(currentColor);
            b.sampled(velocity);
            b.sampled(taa.prev); // last frame's result
            b.color(taa.curr, 0); // this frame's result, becomes next frame's prev
        },
        [](PassContext&) {});

    REQUIRE(g.rg->compile() == nullptr); // writing taa.curr is a sink, so no force_keep needed
}
