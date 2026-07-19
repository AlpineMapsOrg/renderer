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


// immediate-mode render graph for WebGPU. does pass ordering and resource lifetime

#pragma once


#include <cstdint>
#include <initializer_list>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>
#include <QtAssert>
#include <webgpu/webgpu.h>

namespace webgpu::rg {

// picks the encoder the pass records into, and which accesses are legal
enum struct PassKind : uint8_t {
    None = 0,
    Graphics, // PassContext::render_pass, attachments already bound
    Compute, // PassContext::compute_pass
    Transfer // no pass object, copies go on PassContext::encoder
};

enum struct SizeKind : uint8_t {
    Absolute,
    Relative // scaled relative to relativeTo
};

enum struct ResourceKind : uint8_t {
    Texture,
    Buffer,
};

// handle for one a render graph resource for this frame
struct ResourceHandle {
    uint32_t id {};
    ResourceKind kind {};
    uint64_t generation {};

    constexpr explicit operator bool() const { return id != 0; }
    constexpr bool operator==(const ResourceHandle&) const = default;
};

struct TextureHandle : ResourceHandle {};
struct BufferHandle : ResourceHandle {};

// ping-pong pair, rotated each frame. this frame's curr is next frame's prev.
template <typename H>
struct History {
    H curr; // written this frame
    H prev; // last frame's curr
};
using HistoryTexture = History<TextureHandle>;
using HistoryBuffer = History<BufferHandle>;

namespace Internal {
struct ResourceNode;
struct PassNode;
struct GraphPair;
struct PassContextAccess;
}
struct GraphAllocator;
struct RenderGraph;

struct ErrorMessage {
    WGPUStringView message;
    ErrorMessage* next {};
};

// resolves handles to live GPU objects during execute()
struct PassContext {
    WGPUCommandEncoder encoder {};
    WGPURenderPassEncoder render_pass {}; // Graphics passes, else null
    WGPUComputePassEncoder compute_pass {}; // Compute passes, else null
    WGPUQueue queue {};
    WGPUDevice device {};

    // shape comes from this pass's access declaration, ViewRange included
    WGPUTextureView view(TextureHandle h) const;
    WGPUTextureView view(TextureHandle h, uint32_t mip, uint32_t layer = 0) const;

    // buffers bind at the BufferRange declared in this pass, or whole at offset 0 when none was
    WGPUBindGroupEntry bind(uint32_t binding, ResourceHandle h) const;
    WGPUBindGroupEntry bind(uint32_t binding, TextureHandle h, uint32_t mip, uint32_t layer = 0) const;

    // all assert the handle has a declared access in this pass
    WGPUTexture texture(TextureHandle h) const;
    WGPUBuffer buffer(BufferHandle h) const;
    WGPUExtent3D texture_size(TextureHandle h) const;
    WGPUExtent3D texture_size(TextureHandle h, uint32_t mip) const;
    uint64_t buffer_size(BufferHandle h) const;
    WGPUTextureFormat format(TextureHandle h) const;
    uint32_t mip_count(TextureHandle h) const;
    uint32_t sample_count(TextureHandle h) const;

    // false on the frame .prev was (re)created and cleared to 0
    template <typename H>
    bool history_valid(const History<H>& history) const
    {
        return history_valid_impl(history.prev);
    }

    // copy descriptors for a copy declared in this pass, declared mip/layer applied
    WGPUTexelCopyTextureInfo copy_src_info(TextureHandle h, WGPUOrigin3D origin = {}, WGPUTextureAspect aspect = WGPUTextureAspect_All) const;
    WGPUTexelCopyTextureInfo copy_dst_info(TextureHandle h, WGPUOrigin3D origin = {}, WGPUTextureAspect aspect = WGPUTextureAspect_All) const;
    // the declared subresource at its mip level, never the whole array
    WGPUExtent3D copy_extent_src(TextureHandle h) const;
    WGPUExtent3D copy_extent_dst(TextureHandle h) const;
    // buffer side of a texture<->buffer copy. layout is the caller's data layout.
    WGPUTexelCopyBufferInfo copy_src_buffer(BufferHandle h, WGPUTexelCopyBufferLayout layout) const;
    WGPUTexelCopyBufferInfo copy_dst_buffer(BufferHandle h, WGPUTexelCopyBufferLayout layout) const;

    // size is already expanded, never 0
    struct BufferCopyInfo {
        WGPUBuffer src {};
        uint64_t srcOffset {};
        WGPUBuffer dst {};
        uint64_t dstOffset {};
        uint64_t size {};
    };
    BufferCopyInfo buffer_copy_info(BufferHandle src, BufferHandle dst) const;

private:
    RenderGraph* graph {};
    Internal::PassNode* pass {};

    bool history_valid_impl(ResourceHandle prev) const;

    PassContext() = default;
    PassContext(const PassContext&) = delete;
    PassContext& operator=(const PassContext&) = delete;
    friend struct RenderGraph;
    friend struct Internal::PassContextAccess;
};

// exactly one mip level and one array layer
struct Subresource {
    uint32_t mip = 0;
    uint32_t layer = 0;
};

// view shape for a sampled/storage access. the default is one mip, one layer, all aspects, dimension
// inferred.
struct ViewRange {
    uint32_t baseMip = 0; // first mip level the view sees
    uint32_t mipCount = 1; // from baseMip. 0 = all remaining.
    uint32_t baseLayer = 0; // first array layer the view sees
    uint32_t layerCount = 1; // from baseLayer. 0 = all remaining.
    // Undefined infers 3D for a 3D texture, else 2DArray when the view covers more than one layer, else 2D
    WGPUTextureViewDimension dim = WGPUTextureViewDimension_Undefined;
    WGPUTextureAspect aspect = WGPUTextureAspect_All; // picks one plane of a depth-stencil texture

    // same shape, rebased
    constexpr ViewRange at(uint32_t mip, uint32_t layer = 0) const
    {
        ViewRange r = *this;
        r.baseMip = mip;
        r.baseLayer = layer;
        return r;
    }
};

// cube view, exactly 6 layers. sampling only, WebGPU storage textures cannot be cube.
constexpr ViewRange cube(WGPUTextureAspect aspect = WGPUTextureAspect_All, uint32_t mipCount = 1)
{
    return ViewRange { .mipCount = mipCount, .layerCount = 6, .dim = WGPUTextureViewDimension_Cube, .aspect = aspect };
}

// cube array view, 6*cubes layers
constexpr ViewRange cube_array(uint32_t cubes, WGPUTextureAspect aspect = WGPUTextureAspect_All, uint32_t mipCount = 1)
{
    return ViewRange { .mipCount = mipCount, .layerCount = 6 * cubes, .dim = WGPUTextureViewDimension_CubeArray, .aspect = aspect };
}

// all mips and layers from baseMip/baseLayer, dimension inferred
constexpr ViewRange whole(WGPUTextureAspect aspect = WGPUTextureAspect_All)
{
    return ViewRange { .mipCount = 0, .layerCount = 0, .aspect = aspect };
}

// byte range of a buffer binding. the default binds the whole buffer
struct BufferRange {
    uint64_t offset = 0; // must be a multiple of 256 according to the spec
    uint64_t size = 0; // from offset. 0 = all remaining.
};

// the 256-byte row alignment WebGPU requires for buffer<->texture copies
inline uint32_t aligned_bytes_per_row(uint32_t widthTexels, uint32_t texelBlockBytes)
{
    constexpr uint32_t align = 256u;
    uint32_t raw = widthTexels * texelBlockBytes;
    return (raw + (align - 1u)) & ~(align - 1u);
}

// how a color attachment is bound. the default is clear to black, store, mip 0, layer 0.
struct ColorAttachment {
    WGPULoadOp load = WGPULoadOp_Clear; // Load keeps the previous contents, Clear overwrites with clear
    WGPUStoreOp store = WGPUStoreOp_Store; // Discard when nothing reads the result afterwards
    WGPUColor clear = { 0, 0, 0, 1 }; // used only with WGPULoadOp_Clear
    Subresource sub {}; // which mip and layer to render into
};

// left Undefined, the graph fills them in from the format.
struct DepthStencilAttachment {
    WGPULoadOp load = WGPULoadOp_Clear; // depth plane. Load keeps last frame's depth
    WGPUStoreOp store = WGPUStoreOp_Store; // Discard when no later pass samples the depth
    float clearDepth = 1.0f; // used only with WGPULoadOp_Clear. 1.0 is the far plane
    Subresource sub {}; // which mip and layer to render into
    WGPULoadOp stencilLoad = WGPULoadOp_Undefined; // Undefined derives from the format, set only to override
    WGPUStoreOp stencilStore = WGPUStoreOp_Undefined; // Undefined derives from the format, set only to override
    uint32_t stencilClear = 0; // used only with a stencil clear
};

// declares one pass's resource accesses. one call per use.
struct PassBuilder {
    // attachmentIndex is the fragment shader @location. slots may be sparse.
    void color(TextureHandle handle, uint32_t attachmentIndex, ColorAttachment attachment = {});
    void depth_stencil(TextureHandle handle, DepthStencilAttachment attachment = {});
    // test only, no write
    void depth_stencil_read_only(TextureHandle handle, Subresource sub = {});
    // MSAA resolve of src into target
    void resolve(TextureHandle src, TextureHandle target, Subresource sub = {});
    void sampled(TextureHandle handle, ViewRange range = {});
    void storage_read(TextureHandle handle, ViewRange range = {});
    void storage_write(TextureHandle handle, ViewRange range = {});
    void storage_read_write(TextureHandle handle, ViewRange range = {});
    // storage buffer. range picks the bytes ctx.bind() binds; one range per buffer per pass.
    void storage_read(BufferHandle handle, BufferRange range = {});
    void storage_write(BufferHandle handle, BufferRange range = {});
    void storage_read_write(BufferHandle handle, BufferRange range = {});
    void uniform(BufferHandle handle, BufferRange range = {});
    void host_write(BufferHandle handle);
    void copy_texture(TextureHandle src, TextureHandle dst, Subresource srcSub = {}, Subresource dstSub = {});

    void copy_texture_to_buffer(TextureHandle src, BufferHandle dst, Subresource srcSub = {});
    void copy_buffer_to_texture(BufferHandle src, TextureHandle dst, Subresource dstSub = {});

    // size 0 means the whole src buffer from srcOffset
    void copy_buffer(BufferHandle src, BufferHandle dst, uint64_t srcOffset = 0, uint64_t dstOffset = 0, uint64_t size = 0);
    void vertex_buffer(BufferHandle handle);
    void index_buffer(BufferHandle handle);
    void indirect_buffer(BufferHandle handle);

    // bake pass for a pool-backed resource. runs only when the target is stale.
    void initialize(ResourceHandle target, uint64_t hash = 0);

    // exempts this pass from dead-pass culling
    void force_keep();

private:
    Internal::PassNode* m_pass {};
    RenderGraph* m_graph {};

    PassBuilder() = default;
    PassBuilder(const PassBuilder&) = delete;
    PassBuilder& operator=(const PassBuilder&) = delete;
    friend struct RenderGraph;
};

// usage flags are inferred from how passes declare the resource
struct TextureDesc {
    WGPUTextureDimension dimension = WGPUTextureDimension_Undefined; // set 2D or 3D explicitly, Undefined is not normalized. 1D is unsupported.
    WGPUTextureFormat format = WGPUTextureFormat_Undefined; // required, there is no default
    SizeKind sizeKind = SizeKind::Absolute; // picks between absolute and the scale/relativeTo pair
    float scaleX = 1.0f; // Relative only: factor of relativeTo's size
    float scaleY = 1.0f; // Relative only
    TextureHandle relativeTo {}; // Relative only: the texture whose size is scaled
    WGPUExtent3D absolute = WGPU_EXTENT_3D_INIT; // Absolute only: size in texels, depthOrArrayLayers is the layer count for 2D
    uint32_t mipLevelCount = 1; // full chain must be spelled out, there is no 0 = all
    uint32_t sampleCount = 1; // 1 or 4 (WebGPU 1.0 limit)
};

struct BufferDesc {
    uint64_t size {}; // bytes
};

// an object the graph does not own. a null texture registers the view only, which rules out
// ctx.texture() and the copy family. dimension is the source texture's own, so an array or cube source
// says 2D.
struct ImportTextureDesc {
    WGPUTextureView view = nullptr; // required, what ctx.view() returns for this handle
    WGPUExtent3D size = WGPU_EXTENT_3D_INIT; // required, the graph cannot query an imported view
    WGPUTextureFormat format = WGPUTextureFormat_Undefined; // required, must match the view's format
    WGPUTexture texture = nullptr; // optional, enables ctx.texture() and the copy family
    uint32_t mipCount = 1; // of the underlying texture, only relevant when texture is set
    uint32_t sampleCount = 1; // must match the source, wrong value breaks attachment use
    WGPUTextureDimension dimension = WGPUTextureDimension_2D; // the source texture's own dimension, see the struct comment
};

// render graph api for creating resources and adding passes
struct RenderGraph {
    // lives for this frame, pooled and possibly aliased with another transient
    TextureHandle create_transient_texture(std::string_view id, const TextureDesc& desc);
    BufferHandle create_transient_buffer(std::string_view id, const BufferDesc& desc);

    // wraps an object the graph does not own, usually the swapchain. writing an import is a frame
    // output, so those passes are never culled.
    TextureHandle import_texture(std::string_view id, const ImportTextureDesc& desc);

    BufferHandle import_buffer(std::string_view id, WGPUBuffer buffer);

    // cross-frame ping-pong for temporal effects. hash != 0 resets history when the hash changes.
    HistoryTexture create_history_texture(std::string_view id, const TextureDesc& desc, uint64_t hash = 0);
    HistoryBuffer create_history_buffer(std::string_view id, const BufferDesc& desc, uint64_t hash = 0);

    // graph-owned, kept across frames
    BufferHandle create_persistent_buffer(std::string_view id, const BufferDesc& desc);

    // initialized once from data, or zero-filled
    BufferHandle create_initialized_buffer(std::string_view id, const BufferDesc& desc, const void* data = nullptr);

    TextureHandle create_persistent_texture(std::string_view id, const TextureDesc& desc);

    // cleared once to fill
    TextureHandle create_initialized_texture(std::string_view id, const TextureDesc& desc, WGPUColor fill);

    // setup(PassBuilder&) declares the pass's accesses now. executeFn(PassContext&) records the GPU
    // commands during execute(), and must be trivially destructible.
    template <typename BuilderFn, typename ExecuteFn>
    void add_pass(std::string_view id, PassKind kind, BuilderFn&& setup, ExecuteFn&& executeFn)
    {
        Q_ASSERT(!id.empty() && "must have name");
        Q_ASSERT(kind != PassKind::None);
        PassBuilder builder;
        begin_pass(builder, id, kind);
        setup(builder);
        store_exec(builder, std::forward<ExecuteFn>(executeFn));
        end_pass(builder);
    }

    // schedules the graph and freezes it. returns the authoring-error chain, null when ok.
    [[nodiscard]] ErrorMessage* compile(bool enableAlias = true);

    // records the surviving passes into encoder. does not submit.
    void execute(WGPUDevice device, WGPUQueue queue, WGPUCommandEncoder encoder, bool enableProfiling = false);

    // kicks the async read-back of the GPU timings
    void collect_gpu_timings();

    // returns the errors captured by compile()
    ErrorMessage* get_errors() const;

private:
    RenderGraph() = default;
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    friend struct Internal::GraphPair;

    template <class F>
    void store_exec(PassBuilder& b, F&& f)
    {
        using D = std::decay_t<F>;
        static_assert(std::is_trivially_destructible_v<D>,
            "execute callback must be trivially destructible; the arena frees without running destructors, so capture handles or ids by value");
        void* m = alloc_exec(sizeof(D), alignof(D)); // never null, arena OOM aborts
        ::new (m) D(std::forward<F>(f));
        set_exec(b, m, [](void* o, PassContext& c) { (*static_cast<D*>(o))(c); });
    }
    void* alloc_exec(size_t size, size_t align);
    void set_exec(PassBuilder& builder, void* obj, void (*fn)(void*, PassContext&));


    void begin_pass(PassBuilder& builder, std::string_view id, PassKind kind);
    void end_pass(PassBuilder& builder);
};

// backs all graphs, arenas and the GPU object pool. one per device.
GraphAllocator* create_allocator();

// destroys everything it owns, pooled GPU objects included
void destroy_allocator(GraphAllocator* allocator);

// all graphs and handles from the previous frame die here
void begin_frame(GraphAllocator* allocator);

// starts a frame-scoped graph. several per frame are allowed.
RenderGraph* start_recording(GraphAllocator* allocator);

// ages the pools so unused GPU objects get evicted
void end_frame(GraphAllocator* allocator);

} // namespace webgpu::rg
