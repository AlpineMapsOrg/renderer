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

// internal node and allocator layout. not public API, may change without notice.
// used for unit tests and for the RenderGraphPanel

#pragma once

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <utility>
#include <QtAssert>
#include <QDebug>
#include <webgpu/webgpu.h>

#include "RenderGraph.h"

namespace webgpu::rg {

// FNV-1a, the basis of ResourceId::value
inline constexpr uint64_t fnv1a(std::string_view s)
{
    uint64_t hash = 14695981039346656037ULL;
    for (char c : s) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// carries the name too, so a hash collision never conflates two names
struct ResourceId {
    uint64_t value {};
    WGPUStringView name {};
};

// borrows the name, so an id outliving the caller's string must intern it
constexpr ResourceId make_resource_id(std::string_view s) {
    return ResourceId(fnv1a(s), WGPUStringView { .data = s.data(), .length = s.length() });
}

constexpr bool operator==(ResourceId a, ResourceId b)
{
    return a.value == b.value && a.name.length == b.name.length &&
        std::string_view(a.name.data, a.name.length) == std::string_view(b.name.data, b.name.length);
}

} // namespace webgpu::rg

namespace webgpu::rg::Internal {

// TransientAttachments, a Dawn-native feature. by value, since the emdawnwebgpu web headers ship the
// usage bit without the feature enum.
static constexpr WGPUFeatureName kTransientAttachmentsFeature = (WGPUFeatureName)0x00050006u;

// WebGPU spec limit per render pass
static constexpr uint32_t kMaxColorAttachments = 8;

// drives hazard edges and usage bits (columns: category, direction, usage)
enum struct AccessType : uint8_t {
    ColorAttachment, // attachment      write   tex RenderAttachment
    DepthStencilAttachment, // attachment      write   tex RenderAttachment
    DepthStencilReadOnly, // attachment-read read    tex RenderAttachment  (test only, no write hazard)
    ResolveAttachment, // attachment      write   tex RenderAttachment
    Sampled, // constant        read    tex TextureBinding
    StorageRead, // storage-read    read    tex StorageBinding / buf Storage
    StorageWrite, // storage         write   tex StorageBinding / buf Storage
    Uniform, // constant        read    buf Uniform (+CopyDst)
    CopySrc, // copy            read    tex/buf CopySrc
    CopyDst, // copy            write   tex/buf CopyDst
    Vertex, // input           read    buf Vertex
    Index, // input           read    buf Index
    Indirect, // input           read    buf Indirect
};

// one recorded access on a pass. fields ordered wide-to-narrow to kill padding (~96B).
struct ResourceAccess {
    ResourceHandle handle {};

    // byte range of a copy_buffer() or a declared BufferRange on a uniform/storage bind. resolved at
    // declare time so a recorded size is never 0 for a valid range. texture accesses and whole-buffer
    // binds leave both 0.
    uint64_t bufOffset {};
    uint64_t bufSize {};

    // f32 not WGPUColor, lossless for every WebGPU color format at half the size
    float clearColor[4] {};
    float clearDepth {};

    // attachment only
    WGPULoadOp loadOp {};
    WGPUStoreOp storeOp {};
    // set for a depth+stencil format
    WGPULoadOp stencilLoadOp {};
    WGPUStoreOp stencilStoreOp {};

    WGPUTextureViewDimension viewDim { WGPUTextureViewDimension_Undefined }; // Undefined -> infer
    WGPUTextureAspect viewAspect { WGPUTextureAspect_All };

    // subresource + view range.
    uint16_t baseLayer {};
    uint16_t layerCount { 1 };
    AccessType type {};
    uint8_t stencilClear {};
    // color slot of a ColorAttachment, or of the color() a ResolveAttachment resolves. 0 otherwise.
    uint8_t colorIndex {};
    // buffer CopyDst only: provably overwrites the whole buffer. a copy_texture_to_buffer dst stays
    // false, its coverage is only known at encode time.
    bool bufFullDefine {};
    uint8_t baseMip {};
    uint8_t mipCount { 1 };
};

// resolves the WGPU_STRLEN sentinel
size_t sv_length(WGPUStringView s);

bool sv_eq(WGPUStringView a, WGPUStringView b);

// the span before the first '.', empty if none
WGPUStringView group_prefix(WGPUStringView name);

struct Arena;

struct ViewKey {
    WGPUTextureFormat format = WGPUTextureFormat_Undefined;
    WGPUTextureViewDimension dimension = WGPUTextureViewDimension_Undefined;
    WGPUTextureAspect aspect = WGPUTextureAspect_All;
    uint32_t baseMipLevel = 0, mipLevelCount = 0, baseArrayLayer = 0, arrayLayerCount = 0;

    bool operator==(const ViewKey& o) const {
        return format == o.format && dimension == o.dimension && aspect == o.aspect
            && baseMipLevel == o.baseMipLevel && mipLevelCount == o.mipLevelCount
            && baseArrayLayer == o.baseArrayLayer && arrayLayerCount == o.arrayLayerCount;
    }
    WGPUTextureViewDescriptor desc() const {
        return WGPUTextureViewDescriptor {
            .format = format, .dimension = dimension,
            .baseMipLevel = baseMipLevel, .mipLevelCount = mipLevelCount,
            .baseArrayLayer = baseArrayLayer, .arrayLayerCount = arrayLayerCount,
            .aspect = aspect,
        };
    }
};

// one cached non-full view of a physical texture
struct Subview {
    ViewKey key;
    WGPUTextureView view;
    Subview* next;
};


struct SubviewPool {
    Arena* arena = nullptr;
    Subview* freeList = nullptr;

    // pop a recycled node or bump a fresh one
    Subview* alloc();
    // release each view in the list, push its nodes onto freeList
    void recycle(Subview*& head);
};

// cached non-full view for this key on one texture's list
WGPUTextureView subview_for(SubviewPool& pool, Subview*& head, WGPUTexture tex, const ViewKey& k);

inline bool extent_eq(WGPUExtent3D a, WGPUExtent3D b) {
    return a.width == b.width && a.height == b.height && a.depthOrArrayLayers == b.depthOrArrayLayers;
}

// a texture's create-time signature
struct TexSignature {
    WGPUExtent3D size {};
    WGPUTextureFormat format = WGPUTextureFormat_Undefined;
    WGPUTextureDimension dim = WGPUTextureDimension_Undefined;
    uint32_t mipLevelCount = 1;
    uint32_t sampleCount = 1;

    bool operator==(const TexSignature& o) const {
        return extent_eq(size, o.size) && format == o.format && dim == o.dim
            && mipLevelCount == o.mipLevelCount && sampleCount == o.sampleCount;
    }
};

// owns GPU resources that outlive the per-frame graph teardown. one Entry per logical resource, keyed
// by name content, holding the physical objects the graph ping-pongs.
struct PersistentResourcePool {
    static constexpr uint32_t kLayers = 2; // current + previous. N>2 deliberately unsupported.
    static constexpr uint64_t kRetain = 4; // free an entry untouched for this many frames

    struct Entry {
        uint64_t idValue = 0;
        WGPUStringView name {}; // identity across frames
        uint64_t frame = 0; // rotation counter
        uint64_t lastTouched = 0; // evictClock at the last touch
        uint64_t prevTouched = 0; // evictClock at the touch before this frame's
        uint32_t layers = kLayers; // kLayers = history, 1 = single in-place
        ResourceKind kind = ResourceKind::Texture;
        uint64_t initHash = 0; // initialize(): hash of the content baked in
        bool baked = false; // initialize(): cleared on (re)create
        uint64_t historyHash = 0; // mismatch -> destroy+recreate

        WGPUTexture tex[kLayers] = {};
        WGPUTextureView view[kLayers] = {}; // whole-texture view per layer
        Subview* subviews[kLayers] = {};

        // texture
        bool created = false;
        uint64_t createdClock = UINT64_MAX; // catches a same-frame recreate
        TexSignature sig {};
        WGPUTextureUsage usage = {}; // union over layers
        WGPUTextureUsage usageAtCreate = {};

        // buffer
        WGPUBuffer buf[kLayers] = {};
        uint64_t bufferSize = 0;
        WGPUBufferUsage bufUsage = {}; // union over layers/frames
        WGPUBufferUsage bufUsageAtCreate = {};

        Entry* next = nullptr; // live-list link when in entries, free-list link when recycled

        // interned, NUL-terminated, cross-frame stable
        WGPUStringView name_view() const { return name; }
    };

    Entry* entries = nullptr;
    Entry* freeList = nullptr;
    Arena* arena = nullptr;
    SubviewPool* subviewPool = nullptr;
    uint64_t evictClock = 0;

    // pop a recycled cell or create a new one
    Entry* alloc_entry();

    // null if absent
    Entry* find(ResourceId id);

    // ensure the entry exists and advance its rotation, so rotation tracks use, not declaration.
    // idempotent within a frame: a history's two layer nodes share one entry, only the first rotates.
    Entry* touch(ResourceId id, uint32_t layers, uint64_t hash, ResourceKind kind);

    // physical slot backing a layer under the entry's current rotation
    uint32_t slot(const Entry& e, uint32_t layerIndex) const;

    // would realize_*_entry destroy+recreate this entry
    static bool tex_entry_would_recreate(const Entry& e, const TexSignature& sig, WGPUTextureUsage usage);
    static bool buf_entry_would_recreate(const Entry& e, uint64_t size, WGPUBufferUsage usage);

    // does this hash force touch() to destroy the entry
    static bool history_hash_forces_destroy(const Entry& e, uint64_t hash);

    void realize_texture_entry(Entry* e, WGPUDevice device, const TexSignature& sig);

    // usage is the union across layers, since each physical buffer cycles through every layer role
    void realize_buffer_entry(Entry* e, WGPUDevice device, uint64_t size);

    void destroy(Entry* e);

    // free entries untouched for kRetain frames, then advance the clock. once per realized frame.
    void end_frame();

    // destroys all managed entities
    void destroy_all();

    ~PersistentResourcePool();
};

// caches per-frame transient GPU objects across teardown so realize() stops churning the driver. one
// physical object per Entry, keyed by descriptor and tagged by kind. usage matches by superset, except
// the restriction bits in acquire(). objects unclaimed for kRetain frames are evicted.
struct TransientResourcePool {
    static constexpr uint64_t kRetain = 4;

    struct Entry {
        ResourceKind kind = ResourceKind::Texture; // picks the arm
        // texture arm
        TexSignature sig {};
        WGPUTextureUsage usage = {};
        WGPUTexture tex = {};
        WGPUTextureView view = {}; // whole-texture view, the resolve_view fast path
        Subview* subviews = nullptr;
        // buffer arm
        uint64_t bufferSize = 0;
        WGPUBufferUsage bufUsage = {};
        WGPUBuffer buf = {};
        // shared
        bool inUse = false; // released in release_claims
        uint64_t lastUsedFrame = 0; // eviction only
        uint64_t createdFrame = 0; // supersede-eviction key, with identity
        // interned name of the entry's LAST claimant, null if never claimed by name
        const void* identity = nullptr;
        Entry* next = nullptr; // live-list link when in entries, free-list link when recycled
    };
    // cells bump-allocated from arena, evicted cells recycle through freeList
    Entry* entries = nullptr;
    Entry* freeList = nullptr;
    Arena* arena = nullptr;
    SubviewPool* subviewPool = nullptr; // destroy() recycles into it
    uint64_t frame = 0;
    uint32_t createdThisFrame = 0;

    // pop a recycled cell or bump a fresh one, default-constructed
    Entry* alloc_entry();

    // debug event log
    enum class Event : uint8_t { Create, Evict };
    struct LogRec {
        uint64_t frame;
        Event event;
        ResourceKind kind;
        WGPUExtent3D size; // texture
        WGPUTextureFormat format; // texture
        uint64_t bufferSize; // buffer
    };
    static constexpr uint32_t kLog = 128;
    LogRec eventLog[kLog] = {};
    uint64_t eventCount = 0;

    void log_event(Event event, const Entry& e);

    void log_reset();

    // hand out a free texture matching the signature (usage by superset), or create one
    void acquire(WGPUDevice device, const TexSignature& sig, WGPUTextureUsage usage, const void* identity, WGPUTexture& outTex, WGPUTextureView& outView);

    void acquire(WGPUDevice device, uint64_t size, WGPUBufferUsage usage, const void* identity, WGPUBuffer& outBuf);

    // drop every inUse claim so the frame's objects become reusable
    void release_claims();

    // true iff idle entry e is made redundant this frame by same-kind sibling s, identical but for its
    // size. kills the resize VRAM spike. keyed on identity too, so only a resize of the SAME resource
    // supersedes.
    static bool superseded_by(const Entry& e, const Entry& s, uint64_t frame);

    // destroy objects idle >= kRetain frames, advance the clock. once per frame.
    void end_frame();

    void destroy(Entry* e);

    // idempotent. must run while arena is still alive.
    void destroy_all();

    ~TransientResourcePool();
};

// opt-in per-pass GPU timing. two timestamp queries per executed pass, resolved into a staging buffer
// and copied into a ring of mappable read-back buffers so a frame never stalls. created on the first
// profiled frame, and only when the device has TimestampQuery.
struct GpuProfiler {
    static constexpr uint32_t kMaxPasses = 64; // -> 2*64 timestamps in the set
    static constexpr uint32_t kRing = 3; // covers GPU read-back latency without stalling

    WGPUQuerySet querySet {}; // type Timestamp, count 2*kMaxPasses
    WGPUBuffer resolveBuf {}; // 2*kMaxPasses*8, QueryResolve | CopySrc

    struct Slot {
        WGPUBuffer buf {}; // 2*kMaxPasses*8, MapRead | CopyDst
        bool pending {}; // mapAsync in flight -> not reusable until the cb fires
        uint32_t count {}; // passes captured into this fill
        WGPUStringView names[kMaxPasses] {}; // interned pass names, captured at execute
    } ring[kRing];
    int pendingSlot { -1 }; // slot execute() filled this frame, awaiting the map kick

    // last completed read-back, read by the debug UI
    uint32_t resultCount {};
    WGPUStringView resultNames[kMaxPasses] {}; // interned, so they outlive the ring slot's reuse
    float resultUs[kMaxPasses] {};

    bool initialized {};

    // timing history, driven by the "Timings" tab
    static constexpr uint32_t kHistory = 256; // frames retained
    bool recording {};
    uint32_t resultId {}; // bumped by the read-back cb per sample, dedupes capture
    struct Series {
        WGPUStringView name {}; // interned, series matched by canonical .data pointer
        bool enabled { true };
        float v[kHistory] {};
    };
    Series series[kMaxPasses];
    uint32_t seriesCount {};
    uint32_t historyHead {}; // next write column
    uint32_t historyLen {}; // valid columns (<= kHistory)
    uint32_t lastSampledId {}; // resultId already appended

    void clear_history();

    // append one column iff recording and a fresh read-back arrived since last call. results match a
    // series by interned name and occurrence index, so repeated pass names keep distinct rows.
    // NOTE(Huerbe): series keyed by name. switching graphs adds new series and zero-fills old ones. clear resets.
    void sample_history();

    // one-time creation of the query set, resolve buffer, and read-back ring
    void init(WGPUDevice device);

    // first ring slot not awaiting a map, -1 if all are in flight
    int free_slot() const;
};

static constexpr size_t ARENA_DEFAULT_BLOCK_SIZE = 16 * 1024;
static constexpr size_t ARENA_SCRATCH_DEFAULT_BLOCK_SIZE = 2 * 1024;

// chained-block bump allocator behind all graph storage. frees nothing until reset() or free_all().
struct Arena {

    struct ArenaBlock {
        ArenaBlock* next;
        uint8_t* payload; // aligned payload start, inside this same malloc
        size_t capacity;
        size_t used; // bump cursor
    };

    ArenaBlock* head {};
    ArenaBlock* current {};
    size_t blockSize = ARENA_DEFAULT_BLOCK_SIZE;

    // debug stats
    size_t totalCapacity {};
    size_t peakUsage {};
    size_t blockCount {};
    size_t liveBytes {};

    // alignment must be a power of two
    static constexpr size_t align_up(size_t value, size_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    ArenaBlock* new_block(size_t minPayload);

    // ensure the first block exists ahead of the first alloc
    void reserve();

    size_t live_used() const;

    void* alloc_raw(size_t size, size_t align);

    // grow p in place when it is the tail of the current block, null when it cannot
    void* extend_tail(void* p, size_t oldSize, size_t addSize);

    // fatal, aborts via qFatal. alloc_raw and its callers rely on this and never null-check.
    [[noreturn]] void oom(size_t size);

    // rewind every block, keeping the memory
    void reset();

    void free_all();

    struct Mark {
        ArenaBlock* block;
        size_t used;
    };
    Mark mark() const;
    void rewind(Mark m);

    template <typename T, typename... Args>
    static T* construct(void* m, Args&&... args)
    {
        return ::new (m) T(std::forward<Args>(args)...);
    }

    template <typename T>
    static T* zero(void* m, size_t count)
    {
        std::memset(m, 0, sizeof(T) * count);
        return static_cast<T*>(m);
    }

    template <typename T, typename... Args>
    T* make(Args&&... args)
    {
        return construct<T>(alloc_raw(sizeof(T), alignof(T)), std::forward<Args>(args)...);
    }

    // zeroed POD storage, contiguous within one block
    template <typename T>
    T* alloc(size_t count = 1)
    {
        return zero<T>(alloc_raw(sizeof(T) * count, alignof(T)), count);
    }

    // arena-owned copy, always null-terminated
    WGPUStringView copy_string(WGPUStringView s);
};

struct StringInterner {
    static constexpr uint32_t kBuckets = 64; // power of two, index = hash & (kBuckets-1)

    struct Node {
        uint64_t hash; // matched before the content compare
        WGPUStringView name; // NUL-terminated
        Node* next;
    };

    Arena* arena = nullptr;
    Node* buckets[kBuckets] = {};

    ResourceId intern(std::string_view s);
};


} // namespace webgpu::rg::Internal

namespace webgpu::rg {

// persistent data storage for the render graph
struct GraphAllocator {
    Internal::Arena front; // permanent per-frame nodes
    Internal::Arena scratch; // compile()-local temporaries, driven by ScopedScratch
    Internal::Arena persist; // Entry cells for both pools. never per-frame reset, freed at destroy_allocator.

    Internal::StringInterner names; // dedup table for every resource/pass/pool/profiler name
    Internal::PersistentResourcePool pool; // name-keyed history textures + buffers
    Internal::TransientResourcePool transient; // descriptor-keyed per-frame texture + buffer cache
    Internal::SubviewPool subviews; // shared cache of non-full texture views for both pools
    Internal::GpuProfiler profiler; // opt-in per-pass GPU timestamps

    // whether the device advertises WGPUFeatureName_TransientAttachments
    bool transientFeatureOn {};

    // bumped by reset()
    uint64_t frameId { 1 };

    static constexpr size_t align_up(size_t value, size_t alignment) { return Internal::Arena::align_up(value, alignment); }

    // front-arena forwarders for per-frame allocations
    void* alloc_raw(size_t size, size_t align);
    template <typename T, typename... Args>
    T* make(Args&&... args)
    {
        return front.make<T>(std::forward<Args>(args)...);
    }
    template <typename T>
    T* alloc(size_t count = 1)
    {
        return front.alloc<T>(count);
    }
    // null-terminated front-arena copy
    WGPUStringView copy_string(WGPUStringView s);

    // per-frame teardown: rewind both arenas
    void reset();
};

} // namespace webgpu::rg

namespace webgpu::rg::Internal {

struct ScopedScratch {
    Arena* arena;
    Arena::Mark mark;
    explicit ScopedScratch(Arena& a);
    ~ScopedScratch();
    ScopedScratch(const ScopedScratch&) = delete;
    ScopedScratch& operator=(const ScopedScratch&) = delete;

    void reset();

    template <typename T>
    T* alloc(size_t count = 1)
    {
        return arena->alloc<T>(count);
    }
    template <typename T, typename... Args>
    T* make(Args&&... args)
    {
        return arena->make<T>(std::forward<Args>(args)...);
    }
};

struct ResourceNode {
    ResourceHandle handle {};
    ResourceId id {};
    ResourceKind kind {};
    bool imported : 1 {}; // managed outside the graph, e.g. a swapchain texture
    bool persistent : 1 {}; // one layer of a PersistentResourcePool-backed resource
    bool history : 1 {}; // .curr written this frame, read next
    bool historyValid : 1 {}; // false if the entry was recreated this frame
    bool resolving : 1 {}; // on the relativeTo recursion stack -> re-entry is a cycle
    bool hasWriter : 1 {}; // phase-4 aliasing eligibility: some pass writes this
    bool firstDefines : 1 {}; // phase-4 aliasing eligibility: first touch fully overwrites the occupant
    bool transientAttachment : 1 {}; // graph-owned attachment that never leaves the GPU

    uint32_t historyIndex {}; // 0 == current (write target), 1 == previous (read)
    uint64_t historyHash {};

    // storage not owned by the per-frame graph, imported or pool-backed
    bool is_external() const;

    // texture fields
    WGPUTextureDimension dimension = WGPUTextureDimension_Undefined;
    WGPUTextureFormat format = WGPUTextureFormat_Undefined;
    SizeKind sizeKind = SizeKind::Absolute;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    ResourceHandle relativeToHandle {};
    WGPUExtent3D absolute = WGPU_EXTENT_3D_INIT;
    uint32_t mipLevelCount = 1; // > 1 = mip chain, per-mip views built at bind/attach time
    uint32_t sampleCount = 1; // > 1 = MSAA

    // buffer fields
    uint64_t bufferSize {};

    // realized GPU handles
    WGPUTexture texture {}; // created: the texture object backing view
    WGPUTextureView view {}; // imported: the registered swapchain view
    // head slot of the subview list of the pooled physical texture backing this node
    Subview** subviews {};
    WGPUBuffer buffer {}; // imported: the registered buffer
    WGPUExtent3D resolved = WGPU_EXTENT_3D_INIT; // the base for Relative resolution
    WGPUTextureUsage texUsage {}; // accumulated in compile() from the access list
    WGPUBufferUsage bufUsage {};

    // first/last surviving pass to touch this, in execution order. filled in compile() phase 3
    static constexpr uint32_t kNoPass = ~0u;
    uint32_t firstUse = kNoPass; // kNoPass = dead transient or imported.
    uint32_t lastUse = kNoPass; // kNoPass = dead transient or imported.

    // the physical slot this transient shares with other disjoint-lifetime resources. kNoSlot means its own object, ineligible or aliasing off.
    static constexpr uint32_t kNoSlot = ~0u;
    uint32_t aliasSlot = kNoSlot;

    // filled during the phase-3 access walk. soleAccess is the last one recorded, the only one when the count is 1.
    uint32_t liveAccessCount {};
    const ResourceAccess* soleAccess {};

    ResourceNode* next {};
};

struct NodeAdjacency;

// intrusive list node with a growable access array
struct PassNode {
    ResourceId id {};
    PassKind kind {};

    // dense declaration-order index, assigned early in compile()
    uint32_t index {};

    // type-erased pass body, invoked by execute()
    void* exec_obj {};
    void (*exec_fn)(void*, PassContext&) {};

    ResourceAccess* accesses {};
    uint32_t accessCount {};
    uint32_t accessCap {};

    NodeAdjacency* adjacency {};

    bool placed {}; // topo sort: already emitted into execution order
    bool onStack {}; // topo sort: on the DFS stack -> a re-entry is a back-edge
    bool sink {}; // cull root: writes an imported or history resource, so it survives with no reader
    bool forceKeep {}; // force_keep(): extra cull root

    // initialize() targets this pass bakes. empty is an ordinary pass. compile() sets skipInit only when
    // every target is populated and baked with a matching hash, since a body bakes all or none.
    static constexpr uint32_t kMaxInitTargets = 8;
    struct InitTarget {
        ResourceHandle target;
        uint64_t hash;
    };

    InitTarget* initTargets {};
    uint32_t initCount {};
    bool skipInit {};

    PassNode* next {};
};

// pass must execute before the owner of this list
struct NodeAdjacency {
    PassNode* pass {};
    NodeAdjacency* next {};
};

// one physical GPU object shared by transients with disjoint lifetimes and an identical signature
// (aliasing, compile() phase 4). a slot never mixes the texture and buffer arms.
struct PhysicalResource {
    ResourceKind kind {};
    uint32_t freeFrom {}; // occupant lastUse, reusable iff the next member's firstUse > this
    // interned name of the first member to open this slot. stable across frames, which is what lets
    // superseded_by recognise a resize of it.
    const void* identity {};
    // texture
    TexSignature sig {}; // members must match exactly to share this slot
    WGPUTextureUsage texUsage {}; // union over members
    WGPUTexture texture {}; // filled by realize() via transient.acquire()
    WGPUTextureView view {};
    // buffer
    uint64_t bufferSize {};
    WGPUBufferUsage bufUsage {}; // union over members
    WGPUBuffer buffer {};
};

enum struct RenderGraphState {
    Recording = 0, // mutable
    Compiled, // read-only, ready to realize/execute
    Failed, // authoring error, realize/execute no-op
    Finished // spent for this frame
};

// concrete state behind the opaque RenderGraph handle, co-allocated in the same block
struct RenderGraphStorage {
    GraphAllocator* m_allocator {};
    ResourceNode* m_resouces {};
    ResourceNode* m_resoucesTail {};
    PassNode* m_passes {};
    PassNode* m_passesTail {};
    RenderGraphState m_state {};
    uint32_t next_resource_id = 1; // 0 = invalid handle, so this - 1 is the resource count
    uint32_t passCount {}; // incl. skipInit. sizes the dedup stamp.
    uint64_t generation {}; // graph-instance id
    uint64_t frameId {}; // checked against the allocator's
    ResourceNode** byId {};
    PhysicalResource* m_slots {};
    uint32_t m_slotCount {};
    // graph-owned attachments flagged memoryless
    uint32_t transientCount {};
    // execute() scratch: subresource views for the current pass, released after the body. one view per
    // access, so the per-pass access ceiling bounds it.
    WGPUTextureView* viewScratch {};
    uint32_t viewScratchN {};
    uint32_t viewScratchCap {}; // grows on demand, monotonic across passes
    ErrorMessage* m_errors {}; // non-null => m_state is Failed
    ErrorMessage* m_errorsTail {};

    // CPU timings
    float timing_compile_us {};
    float timing_realize_us {};
    float timing_execute_us {};
};

// storage() relies on st sitting immediately after rg
struct GraphPair {
    RenderGraph rg;
    RenderGraphStorage st;
};

// unit-test helper
struct PassContextAccess {
    PassContext ctx;
    PassContextAccess(RenderGraph* graph, PassNode* pass)
    {
        ctx.graph = graph;
        ctx.pass = pass;
    }
};

RenderGraphStorage* storage(RenderGraph* rg);

ResourceNode* find_node(RenderGraph* rg, ResourceHandle h);

// the write half of AccessType
bool access_is_write(AccessType t);

// empty past the end. for the debug lifetime widget.
WGPUStringView pass_name_at(PassNode* head, uint32_t idx);

// texel-block descriptor
struct TexelBlock { uint32_t w, h, bytes; };
TexelBlock format_block(WGPUTextureFormat f);

uint64_t texture_bytes(WGPUExtent3D size, WGPUTextureFormat format, uint32_t mipLevelCount = 1, uint32_t sampleCount = 1, WGPUTextureDimension dim = WGPUTextureDimension_2D);

} // namespace webgpu::rg::Internal
