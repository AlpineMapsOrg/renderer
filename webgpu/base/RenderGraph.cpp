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


#include "RenderGraph.h"
#include "RenderGraph_internal.h"
#include "gpu_utils.h"

#include <QtAssert>
#include <QDebug>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include <webgpu/webgpu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// on WebGPU the usage flag is the only sign of support, so probe for the constant
EM_JS(int, rg_web_has_transient_attachment, (), { return (typeof GPUTextureUsage !== 'undefined' && ('TRANSIENT_ATTACHMENT' in GPUTextureUsage)) ? 1 : 0; });
#endif


template <typename T>
static void list_append(T** head, T** tail, T* newNode)
{
    if (*tail)
        (*tail)->next = newNode;
    else
        *head = newNode;
    *tail = newNode;
}

namespace webgpu::rg {

using namespace Internal;

namespace Internal {

size_t sv_length(WGPUStringView s) { return (s.length == WGPU_STRLEN) ? (s.data ? std::strlen(s.data) : 0) : s.length; }

bool sv_eq(WGPUStringView a, WGPUStringView b)
{
    size_t na = sv_length(a), nb = sv_length(b);
    return na == nb && (na == 0 || std::memcmp(a.data, b.data, na) == 0);
}

WGPUStringView group_prefix(WGPUStringView name)
{
    size_t n = sv_length(name);
    for (size_t i = 0; i < n; ++i)
        if (name.data[i] == '.')
            return WGPUStringView { .data = name.data, .length = i };
    return WGPUStringView {};
}

Subview* SubviewPool::alloc()
{
    if (freeList) {
        Subview* s = freeList;
        freeList = s->next;
        return s;
    }
    return arena->make<Subview>();
}

void SubviewPool::recycle(Subview*& head)
{
    while (Subview* s = head) {
        head = s->next;
        if (s->view)
            wgpuTextureViewRelease(s->view);
        s->view = nullptr;
        s->next = freeList;
        freeList = s;
    }
}

WGPUTextureView subview_for(SubviewPool& pool, Subview*& head, WGPUTexture tex, const ViewKey& k)
{
    for (Subview* s = head; s; s = s->next)
        if (s->key == k)
            return s->view;
    Subview* s = pool.alloc();
    s->key = k;
    WGPUTextureViewDescriptor d = k.desc();
    s->view = wgpuTextureCreateView(tex, &d);
    s->next = head; // prepend
    head = s;
    return s->view;
}

PersistentResourcePool::Entry* PersistentResourcePool::alloc_entry()
{
    if (freeList) {
        Entry* e = freeList;
        freeList = e->next;
        *e = Entry {};
        return e;
    }
    return arena->make<Entry>();
}

PersistentResourcePool::Entry* PersistentResourcePool::find(ResourceId id)
{
    for (Entry* e = entries; e; e = e->next)
        if (e->idValue == id.value && e->name.data == id.name.data) // same data storage so the same data ptr
            return e;
    return nullptr;
}

PersistentResourcePool::Entry* PersistentResourcePool::touch(ResourceId id, uint32_t layers, uint64_t hash, ResourceKind kind)
{
    if (Entry* e = find(id)) {
        if (e->lastTouched == evictClock) // same resource already touched
            return e;
        if (e->kind != kind || e->layers != layers)
            destroy(e);
        e->prevTouched = e->lastTouched;
        ++e->frame;
        e->lastTouched = evictClock;
        e->layers = layers;
        e->kind = kind;
        if (history_hash_forces_destroy(*e, hash)) {
            destroy(e);
            e->historyHash = hash;
        }
        return e;
    }
    Entry* e = alloc_entry();
    e->name = id.name;
    e->idValue = id.value;
    e->lastTouched = evictClock;
    e->prevTouched = evictClock;
    e->layers = layers;
    e->kind = kind;
    e->historyHash = hash;
    e->next = entries;
    entries = e;
    return e;
}

uint32_t PersistentResourcePool::slot(const Entry& e, uint32_t layerIndex) const
{
    return (uint32_t)((e.frame + layerIndex) % e.layers);
}

bool PersistentResourcePool::tex_entry_would_recreate(const Entry& e, const TexSignature& sig, WGPUTextureUsage usage)
{
    return !e.created || !(e.sig == sig) || (usage & ~e.usageAtCreate) != 0;
}

bool PersistentResourcePool::buf_entry_would_recreate(const Entry& e, uint64_t size, WGPUBufferUsage usage)
{
    return !e.created || e.bufferSize != size || (usage & ~e.bufUsageAtCreate) != 0;
}

bool PersistentResourcePool::history_hash_forces_destroy(const Entry& e, uint64_t hash)
{
    return hash != 0 && e.historyHash != hash;
}

void PersistentResourcePool::realize_texture_entry(Entry* e, WGPUDevice device, const TexSignature& sig)
{
    if (!tex_entry_would_recreate(*e, sig, e->usage))
        return;

    destroy(e);
    e->sig = sig;
    e->usageAtCreate = e->usage;
    for (uint32_t i = 0; i < e->layers; ++i)
    {
        WGPUTextureDescriptor d {
            .label = e->name_view(),
            .usage = e->usage,
            .dimension = sig.dim,
            .size = sig.size,
            .format = sig.format,
            .mipLevelCount = sig.mipLevelCount,
            .sampleCount = sig.sampleCount,
        };
        e->tex[i] = wgpuDeviceCreateTexture(device, &d);
        e->view[i] = wgpuTextureCreateView(e->tex[i], nullptr);
    }
    e->created = true;
    e->createdClock = evictClock;
}

void PersistentResourcePool::realize_buffer_entry(Entry* e, WGPUDevice device, uint64_t size)
{
    if (!buf_entry_would_recreate(*e, size, e->bufUsage))
        return;
    destroy(e);
    e->bufferSize = size;
    e->bufUsageAtCreate = e->bufUsage;
    for (uint32_t i = 0; i < e->layers; ++i)
    {
        WGPUBufferDescriptor d {
            .label = e->name_view(),
            .usage = e->bufUsage,
            .size = size,
        };
        e->buf[i] = wgpuDeviceCreateBuffer(device, &d);
    }
    e->created = true;
    e->createdClock = evictClock;
}

void PersistentResourcePool::destroy(Entry* e)
{
    for (uint32_t i = 0; i < kLayers; ++i)
    {
        subviewPool->recycle(e->subviews[i]);
        if (e->view[i]) {
            wgpuTextureViewRelease(e->view[i]);
            e->view[i] = nullptr;
        }
        if (e->tex[i]) {
            wgpuTextureRelease(e->tex[i]);
            e->tex[i] = nullptr;
        }
        if (e->buf[i]) {
            wgpuBufferRelease(e->buf[i]);
            e->buf[i] = nullptr;
        }
    }
    e->created = false;
    e->baked = false;
}

void PersistentResourcePool::end_frame()
{
    for (Entry** pp = &entries; *pp;)
    {
        Entry* e = *pp;
        if (evictClock - e->lastTouched >= kRetain)
        {
            destroy(e);
            *pp = e->next;
            e->next = freeList; // recycle
            freeList = e;
        } else {
            pp = &e->next;
        }
    }
    ++evictClock;
}

void PersistentResourcePool::destroy_all()
{
    for (Entry* e = entries; e; e = e->next)
        destroy(e);
    entries = nullptr;
    freeList = nullptr;
}

PersistentResourcePool::~PersistentResourcePool()
{
    destroy_all();
}

void TransientResourcePool::log_event(Event event, const Entry& e)
{
    eventLog[eventCount++ % kLog] = {
        frame, event, e.kind, e.sig.size, e.sig.format, e.bufferSize
    };
}

void TransientResourcePool::log_reset() { eventCount = 0; }

static constexpr bool transient_usage_satisfies(WGPUFlags have, WGPUFlags want, WGPUFlags exact)
{
    return (have & want) == want && (have & exact) == (want & exact);
}

TransientResourcePool::Entry* TransientResourcePool::alloc_entry()
{
    // recycle
    if (freeList) {
        Entry* e = freeList;
        freeList = e->next;
        *e = Entry {};
        return e;
    }
    return arena->make<Entry>();
}

void TransientResourcePool::acquire(WGPUDevice device, const TexSignature& sig, WGPUTextureUsage usage, const void* identity, WGPUTexture& outTex, WGPUTextureView& outView)
{
    for (Entry* e = entries; e; e = e->next)
    {
        if (e->kind == ResourceKind::Texture && !e->inUse && e->sig == sig
            && transient_usage_satisfies(e->usage, usage, WGPUTextureUsage_TransientAttachment))
        {
            e->inUse = true;
            e->lastUsedFrame = frame;
            e->identity = identity;
            outTex = e->tex;
            outView = e->view;
            return;
        }
    }
    Entry* e = alloc_entry();
    e->kind = ResourceKind::Texture;
    e->sig = sig;
    e->usage = usage;
    e->identity = identity;
    WGPUTextureDescriptor d {
        .usage = usage,
        .dimension = sig.dim,
        .size = sig.size,
        .format = sig.format,
        .mipLevelCount = sig.mipLevelCount,
        .sampleCount = sig.sampleCount,
    };
    e->tex = wgpuDeviceCreateTexture(device, &d);
    e->view = wgpuTextureCreateView(e->tex, nullptr);
    e->inUse = true;
    e->lastUsedFrame = frame;
    e->createdFrame = frame;
    e->next = entries;
    entries = e;
    ++createdThisFrame;
    log_event(Event::Create, *e);
    outTex = e->tex;
    outView = e->view;
}

void TransientResourcePool::acquire(WGPUDevice device, uint64_t size, WGPUBufferUsage usage, const void* identity, WGPUBuffer& outBuf)
{
    for (Entry* e = entries; e; e = e->next)
    {
        if (e->kind == ResourceKind::Buffer && !e->inUse && e->bufferSize == size
            && transient_usage_satisfies(e->bufUsage, usage, /*exact*/ 0))
        {
            e->inUse = true;
            e->lastUsedFrame = frame;
            e->identity = identity; // last claimant, see the texture overload
            outBuf = e->buf;
            return;
        }
    }
    Entry* e = alloc_entry();
    e->kind = ResourceKind::Buffer;
    e->bufferSize = size;
    e->bufUsage = usage;
    e->identity = identity;
    WGPUBufferDescriptor d { .usage = usage, .size = size };
    e->buf = wgpuDeviceCreateBuffer(device, &d);
    e->inUse = true;
    e->lastUsedFrame = frame;
    e->createdFrame = frame;
    e->next = entries;
    entries = e;
    ++createdThisFrame;
    log_event(Event::Create, *e);
    outBuf = e->buf;
}

void TransientResourcePool::release_claims()
{
    for (Entry* e = entries; e; e = e->next)
        e->inUse = false;
}

bool TransientResourcePool::superseded_by(const Entry& e, const Entry& s, uint64_t frame)
{
    if (e.kind != s.kind) // supersede compares like-for-like
        return false;
    if (e.lastUsedFrame >= frame) // claimed this frame -> still live, not superseded
        return false;
    if (s.createdFrame != frame) // s must be this frame's replacement
        return false;
    // same logical resource, or it is not a resize. the descriptor alone cannot tell a resize from an
    // unrelated resource of another size, and evicting on the latter churns without bound.
    if (!e.identity || e.identity != s.identity)
        return false;

    if (e.kind == ResourceKind::Texture) {
        // s must cover e under the same rule acquire reuses by. superset, so a resize may add usage bits,
        // but a differently-purposed idle sibling still cannot supersede.
        if (!transient_usage_satisfies(s.usage, e.usage, WGPUTextureUsage_TransientAttachment))
            return false;
        // same signature EXCEPT the extent
        if (s.sig.format != e.sig.format || s.sig.dim != e.sig.dim
            || s.sig.mipLevelCount != e.sig.mipLevelCount || s.sig.sampleCount != e.sig.sampleCount)
            return false;
        return !extent_eq(s.sig.size, e.sig.size); // identical but for the extent
    }

    if (!transient_usage_satisfies(s.bufUsage, e.bufUsage, /*exact*/ 0))
        return false;
    return s.bufferSize != e.bufferSize;
}

void TransientResourcePool::end_frame()
{
    for (Entry** pp = &entries; *pp;) {
        Entry* e = *pp;
        bool superseded = false;

        if (createdThisFrame && !e->inUse) {
            for (Entry* s = entries; s; s = s->next)
                if (superseded_by(*e, *s, frame)) {
                    superseded = true;
                    break;
                }
        }
        if (superseded || frame - e->lastUsedFrame >= kRetain) {
            log_event(Event::Evict, *e);
            destroy(e);
            *pp = e->next;
            e->next = freeList; // recycle
            freeList = e;
        } else {
            pp = &e->next;
        }
    }
    createdThisFrame = 0;
    ++frame;
}

void TransientResourcePool::destroy(Entry* e)
{
    subviewPool->recycle(e->subviews);
    if (e->view) {
        wgpuTextureViewRelease(e->view);
        e->view = nullptr;
    }
    if (e->tex) {
        wgpuTextureRelease(e->tex);
        e->tex = nullptr;
    }
    if (e->buf) {
        wgpuBufferRelease(e->buf);
        e->buf = nullptr;
    }
}

void TransientResourcePool::destroy_all()
{
    for (Entry* e = entries; e; e = e->next)
        destroy(e);
    entries = nullptr;
    freeList = nullptr;
}

TransientResourcePool::~TransientResourcePool()
{
    destroy_all();
}

void GpuProfiler::clear_history()
{
    seriesCount = 0;
    historyHead = 0;
    historyLen = 0;
    lastSampledId = 0;
}

void GpuProfiler::sample_history()
{
    if (!recording || resultId == lastSampledId)
        return;
    lastSampledId = resultId;
    for (uint32_t s = 0; s < seriesCount; ++s)
        series[s].v[historyHead] = 0.0f;
    for (uint32_t i = 0; i < resultCount; ++i)
    {
        uint32_t occ = 0;
        for (uint32_t j = 0; j < i; ++j)
            if (resultNames[j].data == resultNames[i].data)
                ++occ;
        uint32_t s = 0;
        for (uint32_t seen = 0; s < seriesCount; ++s)
            if (series[s].name.data == resultNames[i].data && seen++ == occ)
                break;
        if (s == seriesCount) {
            if (seriesCount >= kMaxPasses)
                continue;
            series[s] = {};
            series[s].name = resultNames[i];
            ++seriesCount;
        }
        series[s].v[historyHead] = resultUs[i];
    }
    historyHead = (historyHead + 1) % kHistory;
    if (historyLen < kHistory)
        ++historyLen;
}

void GpuProfiler::init(WGPUDevice device)
{
    if (initialized)
        return;
    const uint64_t bytes = uint64_t(2) * kMaxPasses * sizeof(uint64_t);
    WGPUQuerySetDescriptor qd { .type = WGPUQueryType_Timestamp, .count = 2 * kMaxPasses };
    querySet = wgpuDeviceCreateQuerySet(device, &qd);
    WGPUBufferDescriptor rd { .usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc, .size = bytes };
    resolveBuf = wgpuDeviceCreateBuffer(device, &rd);
    for (Slot& s : ring) {
        WGPUBufferDescriptor bd { .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst, .size = bytes };
        s.buf = wgpuDeviceCreateBuffer(device, &bd);
    }
    initialized = true;
}

int GpuProfiler::free_slot() const
{
    for (uint32_t i = 0; i < kRing; ++i)
        if (!ring[i].pending)
            return (int)i;
    return -1;
}

Arena::ArenaBlock* Arena::new_block(size_t minPayload)
{
    const size_t maxAlign = alignof(std::max_align_t);
    const size_t payloadBytes = minPayload < blockSize ? blockSize : minPayload;
    const size_t raw = sizeof(ArenaBlock) + maxAlign + payloadBytes; // +maxAlign covers the round-up
    ArenaBlock* b = static_cast<ArenaBlock*>(std::malloc(raw));
    if (!b)
        return nullptr;
    uint8_t* start = reinterpret_cast<uint8_t*>(b) + sizeof(ArenaBlock);
    b->payload = reinterpret_cast<uint8_t*>(align_up(reinterpret_cast<size_t>(start), maxAlign));
    b->capacity = payloadBytes;
    b->used = 0;
    b->next = nullptr;
    totalCapacity += payloadBytes;
    ++blockCount;
    return b;
}

void Arena::reserve()
{
    if (!current)
        head = current = new_block(blockSize);
}

size_t Arena::live_used() const { return liveBytes; }

void* Arena::alloc_raw(size_t size, size_t align)
{
    if (!current)
    {
        head = current = new_block(size + align);
        if (!current)
            oom(size);
    }

    size_t off = align_up(current->used, align);
    if (off + size > current->capacity)
    {
        if (current->next && size + align <= current->next->capacity) {
            current = current->next;
            liveBytes -= current->used;
            current->used = 0;
        } else {
            ArenaBlock* b = new_block(size + align);
            if (!b)
                oom(size);
            b->next = current->next;
            current->next = b;
            current = b;
        }
        off = align_up(current->used, align);
    }

    void* p = current->payload + off;
    liveBytes += (off + size) - current->used;
    current->used = off + size;
    if (liveBytes > peakUsage)
        peakUsage = liveBytes;
    return p;
}

void* Arena::extend_tail(void* p, size_t oldSize, size_t addSize)
{
    if (!current || !p)
        return nullptr;
    if (reinterpret_cast<uint8_t*>(p) + oldSize != current->payload + current->used)
        return nullptr;
    if (current->used + addSize > current->capacity)
        return nullptr;
    current->used += addSize;
    liveBytes += addSize;
    if (liveBytes > peakUsage)
        peakUsage = liveBytes;
    return p;
}

[[noreturn]] void Arena::oom(size_t size)
{
    qFatal("[RenderGraph] error: arena alloc failed; malloc of %zu bytes failed, heap exhausted.\n", size);
}

void Arena::reset()
{
    for (ArenaBlock* b = head; b; b = b->next) {
#ifndef NDEBUG
        // poison rewound bytes so a stale pointer reads garbage
        std::memset(b->payload, 0xDD, b->used);
#endif
        b->used = 0;
    }
    current = head;
    liveBytes = 0;
    peakUsage = 0;
}

void Arena::free_all()
{
    for (ArenaBlock* b = head; b;) {
        ArenaBlock* n = b->next;
        std::free(b);
        b = n;
    }
    head = current = nullptr;
    totalCapacity = peakUsage = blockCount = liveBytes = 0;
}

Arena::Mark Arena::mark() const
{
    return Mark { current, current ? current->used : 0 };
}

void Arena::rewind(Mark m)
{
    for (ArenaBlock* b = (m.block ? m.block->next : head); b; b = b->next)
    {
        liveBytes -= b->used;
        b->used = 0;
    }

    if (m.block)
    {
        liveBytes -= m.block->used - m.used;
        m.block->used = m.used;
        current = m.block;
    }
    else
    {
        current = head;
    }
}

WGPUStringView Arena::copy_string(WGPUStringView s)
{
    const size_t len = (s.length == WGPU_STRLEN) ? (s.data ? std::strlen(s.data) : 0) : s.length;
    char* buf = alloc<char>(len + 1);
    if (len)
        std::memcpy(buf, s.data, len);
    buf[len] = '\0';
    return WGPUStringView { buf, len };
}

ResourceId StringInterner::intern(std::string_view s)
{
    const uint64_t h = fnv1a(s);
    Node*& head = buckets[h & (kBuckets - 1)];
    for (Node* n = head; n; n = n->next)
        if (n->hash == h && n->name.length == s.length()
            && (s.empty() || std::memcmp(n->name.data, s.data(), s.length()) == 0)) // empty view may carry a null data
            return { n->hash, n->name };

    Node* n = arena->make<Node>();
    n->hash = h;
    n->name = arena->copy_string(WGPUStringView { s.data(), s.length() });
    n->next = head;
    head = n;
    return { n->hash, n->name };
}

ScopedScratch::ScopedScratch(Arena& a)
    : arena(&a)
    , mark(a.mark())
{}

ScopedScratch::~ScopedScratch()
{
    arena->rewind(mark);
}

void ScopedScratch::reset()
{
    arena->rewind(mark);
}

bool ResourceNode::is_external() const
{
    return imported || persistent;
}

RenderGraphStorage* storage(RenderGraph* rg)
{
    return reinterpret_cast<RenderGraphStorage*>(reinterpret_cast<uint8_t*>(rg) + GraphAllocator::align_up(sizeof(RenderGraph), alignof(RenderGraphStorage)));
}

ResourceNode* find_node(RenderGraph* rg, ResourceHandle h)
{
    RenderGraphStorage& s = *storage(rg);

    Q_ASSERT(!(h.id && h.generation && h.generation != s.generation) && "stale/foreign ResourceHandle passed to a ctx resolver");

    if (s.byId && h.id && h.id < s.next_resource_id)
        return s.byId[h.id];
    for (ResourceNode* r = s.m_resouces; r; r = r->next)
        if (r->handle.id == h.id)
            return r;
    return nullptr;
}


enum struct DefineMode : uint8_t { None, IfClearLoadOp, Always, CopyDst };

struct AccessSemantics {
    bool isWrite;            // read-vs-write for hazard edges
    DefineMode define;       // type-level half of access_defines
    WGPUTextureUsage texUsage; // WGPUTextureUsage_None for buffer-only accesses
    WGPUBufferUsage bufUsage;  // WGPUBufferUsage_None for texture-only accesses
};

constexpr AccessSemantics access_semantics(AccessType t)
{
    switch (t) {
    case AccessType::ColorAttachment:
        return { true,  DefineMode::IfClearLoadOp, WGPUTextureUsage_RenderAttachment, WGPUBufferUsage_None };
    case AccessType::DepthStencilAttachment:
        return { true,  DefineMode::IfClearLoadOp, WGPUTextureUsage_RenderAttachment, WGPUBufferUsage_None };
    case AccessType::DepthStencilReadOnly:
        return { false, DefineMode::None,          WGPUTextureUsage_RenderAttachment, WGPUBufferUsage_None };
    case AccessType::ResolveAttachment:
        return { true,  DefineMode::None,          WGPUTextureUsage_RenderAttachment, WGPUBufferUsage_None };
    case AccessType::Sampled:
        return { false, DefineMode::None,          WGPUTextureUsage_TextureBinding,   WGPUBufferUsage_None };
    case AccessType::StorageRead:
        return { false, DefineMode::None,          WGPUTextureUsage_StorageBinding,   WGPUBufferUsage_Storage };
    case AccessType::StorageWrite:
        return { true,  DefineMode::Always,        WGPUTextureUsage_StorageBinding,   WGPUBufferUsage_Storage };
    case AccessType::Uniform:
        return { false, DefineMode::None,          WGPUTextureUsage_None,             WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst };
    case AccessType::CopySrc:
        return { false, DefineMode::None,          WGPUTextureUsage_CopySrc,          WGPUBufferUsage_CopySrc };
    case AccessType::CopyDst:
        return { true,  DefineMode::CopyDst,       WGPUTextureUsage_CopyDst,          WGPUBufferUsage_CopyDst };
    case AccessType::Vertex:
        return { false, DefineMode::None,          WGPUTextureUsage_None,             WGPUBufferUsage_Vertex };
    case AccessType::Index:
        return { false, DefineMode::None,          WGPUTextureUsage_None,             WGPUBufferUsage_Index };
    case AccessType::Indirect:
        return { false, DefineMode::None,          WGPUTextureUsage_None,             WGPUBufferUsage_Indirect };
    }

    Q_UNREACHABLE();
    return {};
}

bool access_is_write(AccessType t)
{
    return access_semantics(t).isWrite;
}

WGPUStringView pass_name_at(PassNode* head, uint32_t idx)
{
    for (PassNode* p = head; p; p = p->next)
    {
        if (idx == 0)
            return p->id.name;
        --idx;
    }
    return WGPUStringView {};
}

// block size in texels (1x1 for uncompressed) plus bytes per block
TexelBlock format_block(WGPUTextureFormat f)
{
    switch (f)
    {
    // 8-bit
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_Stencil8:
        return { 1, 1, 1 };

    // 16-bit
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:

    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:

    case WGPUTextureFormat_Depth16Unorm:
        return { 1, 1, 2 };

    // 32-bit
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:

    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:

    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:

    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:

    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:

    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth32Float:
        return { 1, 1, 4 };

    // 64-bit
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:

    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:

    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return { 1, 1, 8 };

    // 128-bit
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return { 1, 1, 16 };


    // BC1 / BC4: 8 bytes per 4x4 block
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
        return { 4, 4, 8 };

    // BC2/3/5/6H/7: 16 bytes per 4x4 block
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return { 4, 4, 16 };


    // ETC2 RGB8 / RGB8A1 and EAC R11: 8 bytes per 4x4 block
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
        return { 4, 4, 8 };

    // ETC2 RGBA8 and EAC RG11: 16 bytes per 4x4 block
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
        return { 4, 4, 16 };


    // ASTC: always 16 bytes per block, block footprint read straight off the enum name
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:   return { 4, 4, 16 };
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:   return { 5, 4, 16 };
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:   return { 5, 5, 16 };
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:   return { 6, 5, 16 };
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:   return { 6, 6, 16 };
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:   return { 8, 5, 16 };
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:   return { 8, 6, 16 };
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:   return { 8, 8, 16 };
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:  return { 10, 5, 16 };
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:  return { 10, 6, 16 };
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:  return { 10, 8, 16 };
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb: return { 10, 10, 16 };
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb: return { 12, 10, 16 };
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb: return { 12, 12, 16 };

    default:
        break;
    }

    return { 0, 0, 0 };
}

// exact size of one texture
uint64_t texture_bytes(WGPUExtent3D size, WGPUTextureFormat format, uint32_t mipLevelCount,
    uint32_t sampleCount, WGPUTextureDimension dim)
{
    const TexelBlock b = format_block(format);
    if (!b.bytes) return 0;
    const bool is3D = dim == WGPUTextureDimension_3D;
    const uint32_t layers = is3D ? 1u : (size.depthOrArrayLayers ? size.depthOrArrayLayers : 1);
    const uint32_t samples = sampleCount ? sampleCount : 1;
    uint64_t total = 0;
    for (uint32_t m = 0; m < mipLevelCount; ++m) {
        const uint32_t w = (size.width  >> m) ? (size.width  >> m) : 1u;
        const uint32_t h = (size.height >> m) ? (size.height >> m) : 1u;
        const uint32_t d = is3D ? ((size.depthOrArrayLayers >> m) ? (size.depthOrArrayLayers >> m) : 1u) : 1u;
        const uint32_t bw = (w + b.w - 1) / b.w;
        const uint32_t bh = (h + b.h - 1) / b.h;
        total += (uint64_t)bw * bh * d * layers * samples * b.bytes;
    }
    return total;
}

} // namespace Internal

void* GraphAllocator::alloc_raw(size_t size, size_t align)
{
    return front.alloc_raw(size, align);
}

WGPUStringView GraphAllocator::copy_string(WGPUStringView s)
{
    return front.copy_string(s);
}

void GraphAllocator::reset()
{
    front.reset();
    scratch.reset();
    ++frameId;
}

// printf arg pair for a "%.*s" name, null data prints empty
#define RG_NAME(x) (int)(x).name.length, (x).name.data ? (x).name.data : ""

// records the error and poisons the graph to the failed state
static void push_error(RenderGraphStorage& s, const char* fmt, ...)
{
    ScopedScratch scratch(s.m_allocator->scratch);

    va_list ap;
    va_start(ap, fmt);
    va_list probe;
    va_copy(probe, ap);
    const int len = std::vsnprintf(nullptr, 0, fmt, probe);
    va_end(probe);

    char* buf = scratch.alloc<char>(size_t(len) + 1);
    std::vsnprintf(buf, size_t(len) + 1, fmt, ap);
    va_end(ap);

    ErrorMessage* e = s.m_allocator->make<ErrorMessage>();
    e->message = s.m_allocator->copy_string(WGPUStringView { buf, size_t(len) });
    list_append(&s.m_errors, &s.m_errorsTail, e);
    s.m_state = RenderGraphState::Failed;
}


static ResourceHandle create_handle(RenderGraphStorage& s, ResourceKind kind)
{
    return {
        .id = s.next_resource_id++,
        .kind = kind,
        .generation = s.generation,
    };
}

static ResourceId intern_id(GraphAllocator& a, std::string_view s)
{
    return a.names.intern(s);
}

static void assert_unique_id(RenderGraphStorage& s, ResourceId id)
{
#ifndef NDEBUG
    for (ResourceNode* r = s.m_resouces; r; r = r->next)
        Q_ASSERT(!(r->id == id) && "ResourceIds must to be unique!");
#else
    (void)s;
    (void)id;
#endif
}

GraphAllocator* create_allocator()
{
    GraphAllocator* allocator = new GraphAllocator;
    allocator->front.blockSize = ARENA_DEFAULT_BLOCK_SIZE;
    allocator->scratch.blockSize = ARENA_SCRATCH_DEFAULT_BLOCK_SIZE;
    allocator->persist.blockSize = ARENA_SCRATCH_DEFAULT_BLOCK_SIZE;
    allocator->front.reserve();
    allocator->scratch.reserve();
    allocator->persist.reserve();
    allocator->names.arena = &allocator->persist;
    allocator->pool.arena = &allocator->persist;
    allocator->transient.arena = &allocator->persist;
    allocator->subviews.arena = &allocator->persist;
    allocator->pool.subviewPool = &allocator->subviews;
    allocator->transient.subviewPool = &allocator->subviews;
    return allocator;
}

void destroy_allocator(GraphAllocator* allocator)
{
    allocator->pool.destroy_all();
    allocator->transient.destroy_all();
    allocator->front.free_all();
    allocator->scratch.free_all();
    allocator->persist.free_all();
    delete allocator;
}

void begin_frame(GraphAllocator* allocator)
{
    allocator->reset();
}

void end_frame(GraphAllocator* allocator)
{
    allocator->pool.end_frame();
    allocator->transient.end_frame();
}

RenderGraph* start_recording(GraphAllocator* allocator)
{
    Q_ASSERT(allocator);
    Internal::GraphPair* pair = allocator->make<Internal::GraphPair>();
    RenderGraph* rg = &pair->rg;
    RenderGraphStorage* st = &pair->st;
    Q_ASSERT(st == storage(rg) && "storage must sit immediately after the RenderGraph");
    st->m_allocator = allocator;
    st->frameId = allocator->frameId;

    static uint64_t g_next_graph_generation = 1;
    st->generation = g_next_graph_generation++;
    return rg;
}


static bool access_defines(const ResourceAccess& a)
{
    switch (access_semantics(a.type).define) {
    case DefineMode::None:          return false;
    case DefineMode::IfClearLoadOp: return a.loadOp == WGPULoadOp_Clear;
    case DefineMode::Always:        return true;
    case DefineMode::CopyDst:       return a.handle.kind == ResourceKind::Buffer && a.bufFullDefine;
    }
    return false;
}

static WGPUTextureUsage tex_usage_of(AccessType t) { return access_semantics(t).texUsage; }
static WGPUBufferUsage buf_usage_of(AccessType t) { return access_semantics(t).bufUsage; }

// half-open ranges [base, base+count). count 0 means all remaining.
static constexpr bool ranges_overlap(uint32_t aBase, uint32_t aCount, uint32_t bBase, uint32_t bCount)
{
    uint32_t aEnd = aCount ? aBase + aCount : UINT32_MAX;
    uint32_t bEnd = bCount ? bBase + bCount : UINT32_MAX;
    return aBase < bEnd && bBase < aEnd;
}

// All covers depth+stencil, so it overlaps either aspect. otherwise two accesses clash only on the same one.
static constexpr bool aspects_overlap(WGPUTextureAspect a, WGPUTextureAspect b)
{
    return a == WGPUTextureAspect_All || b == WGPUTextureAspect_All || a == b;
}

static constexpr bool in_pass_accesses_conflict(AccessType a,
    uint32_t aBaseMip,
    uint32_t aMipCount,
    uint32_t aBaseLayer,
    uint32_t aLayerCount,
    WGPUTextureAspect aAspect,
    AccessType b,
    uint32_t bBaseMip,
    uint32_t bMipCount,
    uint32_t bBaseLayer,
    uint32_t bLayerCount,
    WGPUTextureAspect bAspect)
{
    if (!ranges_overlap(aBaseMip, aMipCount, bBaseMip, bMipCount))
        return false; // disjoint mips
    if (!ranges_overlap(aBaseLayer, aLayerCount, bBaseLayer, bLayerCount))
        return false; // disjoint layers
    if (!aspects_overlap(aAspect, bAspect))
        return false; // disjoint aspects
    if (!access_is_write(a) && !access_is_write(b))
        return false;
    if ((a == AccessType::StorageRead && b == AccessType::StorageWrite) || (a == AccessType::StorageWrite && b == AccessType::StorageRead))
        return false;
    return true;
}

static void validate_texture_desc(const TextureDesc& desc)
{
    Q_ASSERT_X(desc.dimension != WGPUTextureDimension_1D, "validate_texture_desc",
        "The render graph does not support 1d textures, use 2d textures instead");
    Q_ASSERT_X(desc.relativeTo.id == 0 || (desc.scaleX > 0.0f && desc.scaleY > 0.0f), "validate_texture_desc",
        "When relative to another texture it cannot be a scale of 0");
    Q_ASSERT_X(desc.sizeKind != SizeKind::Relative || desc.relativeTo.id != 0, "validate_texture_desc",
        "SizeKind::Relative needs a relativeTo handle to size against, otherwise it resolves to nothing");
    Q_ASSERT_X(desc.sizeKind != SizeKind::Absolute || (desc.absolute.width && desc.absolute.height), "validate_texture_desc",
        "SizeKind::Absolute needs a non-zero width and height in TextureDesc.absolute");
    Q_ASSERT_X(desc.sampleCount == 1 || desc.sampleCount == 4, "validate_texture_desc",
        "WebGPU(1.0) only supports MSAA samples of 1 or 4.");
    Q_ASSERT_X(desc.dimension != WGPUTextureDimension_3D || desc.sampleCount == 1, "validate_texture_desc",
        "Texture3D does not support MSAA");
    Q_ASSERT_X(desc.dimension != WGPUTextureDimension_3D || desc.mipLevelCount == 1, "validate_texture_desc",
        "Texture3D only supports mipLevelCount of 1");
}

static void apply_texture_desc(ResourceNode* r, const TextureDesc& desc)
{
    validate_texture_desc(desc);
    r->dimension        = desc.dimension;
    r->format           = desc.format;
    r->sizeKind         = desc.sizeKind;
    r->scaleX           = desc.scaleX;
    r->scaleY           = desc.scaleY;
    r->relativeToHandle = desc.relativeTo;
    r->absolute         = desc.absolute;
    r->mipLevelCount    = desc.mipLevelCount ? desc.mipLevelCount : 1;
    r->sampleCount      = desc.sampleCount ? desc.sampleCount : 1;
}

static TexSignature tex_signature(const ResourceNode* r)
{
    return TexSignature { r->resolved, r->format, r->dimension, r->mipLevelCount, r->sampleCount };
}

TextureHandle RenderGraph::create_transient_texture(std::string_view id, const TextureDesc& desc)
{
    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");

    const ResourceId rid = intern_id(*s.m_allocator, id);
    assert_unique_id(s, rid);
    Q_ASSERT((desc.relativeTo.id == 0 || desc.relativeTo.generation == s.generation) && "TextureDesc.relativeTo is a stale/foreign handle");

    ResourceNode* resouce = s.m_allocator->make<ResourceNode>();

    resouce->handle = create_handle(s, ResourceKind::Texture);
    resouce->id = rid;
    resouce->kind = ResourceKind::Texture;
    apply_texture_desc(resouce, desc);

    list_append(&s.m_resouces, &s.m_resoucesTail, resouce);

    return TextureHandle { resouce->handle };
}

BufferHandle RenderGraph::create_transient_buffer(std::string_view id, const BufferDesc& desc)
{
    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");

    const ResourceId rid = intern_id(*s.m_allocator, id);
    assert_unique_id(s, rid);

    ResourceNode* resouce = s.m_allocator->make<ResourceNode>();

    resouce->handle = create_handle(s, ResourceKind::Buffer);
    resouce->id = rid;
    resouce->kind = ResourceKind::Buffer;
    resouce->bufferSize = desc.size;

    list_append(&s.m_resouces, &s.m_resoucesTail, resouce);

    return BufferHandle { resouce->handle };
}

TextureHandle RenderGraph::import_texture(std::string_view id, const ImportTextureDesc& desc)
{
    const WGPUTextureView view = desc.view;
    const WGPUExtent3D size = desc.size;
    const WGPUTextureFormat format = desc.format;
    WGPUTexture texture = desc.texture;
    const uint32_t mipCount = desc.mipCount;
    const uint32_t sampleCount = desc.sampleCount;
    const WGPUTextureDimension dimension = desc.dimension;

    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");

    const ResourceId rid = intern_id(*s.m_allocator, id);
    assert_unique_id(s, rid);

    ResourceNode* resouce = s.m_allocator->make<ResourceNode>();

    resouce->handle = create_handle(s, ResourceKind::Texture);
    resouce->id = rid;
    resouce->kind = ResourceKind::Texture;
    Q_ASSERT(view && "import_texture: ImportTextureDesc.view is required");
    Q_ASSERT(format != WGPUTextureFormat_Undefined && "import_texture: pass the imported view's format");
    resouce->imported = true;
    resouce->view = view;
    resouce->texture = texture;
    if (texture) {
        Q_ASSERT(size.width == wgpuTextureGetWidth(texture) && size.height == wgpuTextureGetHeight(texture)
            && size.depthOrArrayLayers == wgpuTextureGetDepthOrArrayLayers(texture)
            && "import_texture: size disagrees with the backing texture (stale extent after resize?)");
    }
    resouce->resolved = size;
    resouce->format = format;
    resouce->mipLevelCount = mipCount ? mipCount : 1;
    resouce->sampleCount = sampleCount ? sampleCount : 1;
    Q_ASSERT_X(dimension != WGPUTextureDimension_1D, "import_texture",
        "The render graph does not support 1d textures, use 2d textures instead");
    resouce->dimension = dimension;

    list_append(&s.m_resouces, &s.m_resoucesTail, resouce);

    return TextureHandle { resouce->handle };
}

BufferHandle RenderGraph::import_buffer(std::string_view id, WGPUBuffer buffer)
{
    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");
    Q_ASSERT(buffer && "import_buffer: null buffer; wgpuBufferGetSize below would dereference it");

    const ResourceId rid = intern_id(*s.m_allocator, id);
    assert_unique_id(s, rid);

    ResourceNode* resouce = s.m_allocator->make<ResourceNode>();

    resouce->handle = create_handle(s, ResourceKind::Buffer);
    resouce->id = rid;
    resouce->kind = ResourceKind::Buffer;
    resouce->imported = true;
    resouce->buffer = buffer;
    resouce->bufferSize = wgpuBufferGetSize(buffer);

    list_append(&s.m_resouces, &s.m_resoucesTail, resouce);

    return BufferHandle { resouce->handle };
}

HistoryTexture RenderGraph::create_history_texture(std::string_view id, const TextureDesc& desc, uint64_t hash)
{
    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");

    const ResourceId rid = intern_id(*s.m_allocator, id);
    assert_unique_id(s, rid);
    Q_ASSERT((desc.relativeTo.id == 0 || desc.relativeTo.generation == s.generation) && "TextureDesc.relativeTo is a stale/foreign handle");

    HistoryTexture out {};
    for (uint32_t i = 0; i < PersistentResourcePool::kLayers; ++i)
    {
        ResourceNode* resouce = s.m_allocator->make<ResourceNode>();
        resouce->handle = create_handle(s, ResourceKind::Texture);
        resouce->id = rid;
        resouce->kind = ResourceKind::Texture;
        resouce->persistent = true;
        resouce->history = true;
        resouce->historyIndex = i;
        resouce->historyHash = hash;
        apply_texture_desc(resouce, desc);
        list_append(&s.m_resouces, &s.m_resoucesTail, resouce);
        if (i == 0)
            out.curr = TextureHandle { resouce->handle };
        else
            out.prev = TextureHandle { resouce->handle };
    }
    return out;
}

HistoryBuffer RenderGraph::create_history_buffer(std::string_view id, const BufferDesc& desc, uint64_t hash)
{
    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");

    const ResourceId rid = intern_id(*s.m_allocator, id);
    assert_unique_id(s, rid);

    HistoryBuffer out {};
    for (uint32_t i = 0; i < PersistentResourcePool::kLayers; ++i)
    {
        ResourceNode* resouce = s.m_allocator->make<ResourceNode>();
        resouce->handle = create_handle(s, ResourceKind::Buffer);
        resouce->id = rid;
        resouce->kind = ResourceKind::Buffer;
        resouce->persistent = true;
        resouce->history = true;
        resouce->historyIndex = i;
        resouce->historyHash = hash;
        resouce->bufferSize = desc.size;
        list_append(&s.m_resouces, &s.m_resoucesTail, resouce);
        if (i == 0)
            out.curr = BufferHandle { resouce->handle };
        else
            out.prev = BufferHandle { resouce->handle };
    }
    return out;
}


BufferHandle RenderGraph::create_persistent_buffer(std::string_view id, const BufferDesc& desc)
{
    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");

    const ResourceId rid = intern_id(*s.m_allocator, id);
    assert_unique_id(s, rid);

    ResourceNode* resouce = s.m_allocator->make<ResourceNode>();
    resouce->handle = create_handle(s, ResourceKind::Buffer);
    resouce->id = rid;
    resouce->kind = ResourceKind::Buffer;
    resouce->persistent = true;
    resouce->historyIndex = 0; // the only layer
    resouce->bufferSize = desc.size;
    list_append(&s.m_resouces, &s.m_resoucesTail, resouce);
    return BufferHandle { resouce->handle };
}


TextureHandle RenderGraph::create_persistent_texture(std::string_view id, const TextureDesc& desc)
{
    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");

    const ResourceId rid = intern_id(*s.m_allocator, id);
    assert_unique_id(s, rid);
    Q_ASSERT((desc.relativeTo.id == 0 || desc.relativeTo.generation == s.generation) && "TextureDesc.relativeTo is a stale/foreign handle");

    ResourceNode* resouce = s.m_allocator->make<ResourceNode>();
    resouce->handle = create_handle(s, ResourceKind::Texture);
    resouce->id = rid;
    resouce->kind = ResourceKind::Texture;
    resouce->persistent = true;
    resouce->historyIndex = 0; // the only layer
    apply_texture_desc(resouce, desc);
    list_append(&s.m_resouces, &s.m_resoucesTail, resouce);
    return TextureHandle { resouce->handle };
}

static bool is_depth_format(WGPUTextureFormat f)
{
    switch (f)
    {
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    default:
        return false;
    }
}

static bool format_has_stencil(WGPUTextureFormat f)
{
    switch (f)
    {
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    default:
        return false;
    }
}

TextureHandle RenderGraph::create_initialized_texture(std::string_view id, const TextureDesc& desc, WGPUColor fill)
{
    // the fill is a clear on a render target, and the graph cannot render into a 3D texture
    Q_ASSERT_X(desc.dimension != WGPUTextureDimension_3D, "create_initialized_texture",
        "3D textures cannot be cleared this way, the graph cannot use them as render targets");
    TextureHandle h = create_persistent_texture(id, desc);
    uint32_t layers = desc.absolute.depthOrArrayLayers ? desc.absolute.depthOrArrayLayers : 1;
    bool depth = is_depth_format(desc.format);
    Arena& scratch = storage(this)->m_allocator->scratch;

    for (uint32_t layer = 0; layer < layers; ++layer)
    {
        ScopedScratch ss(scratch);
        std::string_view passId = id;
        if (layers > 1) {
            const int n = std::snprintf(nullptr, 0, "%.*s.layer%u", (int)id.size(), id.data(), layer);
            char* buf = ss.alloc<char>(size_t(n) + 1);
            std::snprintf(buf, size_t(n) + 1, "%.*s.layer%u", (int)id.size(), id.data(), layer);
            passId = std::string_view(buf, size_t(n));
        }
        add_pass(
            passId,
            PassKind::Graphics,
            [h, depth, fill, layer](PassBuilder& b) {
                if (depth)
                    b.depth_stencil(h, { .clearDepth = (float)fill.r, .sub = { .layer = layer } });
                else
                    b.color(h, 0, { .clear = fill, .sub = { .layer = layer } });
                b.initialize(h);
            },
            [](PassContext&) {});
    }
    return h;
}

BufferHandle RenderGraph::create_initialized_buffer(std::string_view id, const BufferDesc& desc, const void* data)
{
    BufferHandle h = create_persistent_buffer(id, desc);
    RenderGraphStorage& s = *storage(this);
    // copy into the arena, the caller's data does not outlive this call
    uint8_t* bytes = s.m_allocator->alloc<uint8_t>(desc.size);
    if (data)
        std::memcpy(bytes, data, desc.size);
    uint64_t size = desc.size;
    add_pass(
        id,
        PassKind::Transfer,
        [h](PassBuilder& b) {
            b.host_write(h);
            b.initialize(h);
        },
        [h, bytes, size](PassContext& ctx) { wgpuQueueWriteBuffer(ctx.queue, ctx.buffer(h), 0, bytes, size); });
    return h;
}

void RenderGraph::begin_pass(PassBuilder& builder, std::string_view id, PassKind kind)
{
    RenderGraphStorage& s = *storage(this);
    Q_ASSERT(s.m_state == RenderGraphState::Recording && "Render graph is in read only mode after compile()");
    PassNode* pass = s.m_allocator->make<PassNode>();
    pass->id = intern_id(*s.m_allocator, id);
    pass->kind = kind;

    builder.m_pass = pass;
    builder.m_graph = this;
}

void RenderGraph::end_pass(PassBuilder& builder)
{
    PassNode* pass = builder.m_pass;
    for (uint32_t t = 0; t < pass->initCount; ++t)
    {
        bool wrote = false;
        for (uint32_t i = 0; i < pass->accessCount; ++i)
            if (pass->accesses[i].handle.id == pass->initTargets[t].target.id && access_is_write(pass->accesses[i].type)) {
                wrote = true;
                break;
            }
        Q_ASSERT(wrote
            && "initialize() target must also be written by this pass (e.g. .color()/.storage_write()). "
               "initialize() only gates skip/run, it is not itself a write");
    }
    RenderGraphStorage& s = *storage(this);
    list_append(&s.m_passes, &s.m_passesTail, pass);
}

void* RenderGraph::alloc_exec(size_t size, size_t align)
{
    return storage(this)->m_allocator->alloc_raw(size, align);
}

void RenderGraph::set_exec(PassBuilder& builder, void* obj, void (*fn)(void*, PassContext&))
{
    builder.m_pass->exec_obj = obj;
    builder.m_pass->exec_fn = fn;
}

static void add_dependency(GraphAllocator* alloc, PassNode* p, PassNode* dep)
{
    NodeAdjacency* link = alloc->make<NodeAdjacency>();
    link->pass = dep;
    link->next = p->adjacency;
    p->adjacency = link;
}

// one declaration-order sweep with implicit SSA resource versioning
static void sweep_resource_versions(Arena& scratch, PassNode* head, uint32_t next_resource_id, uint32_t passCount, GraphAllocator* alloc)
{
    ScopedScratch ss(scratch);
    PassNode** currentProducer = ss.alloc<PassNode*>(next_resource_id);
    NodeAdjacency** pendingReaders = ss.alloc<NodeAdjacency*>(next_resource_id);
    uint32_t* linkedBy = ss.alloc<uint32_t>(passCount);

    auto emit_edge = [&](PassNode* dependent, PassNode* dep) {
        if (linkedBy[dep->index] == dependent->index)
            return;
        linkedBy[dep->index] = dependent->index;
        add_dependency(alloc, dependent, dep);
    };

    for (PassNode* p = head; p; p = p->next) {
        if (p->skipInit)
            continue; // initialize() pass already satisfied
        for (uint32_t i = 0; i < p->accessCount; ++i) {
            uint32_t id = p->accesses[i].handle.id;
            if (!id)
                continue;
            if (access_is_write(p->accesses[i].type)) {
                // WAW: order this write after the previous writer. without it two writers of one
                // resource have no edge -> undefined order -> corruption.
                if (currentProducer[id] && currentProducer[id] != p)
                    emit_edge(p, currentProducer[id]); // WAW
                // WAR: order this write after every reader still using the version it clobbers.
                for (NodeAdjacency* r = pendingReaders[id]; r; r = r->next)
                    if (r->pass != p)
                        emit_edge(p, r->pass); // WAR
                currentProducer[id] = p; // new version
                pendingReaders[id] = nullptr; // readers retired
            } else {
                // RAW: this read depends on the producer of the version it sees. a read before any
                // writer binds to "no producer" (no edge).
                if (currentProducer[id] && currentProducer[id] != p)
                    emit_edge(p, currentProducer[id]); // RAW
                // register as a pending reader of the current version (for a future write's WAR).
                NodeAdjacency* link = ss.make<NodeAdjacency>();
                link->pass = p;
                link->next = pendingReaders[id];
                pendingReaders[id] = link;
            }
        }
    }
}

// one level of the explicit DFS stack
struct TopoFrame {
    PassNode* pass {};
    NodeAdjacency* next {}; // unvisited remainder of pass->adjacency
};

// post-order DFS
static void topo_visit(PassNode* root, PassNode** order, uint32_t& count, bool& hadCycle, TopoFrame* stack)
{
    if (root->onStack) { // trap for cycle detection
        hadCycle = true;
        return;
    }
    if (root->placed)
        return;

    uint32_t depth = 0;
    root->placed = root->onStack = true;
    stack[depth++] = { root, root->adjacency };

    while (depth) {
        TopoFrame& f = stack[depth - 1];
        if (f.next) {
            PassNode* child = f.next->pass;
            f.next = f.next->next;
            if (child->onStack) { // trap for cycle detection
                hadCycle = true;
                continue;
            }
            if (child->placed)
                continue;
            child->placed = child->onStack = true;
            stack[depth++] = { child, child->adjacency };
            continue;
        }

        f.pass->onStack = false;
        order[count++] = f.pass;
        --depth;
    }
}

static bool is_sink(PassNode* p, const bool* sinkRoot)
{
    for (uint32_t i = 0; i < p->accessCount; ++i)
        if (access_is_write(p->accesses[i].type) && sinkRoot[p->accesses[i].handle.id])
            return true;
    return false;
}

static WGPUExtent3D resolve_size(ResourceNode* r, RenderGraphStorage& s)
{
    if (r->imported || r->resolved.width)
        return r->resolved;
    if (r->sizeKind == SizeKind::Absolute) {
        r->resolved = r->absolute;
        if (!r->resolved.depthOrArrayLayers)
            r->resolved.depthOrArrayLayers = 1; // 0 layers means 1 everywhere else, see node_layers()
        return r->resolved;
    }
    if (r->resolving) {
        push_error(s,
            "resource \"%.*s\" sits on a cyclic relativeTo chain. A relative size must "
            "eventually resolve against an absolutely sized resource.",
            RG_NAME(r->id));
        return WGPUExtent3D {};
    }

    ResourceNode* base = r->relativeToHandle.id ? s.byId[r->relativeToHandle.id] : nullptr;
    if (!base) {
        push_error(s,
            "resource \"%.*s\" is SizeKind::Relative but names no resource to size against. "
            "set TextureDesc.relativeTo, or use SizeKind::Absolute.",
            RG_NAME(r->id));
        return r->resolved = WGPUExtent3D {};
    }
    r->resolving = true;
    WGPUExtent3D b = resolve_size(base, s);
    r->resolving = false;
    uint32_t layers = r->absolute.depthOrArrayLayers ? r->absolute.depthOrArrayLayers : 1;
    // round, don't truncate. clamp to 1
    uint32_t w = (uint32_t)(b.width * r->scaleX + 0.5f);
    uint32_t h = (uint32_t)(b.height * r->scaleY + 0.5f);
    return r->resolved = { w ? w : 1u, h ? h : 1u, layers };
}

static void detect_transient_attachments(RenderGraphStorage& s)
{
    for (ResourceNode* r = s.m_resouces; r; r = r->next) {
        if (r->is_external() || r->kind != ResourceKind::Texture)
            continue;
        if (r->texUsage != WGPUTextureUsage_RenderAttachment)
            continue;
        if (r->dimension != WGPUTextureDimension_2D || r->mipLevelCount != 1 || r->resolved.depthOrArrayLayers > 1)
            continue;

        // exactly one recorded access, otherwise its contents span passes and cannot be memoryless
        if (r->liveAccessCount != 1)
            continue;
        const ResourceAccess* a = r->soleAccess;

        // a writable color/depth attachment only
        if (a->type != AccessType::ColorAttachment && a->type != AccessType::DepthStencilAttachment)
            continue;
        if (a->loadOp != WGPULoadOp_Clear || a->storeOp != WGPUStoreOp_Discard)
            continue;

        if (a->type == AccessType::DepthStencilAttachment && a->stencilLoadOp != WGPULoadOp_Undefined
            && (a->stencilLoadOp != WGPULoadOp_Clear || a->stencilStoreOp != WGPUStoreOp_Discard))
            continue;

        r->transientAttachment = true;
        ++s.transientCount;
    }
}

// a history resource is two layer nodes sharing one pool entry
static void validate_history_layers(RenderGraphStorage& s)
{
    auto pool_used = [](const ResourceNode* r) { return r->kind == ResourceKind::Texture ? r->texUsage != 0 : r->bufUsage != 0; };
    for (ResourceNode* r = s.m_resouces; r; r = r->next) {
        if (!r->history || r->historyIndex != 0)
            continue; // anchor on .curr, skip .prev and single-layer persistent
        ResourceNode* prev = r->next; // .prev (layer 1) was appended right after .curr
        // both ids are interned, so canonical pointers match exactly where hashes could collide
        if (!prev || !prev->history || prev->id.name.data != r->id.name.data || prev->historyIndex != 1) {
            push_error(s,
                "history resource \"%.*s\": its .curr and .prev layer nodes are not adjacent in "
                "declaration order. This is an internal error, not an authoring one.",
                RG_NAME(r->id));
            continue;
        }
        if (pool_used(r) != pool_used(prev))
            push_error(s,
                "history resource \"%.*s\": .curr (layer 0) is %s but .prev (layer 1) is %s "
                "after culling; read .prev and write .curr in one pass, or use neither.",
                RG_NAME(r->id),
                pool_used(r) ? "used" : "unused",
                pool_used(prev) ? "used" : "unused");
    }
}

// validate cube/cubearray view layer counts
static void validate_cube_views(RenderGraphStorage& s);

// validate every access's subresource / byte range against the resource it names
static void validate_access_ranges(RenderGraphStorage& s);

// validate the render-pass descriptor each Graphics pass will build: attachment sizes and sample counts,
// resolve pairs, depth-stencil formats
static void validate_render_passes(RenderGraphStorage& s);

// compile()
static void compile_impl(RenderGraph* graph, bool enableAlias)
{
    auto t0 = std::chrono::steady_clock::now();
    RenderGraphStorage& s = *storage(graph);
    if (s.m_state != RenderGraphState::Recording) {
        static constexpr const char* kStateName[] = { "recording", "compiled", "failed", "finished" };
        push_error(s, "compile() called on a graph that is already %s. compile() runs once per recording.",
            kStateName[(int)s.m_state]);
        return;
    }

    // a handle this frame's graph did not create
    auto foreign = [&s](ResourceHandle h) { return h.id >= s.next_resource_id || h.generation != s.generation; };

    uint32_t passIndex = 0;
    for (PassNode* p = s.m_passes; p; p = p->next) {
        p->index = passIndex++;
        for (uint32_t i = 0; i < p->accessCount; ++i) {
            ResourceHandle h = p->accesses[i].handle;
            if (h.id == 0 || foreign(h)) {
                push_error(s,
                    "pass \"%.*s\" uses an invalid resource handle (id %u): zero, out of range, "
                    "or from another graph / a previous frame.",
                    RG_NAME(p->id),
                    h.id);
                return;
            }
        }
        for (uint32_t i = 0; i < p->initCount; ++i) {
            ResourceHandle t = p->initTargets[i].target;
            if (foreign(t)) {
                push_error(s,
                    "pass \"%.*s\" initialize() target handle (id %u) is from another graph or a previous frame.",
                    RG_NAME(p->id),
                    t.id);
                return;
            }
        }
    }
    s.passCount = passIndex;

    for (ResourceNode* r = s.m_resouces; r; r = r->next) {
        ResourceHandle rel = r->relativeToHandle;
        if (rel.id != 0 && foreign(rel)) {
            push_error(s,
                "resource \"%.*s\" relativeTo (id %u) is from another graph or a previous frame.",
                RG_NAME(r->id),
                rel.id);
            return;
        }
    }

    // build fast index based lookup
    s.byId = s.m_allocator->alloc<ResourceNode*>(s.next_resource_id);
    for (ResourceNode* r = s.m_resouces; r; r = r->next){
        if(r->handle.id < s.next_resource_id)
            s.byId[r->handle.id] = r;
    }

    WGPUTextureUsage* predTexUsage = s.m_allocator->alloc<WGPUTextureUsage>(s.next_resource_id);
    WGPUBufferUsage* predBufUsage = s.m_allocator->alloc<WGPUBufferUsage>(s.next_resource_id);
    bool* hasWriter = s.m_allocator->alloc<bool>(s.next_resource_id);
    for (PassNode* p = s.m_passes; p; p = p->next)
        for (uint32_t i = 0; i < p->accessCount; ++i) {
            const ResourceAccess& a = p->accesses[i];
            predTexUsage[a.handle.id] |= tex_usage_of(a.type);
            predBufUsage[a.handle.id] |= buf_usage_of(a.type);
            hasWriter[a.handle.id] |= access_is_write(a.type); // OR: any declared write, not just the last access to id
        }

    // phase 0: skip up-to-date bake passes. a skipped pass produces no version and is not a cull root, so
    // phases 1-2 drop it. readers still bind to the pooled result.
    for (PassNode* p = s.m_passes; p; p = p->next) {
        p->skipInit = false;
        if (!p->initCount)
            continue;
        // skip only when every target is realized and baked with its exact hash. one stale target re-arms
        // the pass, since a body bakes all its targets or none.
        bool allBaked = true;
        for (uint32_t i = 0; i < p->initCount; ++i) {
            ResourceNode* r = s.byId[p->initTargets[i].target.id]; // non-null: backstop range-checked the id
            if (!r->persistent) {
                push_error(s, "initialize() target must be persistent or history (create_persistent_texture/_buffer or create_history_texture/_buffer)");
                allBaked = false;
                continue;
            }
            PersistentResourcePool::Entry* e = s.m_allocator->pool.find(r->id);
            bool upToDate = e && e->created && e->baked && e->initHash == p->initTargets[i].hash;
            // a matching bake is still stale if realize() would recreate the entry after skipInit was
            // decided, leaving it blank for a frame.
            bool wouldRecreate = false;
            if (e && e->created) {
                // same predicate realize_*_entry uses, so the prediction cannot drift from the decision.
                if (r->kind == ResourceKind::Texture) {
                    resolve_size(r, s); // tex_signature reads r->resolved
                    wouldRecreate = PersistentResourcePool::tex_entry_would_recreate(*e, tex_signature(r), predTexUsage[r->handle.id]);
                } else {
                    wouldRecreate = PersistentResourcePool::buf_entry_would_recreate(*e, r->bufferSize, predBufUsage[r->handle.id]);
                }
                // touch() also recreates on a history-hash mismatch, and has not run yet at phase-0 time.
                wouldRecreate = wouldRecreate || PersistentResourcePool::history_hash_forces_destroy(*e, r->historyHash);
            }
            if (!upToDate || wouldRecreate)
                allBaked = false;
        }
        p->skipInit = allBaked;
    }
    if (s.m_state == RenderGraphState::Failed) // a non-persistent initialize() target or a cyclic relativeTo chain
        return;

    // phase 1: build the dependency DAG in the "depends-on" direction. reads before any writer get no edge
    // here, the post-cull pass turns them into errors.
    sweep_resource_versions(s.m_allocator->scratch, s.m_passes, s.next_resource_id, s.passCount, s.m_allocator);

    // phase 2: cull dead passes and topo sort, fused into one DFS seeded from sinks.
    {
        // sinkRoot = imported or history .curr, whose value must survive without a same-frame reader.
        // persistent bakes are excluded on purpose, they are pool-cached and dependency-driven.
        ScopedScratch ss(s.m_allocator->scratch);
        bool* sinkRoot = ss.alloc<bool>(s.next_resource_id);
        for (ResourceNode* r = s.m_resouces; r; r = r->next)
            sinkRoot[r->handle.id] = r->imported || r->history;

        // topo into a scratch array, then relink the intrusive list into execution order
        PassNode** order = ss.alloc<PassNode*>(s.passCount);
        TopoFrame* topoStack = ss.alloc<TopoFrame>(s.passCount);
        uint32_t count = 0;
        bool hadCycle = false;
        for (PassNode* p = s.m_passes; p; p = p->next)
            if (!p->skipInit
                && (is_sink(p, sinkRoot) || p->forceKeep)) // satisfied initialize() pass is not a root; force_keep() keeps a reader-less side-effect pass alive
            {
                p->sink = true;
                topo_visit(p, order, count, hadCycle, topoStack); // only reaches passes that feed a sink
            }

        // acyclic by construction, phase 1 emits only backward edges, so a cycle means corruption.
        if (hadCycle) {
            push_error(s,
                "compile() found a cycle in the pass dependency graph. The graph is "
                "acyclic by construction, so this is an internal error, not an authoring one.");
            return;
        }

        // relink next-pointers to follow topo order
        for (uint32_t i = 0; i + 1 < count; ++i)
            order[i]->next = order[i + 1];
        if (count)
            order[count - 1]->next = nullptr;
        s.m_passes = count ? order[0] : nullptr;
        s.m_passesTail = count ? order[count - 1] : nullptr;
    }

    // post-cull validation over the final schedule: reading a transient no earlier surviving pass produced
    // samples uninitialized pool contents. external resources are exempt, their value comes from outside
    // the frame. bails before phase 3, so a misordered graph never reaches realize().
    {
        ScopedScratch ss(s.m_allocator->scratch);
        bool* produced = ss.alloc<bool>(s.next_resource_id); // a surviving pass has written it so far
        bool* external = ss.alloc<bool>(s.next_resource_id); // imported or history
        bool* prevLayer = ss.alloc<bool>(s.next_resource_id); // history layer k>0, read-only this frame
        for (ResourceNode* r = s.m_resouces; r; r = r->next) {
            external[r->handle.id] = r->is_external();
            prevLayer[r->handle.id] = r->persistent && r->historyIndex != 0;
        }

        bool hadError = false;
        for (PassNode* p = s.m_passes; p; p = p->next)
            for (uint32_t i = 0; i < p->accessCount; ++i) {
                uint32_t id = p->accesses[i].handle.id;
                if (access_is_write(p->accesses[i].type)) {
                    produced[id] = true;
                    // older layers of a history resource are read-only this frame, .curr is the only legal
                    // write target. writing layer k>0 clobbers a slot that becomes a future current.
                    if (prevLayer[id]) {
                        ResourceNode* w = s.byId[id]; // non-null: prevLayer is only set from a live node
                        push_error(s,
                            "pass \"%.*s\" writes history resource \"%.*s\" "
                            "layer %u; only layer 0, the .curr handle, is writable.",
                            RG_NAME(p->id),
                            RG_NAME(w->id),
                            w->historyIndex);
                        hadError = true;
                    }
                    continue;
                }
                if (id == 0 || external[id] || produced[id])
                    continue;
                ResourceNode* r = s.byId[id]; // non-null: the handle backstop range-checked every access id

                // a write to id in THIS pass makes the read an in-place read_write, which needs its own
                // diagnosis below.
                bool samePassWrite = false;
                for (uint32_t j = 0; j < p->accessCount; ++j)
                    if (p->accesses[j].handle.id == id && access_is_write(p->accesses[j].type)) {
                        samePassWrite = true;
                        break;
                    }

                // three diagnoses of one shape, all naming the pass then the resource twice
                const char* fmt = !hasWriter[id]
                    ? "pass \"%.*s\" reads transient resource \"%.*s\" that no pass ever writes; "
                      "a transient starts uninitialized, so write \"%.*s\" before reading it."
                    : samePassWrite
                    ? "pass \"%.*s\" read-modify-writes transient resource \"%.*s\" that no earlier pass "
                      "produced; the same-pass write can't seed the read (it sees uninitialized contents). "
                      "Write \"%.*s\" in an earlier pass, or use storage_write if the shader is write-only."
                    : "pass \"%.*s\" reads resource \"%.*s\" before any pass "
                      "writes it; declare a writer of \"%.*s\" first.";
                push_error(s, fmt, RG_NAME(p->id), RG_NAME(r->id), RG_NAME(r->id));
                hadError = true;
            }
        if (hadError)
            return;
    }

    // phase 3: accumulate WGPU usage and resolve concrete sizes. WebGPU requires the usage bits at create
    // time, so realize() is left with only the device create calls.
    {
        uint32_t passIdx = 0;
        for (PassNode* p = s.m_passes; p; p = p->next, ++passIdx) // m_passes == surviving (post-cull) passes
            for (uint32_t i = 0; i < p->accessCount; ++i) {
                ResourceNode* r = s.byId[p->accesses[i].handle.id];
                if (!r)
                    continue;
                // transient-attachment bookkeeping: count live accesses and keep the last one
                ++r->liveAccessCount;
                r->soleAccess = &p->accesses[i];
                // lifetime: the walk is in execution order, so the first touch is firstUse and each later
                // one overwrites lastUse.
                if (!r->is_external()) { // external is pool- or caller-owned -> excluded from aliasing
                    if (r->firstUse == ResourceNode::kNoPass) {
                        r->firstUse = passIdx;
                        r->firstDefines = access_defines(p->accesses[i]); // aliasing: does the first touch overwrite?
                    }
                    r->lastUse = passIdx;
                    if (access_is_write(p->accesses[i].type))
                        r->hasWriter = true;
                }
                // tex_usage_of/buf_usage_of are kind-agnostic, so dispatch on the node kind
                if (r->kind == ResourceKind::Texture)
                    r->texUsage |= tex_usage_of(p->accesses[i].type);
                else
                    r->bufUsage |= buf_usage_of(p->accesses[i].type);
            }

        // walk each texture's relativeTo chain. memoized and recursive, so any declaration order works.
        for (ResourceNode* r = s.m_resouces; r; r = r->next)
            if (r->kind == ResourceKind::Texture)
                resolve_size(r, s);
        if (s.m_state == RenderGraphState::Failed) // a cyclic relativeTo chain leaves sizes unresolved
            return;

        // a zero extent would reach wgpuDeviceCreateTexture and surface as a device validation error with
        // no link back to the descriptor that caused it.
        for (ResourceNode* r = s.m_resouces; r; r = r->next) {
            if (r->kind != ResourceKind::Texture || !r->texUsage)
                continue;
            if (r->resolved.width && r->resolved.height)
                continue;
            push_error(s,
                "resource \"%.*s\" resolves to a zero extent (%u x %u); a texture needs a non-zero width "
                "and height. Check its TextureDesc.absolute, or the relativeTo base it scales against.",
                RG_NAME(r->id),
                r->resolved.width,
                r->resolved.height);
        }
        if (s.m_state == RenderGraphState::Failed)
            return;

        // NOTE(Huerbe): usage==0 here == untouched by a live pass -> realize() skips it = free
        // dead-resource culling. no separate resource liveness list needed.
    }

    // reject a history whose .curr/.prev layers ended up asymmetrically culled. needs phase-3 usage.
    validate_history_layers(s);
    if (s.m_state == RenderGraphState::Failed)
        return;

    // reject misauthored cube/cubearray views (needs the resolved layer counts from phase-3).
    validate_cube_views(s);
    if (s.m_state == RenderGraphState::Failed)
        return;

    // reject accesses whose subresource/byte range does not fit the resource (same phase-3 dependency).
    validate_access_ranges(s);
    if (s.m_state == RenderGraphState::Failed)
        return;

    // reject render passes WebGPU would refuse. runs after the range checks, so the attachment subresources
    // it measures are already known to be in range.
    validate_render_passes(s);
    if (s.m_state == RenderGraphState::Failed)
        return;

    // infer transient (memoryless) attachments from the usage gather.
    detect_transient_attachments(s);

    // phase 4: transient memory aliasing (opt-in). pack disjoint-lifetime, same-signature transients onto
    // a shared physical object
    if (enableAlias)
    {
        ScopedScratch ss(s.m_allocator->scratch);
        ResourceNode** elig = ss.alloc<ResourceNode*>(s.next_resource_id);
        uint32_t nElig = 0;
        for (ResourceNode* r = s.m_resouces; r; r = r->next) {
            // a transientAttachment is memoryless, so there is no storage to alias
            if (r->is_external() || r->firstUse == ResourceNode::kNoPass || !r->hasWriter || !r->firstDefines || r->transientAttachment)
                continue;
            if (r->kind == ResourceKind::Texture && (r->mipLevelCount != 1 || r->sampleCount != 1 || r->resolved.depthOrArrayLayers > 1))
                continue;
            elig[nElig++] = r;
        }

        // by firstUse asc. stable so equal firstUse keeps declaration order, which first-fit below depends on
        std::stable_sort(elig, elig + nElig, [](const ResourceNode* a, const ResourceNode* b) { return a->firstUse < b->firstUse; });

        // first-fit: reuse the first signature-matching slot whose occupant is already dead, else open a
        // new one. strict freeFrom < firstUse, equality means they share a pass, which WebGPU forbids.
        if (nElig)
            s.m_slots = s.m_allocator->alloc<PhysicalResource>(nElig);
        for (uint32_t i = 0; i < nElig; ++i) {
            ResourceNode* r = elig[i];
            const bool isBuf = r->kind == ResourceKind::Buffer;
            // the signature match below assumes the eligibility filter kept mip>1 / MSAA nodes out. were
            // that filter to change, the match would silently pass them, so trip here instead.
            if (!isBuf && (r->mipLevelCount != 1 || r->sampleCount != 1)) {
                push_error(s,
                    "resource \"%.*s\" reached alias slot assignment with %u mip(s) and %u sample(s); "
                    "aliasing eligibility must exclude mip>1 / MSAA nodes. This is an internal error, "
                    "not an authoring one.",
                    RG_NAME(r->id),
                    r->mipLevelCount,
                    r->sampleCount);
                return;
            }
            uint32_t slot = ResourceNode::kNoSlot;
            for (uint32_t k = 0; k < s.m_slotCount; ++k) {
                PhysicalResource& ph = s.m_slots[k];
                if (ph.kind != r->kind)
                    continue; // never mix textures + buffers in one slot
                const bool sig = isBuf ? (ph.bufferSize == r->bufferSize) : (ph.sig == tex_signature(r));
                if (sig && ph.freeFrom < r->firstUse) {
                    slot = k;
                    break;
                } // no shared pass
            }
            if (slot == ResourceNode::kNoSlot) {
                slot = s.m_slotCount++;
                PhysicalResource& ph = s.m_slots[slot];
                ph.kind = r->kind;
                ph.identity = r->id.name.data; // the member that opens the slot names it, see PhysicalResource
                if (isBuf)
                    ph.bufferSize = r->bufferSize;
                else
                    ph.sig = tex_signature(r);
            }
            PhysicalResource& ph = s.m_slots[slot];
            if (isBuf)
                ph.bufUsage |= r->bufUsage; // widen to the union: every member shares this one object
            else
                ph.texUsage |= r->texUsage;
            ph.freeFrom = r->lastUse;
            r->aliasSlot = slot;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    s.timing_compile_us = std::chrono::duration<float, std::micro>(t1 - t0).count();

    s.m_state = (s.m_errors == nullptr) ? RenderGraphState::Compiled : RenderGraphState::Failed;
}

ErrorMessage* RenderGraph::compile(bool enableAlias)
{
    compile_impl(this, enableAlias);
    return get_errors();
}

static uint32_t node_layers(const ResourceNode* r)
{
    return (r->dimension == WGPUTextureDimension_2D) ? (r->resolved.depthOrArrayLayers ? r->resolved.depthOrArrayLayers : 1) : 1;
}

static uint32_t mip_dim(uint32_t v, uint32_t mip)
{
    v >>= mip;
    return v ? v : 1;
}

static void validate_cube_views(RenderGraphStorage& s)
{
    for (PassNode* p = s.m_passes; p; p = p->next)
        for (uint32_t i = 0; i < p->accessCount; ++i) {
            const ResourceAccess& a = p->accesses[i];
            if (a.viewDim != WGPUTextureViewDimension_Cube && a.viewDim != WGPUTextureViewDimension_CubeArray)
                continue;
            ResourceNode* r = s.byId[a.handle.id];
            if (!r || r->kind != ResourceKind::Texture)
                continue;

            if (a.type == AccessType::StorageRead || a.type == AccessType::StorageWrite) {
                push_error(s, "pass \"%.*s\": resource \"%.*s\" requests a cube view on a storage access; "
                    "WebGPU storage textures cannot be cube.", RG_NAME(p->id), RG_NAME(r->id));
                continue;
            }
            if (r->dimension != WGPUTextureDimension_2D) {
                push_error(s, "pass \"%.*s\": resource \"%.*s\" requests a cube view but is not a 2D texture; "
                    "cube views require a 2D texture with 6 (cube) or 6*N (cube array) layers.", RG_NAME(p->id), RG_NAME(r->id));
                continue;
            }
            uint32_t layers = node_layers(r);
            uint32_t layerCount = a.layerCount ? a.layerCount : (layers - a.baseLayer); // 0 == all remaining
            if (a.viewDim == WGPUTextureViewDimension_Cube && layerCount != 6)
                push_error(s, "pass \"%.*s\": cube view of \"%.*s\" covers %u layer(s); a cube view needs exactly 6. "
                    "Use ViewRange.layerCount = 6 (e.g. rg::cube()).", RG_NAME(p->id), RG_NAME(r->id), layerCount);
            else if (a.viewDim == WGPUTextureViewDimension_CubeArray && (layerCount == 0 || layerCount % 6 != 0))
                push_error(s, "pass \"%.*s\": cube-array view of \"%.*s\" covers %u layer(s); a cube array needs a "
                    "positive multiple of 6 (e.g. rg::cube_array(n)).", RG_NAME(p->id), RG_NAME(r->id), layerCount);
            else if (a.baseLayer + layerCount > layers)
                push_error(s, "pass \"%.*s\": cube view of \"%.*s\" (baseLayer %u + %u layers) exceeds the "
                    "texture's %u layer(s).", RG_NAME(p->id), RG_NAME(r->id), (uint32_t)a.baseLayer, layerCount, layers);
        }
}

// render-pass rules WebGPU enforces on the descriptor execute() builds. checking them here names the pass
// and the resources that disagree, where the device can only name the descriptor it was handed.
static void validate_render_passes(RenderGraphStorage& s)
{
    struct Att {
        const ResourceAccess* a;
        ResourceNode* r;
    };

    // an attachment view is a single mip, so its size is that mip's size
    auto att_w = [](const Att& t) { return mip_dim(t.r->resolved.width, t.a->baseMip); };
    auto att_h = [](const Att& t) { return mip_dim(t.r->resolved.height, t.a->baseMip); };

    for (PassNode* p = s.m_passes; p; p = p->next) {
        if (p->kind != PassKind::Graphics)
            continue;

        Att color[kMaxColorAttachments] {};
        uint32_t colorCount = 0;
        Att resolve[kMaxColorAttachments] {};
        uint32_t resolveCount = 0;
        Att depth {};
        bool hasDepth = false;

        for (uint32_t i = 0; i < p->accessCount; ++i) {
            const ResourceAccess& a = p->accesses[i];
            ResourceNode* r = s.byId[a.handle.id];
            if (!r)
                continue;
            const Att t { &a, r };
            if (a.type == AccessType::ColorAttachment && colorCount < kMaxColorAttachments)
                color[colorCount++] = t;
            else if (a.type == AccessType::ResolveAttachment && resolveCount < kMaxColorAttachments)
                resolve[resolveCount++] = t;
            else if (a.type == AccessType::DepthStencilAttachment || a.type == AccessType::DepthStencilReadOnly) {
                depth = t;
                hasDepth = true;
            } else
                continue;

            // execute() always leaves depthSlice at UNDEFINED, which WebGPU requires a 3D color target to
            // set, so a 3D attachment can never be valid here
            if (r->dimension == WGPUTextureDimension_3D)
                push_error(s,
                    "pass \"%.*s\": resource \"%.*s\" is a 3D texture used as a render target; the graph "
                    "does not set depthSlice, so it cannot render into one. Use a 2D array texture.",
                    RG_NAME(p->id), RG_NAME(r->id));
        }

        if (!colorCount && !hasDepth) {
            push_error(s,
                "pass \"%.*s\" is a Graphics pass with no attachments; a render pass needs at least one "
                "color() or a depth_stencil(). Use PassKind::Compute if the pass records no draws.",
                RG_NAME(p->id));
            continue;
        }

        // every color and depth attachment shares one size and sample count. resolve targets are excluded,
        // they are single-sample by definition and checked against their own color below.
        const Att& ref = colorCount ? color[0] : depth;
        auto check_against_ref = [&](const Att& t) {
            if (att_w(t) != att_w(ref) || att_h(t) != att_h(ref))
                push_error(s,
                    "pass \"%.*s\": attachment \"%.*s\" is %u x %u but \"%.*s\" is %u x %u; every "
                    "attachment in a render pass must be the same size.",
                    RG_NAME(p->id), RG_NAME(t.r->id), att_w(t), att_h(t), RG_NAME(ref.r->id), att_w(ref), att_h(ref));
            if (t.r->sampleCount != ref.r->sampleCount)
                push_error(s,
                    "pass \"%.*s\": attachment \"%.*s\" has %u sample(s) but \"%.*s\" has %u; every "
                    "attachment in a render pass must have the same sample count.",
                    RG_NAME(p->id), RG_NAME(t.r->id), t.r->sampleCount, RG_NAME(ref.r->id), ref.r->sampleCount);
        };
        for (uint32_t i = 0; i < colorCount; ++i)
            check_against_ref(color[i]);
        if (hasDepth)
            check_against_ref(depth);

        if (hasDepth) {
            const ResourceAccess& a = *depth.a;
            if (!is_depth_format(depth.r->format))
                push_error(s,
                    "pass \"%.*s\": resource \"%.*s\" is bound as the depth-stencil attachment but its "
                    "format is not a depth format.",
                    RG_NAME(p->id), RG_NAME(depth.r->id));
            else {
                const bool stencilOps = a.stencilLoadOp != WGPULoadOp_Undefined || a.stencilStoreOp != WGPUStoreOp_Undefined;
                const bool hasStencil = format_has_stencil(depth.r->format);
                if (stencilOps && !hasStencil)
                    push_error(s,
                        "pass \"%.*s\": depth-stencil attachment \"%.*s\" declares stencil load/store ops "
                        "but its format has no stencil aspect.",
                        RG_NAME(p->id), RG_NAME(depth.r->id));
                // read-only binds both aspects read-only, so WebGPU asks for no ops at all
                if (!stencilOps && hasStencil && a.type == AccessType::DepthStencilAttachment)
                    push_error(s,
                        "pass \"%.*s\": depth-stencil attachment \"%.*s\" has a stencil aspect, so WebGPU "
                        "requires stencil load/store ops; pass them to depth_stencil().",
                        RG_NAME(p->id), RG_NAME(depth.r->id));
            }
        }

        // a resolve target takes the multisampled colour of the slot it was declared against
        for (uint32_t i = 0; i < resolveCount; ++i) {
            const Att& t = resolve[i];
            const Att* src = nullptr;
            for (uint32_t k = 0; k < colorCount; ++k)
                if (color[k].a->colorIndex == t.a->colorIndex) {
                    src = &color[k];
                    break;
                }
            if (!src)
                continue; // resolve() rejects a src that is not a color() in this pass
            if (src->r->sampleCount <= 1)
                push_error(s,
                    "pass \"%.*s\": \"%.*s\" resolves \"%.*s\", which has %u sample(s); a resolve source "
                    "must be multisampled.",
                    RG_NAME(p->id), RG_NAME(t.r->id), RG_NAME(src->r->id), src->r->sampleCount);
            if (t.r->sampleCount != 1)
                push_error(s,
                    "pass \"%.*s\": resolve target \"%.*s\" has %u sample(s); a resolve target must be "
                    "single-sampled.",
                    RG_NAME(p->id), RG_NAME(t.r->id), t.r->sampleCount);
            if (t.r->format != src->r->format)
                push_error(s,
                    "pass \"%.*s\": resolve target \"%.*s\" and its source \"%.*s\" must have the same "
                    "format.",
                    RG_NAME(p->id), RG_NAME(t.r->id), RG_NAME(src->r->id));
            if (att_w(t) != att_w(*src) || att_h(t) != att_h(*src))
                push_error(s,
                    "pass \"%.*s\": resolve target \"%.*s\" is %u x %u but its source \"%.*s\" is %u x %u; "
                    "they must be the same size.",
                    RG_NAME(p->id), RG_NAME(t.r->id), att_w(t), att_h(t), RG_NAME(src->r->id), att_w(*src), att_h(*src));
        }
    }
}

// true for the access types whose declared BufferRange ctx.bind() applies
static bool is_bind_access(AccessType t) { return t == AccessType::Uniform || t == AccessType::StorageRead || t == AccessType::StorageWrite; }

static void validate_access_ranges(RenderGraphStorage& s)
{
    for (PassNode* p = s.m_passes; p; p = p->next)
        for (uint32_t i = 0; i < p->accessCount; ++i) {
            const ResourceAccess& a = p->accesses[i];
            ResourceNode* r = s.byId[a.handle.id];
            if (!r)
                continue;

            if (r->kind == ResourceKind::Texture) {
                const uint32_t mips = r->mipLevelCount;
                const uint32_t layers = node_layers(r);
                if (a.baseMip >= mips) {
                    push_error(s, "pass \"%.*s\": resource \"%.*s\" is accessed at mip %u but has %u mip(s).",
                        RG_NAME(p->id), RG_NAME(r->id), (uint32_t)a.baseMip, mips);
                    continue;
                }
                if (a.baseLayer >= layers) {
                    push_error(s, "pass \"%.*s\": resource \"%.*s\" is accessed at layer %u but has %u layer(s).",
                        RG_NAME(p->id), RG_NAME(r->id), (uint32_t)a.baseLayer, layers);
                    continue;
                }
                // a count of 0 means all remaining, so only an explicit count can overrun.
                if (a.mipCount && uint32_t(a.baseMip) + a.mipCount > mips)
                    push_error(s, "pass \"%.*s\": resource \"%.*s\" declares a view of %u mip(s) from mip %u but has %u mip(s).",
                        RG_NAME(p->id), RG_NAME(r->id), (uint32_t)a.mipCount, (uint32_t)a.baseMip, mips);
                if (a.layerCount && uint32_t(a.baseLayer) + a.layerCount > layers)
                    push_error(s, "pass \"%.*s\": resource \"%.*s\" declares a view of %u layer(s) from layer %u but has %u layer(s).",
                        RG_NAME(p->id), RG_NAME(r->id), (uint32_t)a.layerCount, (uint32_t)a.baseLayer, layers);
                continue;
            }

            if (!a.bufSize && !a.bufOffset)
                continue;
            if (a.bufOffset + a.bufSize > r->bufferSize)
                push_error(s, "pass \"%.*s\": range on \"%.*s\" covers bytes [%llu, %llu) but the buffer is %llu byte(s).",
                    RG_NAME(p->id), RG_NAME(r->id), (unsigned long long)a.bufOffset, (unsigned long long)(a.bufOffset + a.bufSize),
                    (unsigned long long)r->bufferSize);
            if (is_bind_access(a.type)) {
                // a declared offset at or past the end resolves to a zero-byte range
                if (!a.bufSize)
                    push_error(s, "pass \"%.*s\": range on \"%.*s\" starts at byte %llu but the buffer is %llu byte(s).",
                        RG_NAME(p->id), RG_NAME(r->id), (unsigned long long)a.bufOffset, (unsigned long long)r->bufferSize);
                // 256 is the spec-default minUniform/StorageBufferOffsetAlignment. the graph checks
                // the portable default rather than querying device limits.
                if (a.bufOffset % 256 != 0)
                    push_error(s, "pass \"%.*s\": binding range on \"%.*s\" starts at byte %llu, which is not 256-byte aligned.",
                        RG_NAME(p->id), RG_NAME(r->id), (unsigned long long)a.bufOffset);
            }
        }
}

static WGPUTextureViewDimension default_view_dim(const ResourceNode* r, uint32_t layerCount)
{
    return (r->dimension == WGPUTextureDimension_3D) ? WGPUTextureViewDimension_3D
        : (layerCount > 1)                           ? WGPUTextureViewDimension_2DArray
                                                     : WGPUTextureViewDimension_2D;
}

static Subview** find_subview_list(GraphAllocator* a, WGPUTexture tex)
{
    if (!tex)
        return nullptr;
    for (TransientResourcePool::Entry* e = a->transient.entries; e; e = e->next)
        if (e->kind == ResourceKind::Texture && e->tex == tex)
            return &e->subviews;
    for (PersistentResourcePool::Entry* e = a->pool.entries; e; e = e->next)
        for (uint32_t i = 0; i < PersistentResourcePool::kLayers; ++i)
            if (e->tex[i] == tex)
                return &e->subviews[i];
    return nullptr;
}


static ViewKey view_signature_for(const ResourceNode* r, const ResourceAccess& a)
{
    uint32_t layers = node_layers(r);
    uint32_t mipCount = a.mipCount ? a.mipCount : (r->mipLevelCount - a.baseMip);
    uint32_t layerCount = a.layerCount ? a.layerCount : (layers - a.baseLayer);
    WGPUTextureViewDimension dim = a.viewDim;
    if (dim == WGPUTextureViewDimension_Undefined)
        dim = default_view_dim(r, layerCount);
    return ViewKey {
        .format = r->format,
        .dimension = dim,
        .aspect = a.viewAspect,
        .baseMipLevel = a.baseMip,
        .mipLevelCount = mipCount,
        .baseArrayLayer = a.baseLayer,
        .arrayLayerCount = layerCount,
    };
}

static bool is_full_view(const ResourceNode* r, const ViewKey& k)
{
    uint32_t layers = node_layers(r);
    WGPUTextureViewDimension full = default_view_dim(r, layers);
    return k.baseMipLevel == 0 && k.mipLevelCount == r->mipLevelCount && k.baseArrayLayer == 0 && k.arrayLayerCount == layers
        && k.aspect == WGPUTextureAspect_All && k.format == r->format && k.dimension == full;
}

static WGPUTextureView resolve_view(RenderGraphStorage& s, ResourceNode* r, const ViewKey& key)
{
    if (!r->texture) { // imported
        Q_ASSERT(key.baseMipLevel == 0 && key.baseArrayLayer == 0 && "subresource selection on an imported texture is ignored (caller owns the view)");
        Q_ASSERT(key.aspect == WGPUTextureAspect_All && key.format == r->format
            && "aspect/format reinterpretation on an imported texture is ignored (caller owns the view)");
        return r->view;
    }
    if (is_full_view(r, key))
        return r->view;
    if (r->subviews)
        return subview_for(s.m_allocator->subviews, *r->subviews, r->texture, key);

    if (s.viewScratchN == s.viewScratchCap)
    {
        uint32_t newCap = s.viewScratchCap ? s.viewScratchCap * 2 : 8;
        WGPUTextureView* grown = s.m_allocator->scratch.alloc<WGPUTextureView>(newCap);
        if (s.viewScratchN)
            std::memcpy(grown, s.viewScratch, (size_t)s.viewScratchN * sizeof(WGPUTextureView));
        s.viewScratch = grown;
        s.viewScratchCap = newCap;
    }
    WGPUTextureViewDescriptor desc = key.desc();
    return s.viewScratch[s.viewScratchN++] = wgpuTextureCreateView(r->texture, &desc);
}

static const ResourceAccess* find_pass_access(const PassNode* pass, ResourceHandle h)
{
    for (uint32_t i = 0; i < pass->accessCount; ++i)
        if (pass->accesses[i].handle.id == h.id)
            return &pass->accesses[i];
    return nullptr;
}

static const ResourceAccess* find_copy_access(PassNode* pass, ResourceHandle h, bool wantDst, const char* who)
{
    const ResourceAccess* found = nullptr;
    AccessType want = wantDst ? AccessType::CopyDst : AccessType::CopySrc;
    for (uint32_t i = 0; i < pass->accessCount; ++i)
    {
        const ResourceAccess& a = pass->accesses[i];
        if (a.handle.id != h.id || a.type != want)
            continue;
        Q_ASSERT(!found && "ctx: two copy declarations pair the same handle+direction in this pass; ambiguous subresource");
        found = &a;
    }
    Q_ASSERT_X(found, "find_copy_access", who);
    return found;
}

// finds the bind-type access for h
static const ResourceAccess* find_bind_access(PassNode* pass, ResourceHandle h)
{
    const ResourceAccess* found = nullptr;
    for (uint32_t i = 0; i < pass->accessCount; ++i)
    {
        const ResourceAccess& a = pass->accesses[i];
        if (a.handle.id != h.id || !is_bind_access(a.type))
            continue;
        Q_ASSERT((!found || (found->bufOffset == a.bufOffset && found->bufSize == a.bufSize))
            && "ctx.bind: two bind declarations give the same buffer different ranges in this pass; ambiguous bind");
        found = &a;
    }
    return found;
}

static WGPUExtent3D copy_extent_for(RenderGraph* graph, PassNode* pass, ResourceHandle h, bool wantDst)
{
    Q_ASSERT(h.kind == ResourceKind::Texture);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    const ResourceAccess* a = find_copy_access(pass, h, wantDst, "copy_extent: no matching copy declared with this handle in this pass");
    uint32_t depth;
    if (r->dimension == WGPUTextureDimension_3D)
        depth = mip_dim(r->resolved.depthOrArrayLayers, a->baseMip); // baseLayer is 0 here, node_layers() reports 1 for 3D so validate_access_ranges rejects any other
    else
        depth = a->layerCount ? a->layerCount : (node_layers(r) - a->baseLayer); // 0 == all remaining
    return WGPUExtent3D { mip_dim(r->resolved.width, a->baseMip), mip_dim(r->resolved.height, a->baseMip), depth };
}

static bool history_valid_impl(RenderGraph* graph, ResourceHandle h)
{
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "ctx: failed to find node! Handle is 0 or from another graph");
    if (!r)
        return false;
    Q_ASSERT(r->persistent && "ctx: can only be called on persistent resources");
    return r->historyValid;
}

bool PassContext::history_valid_impl(ResourceHandle prev) const
{
    return webgpu::rg::history_valid_impl(graph, prev);
}

WGPUTextureView PassContext::view(TextureHandle h) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture && "ctx.view: only textures can create a view");
    Q_ASSERT(pass);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r)
    {
        return {};
    }
    Q_ASSERT(find_pass_access(pass, h) && "ctx.view: resource not declared as an access in this pass's setup");
    if (!r->texture)
        return r->view;

    const ResourceAccess* rd = nullptr;
    const ResourceAccess* wr = nullptr;
    for (uint32_t i = 0; i < pass->accessCount; ++i) {
        const ResourceAccess& a = pass->accesses[i];
        if (a.handle.id != h.id)
            continue;
        if (!access_is_write(a.type)) {
            Q_ASSERT(!rd && "ctx.view: two reads of one handle in a pass; ambiguous subresource; use ctx.texture");
            rd = &a;
        } else {
            wr = &a;
        }
    }
    const ResourceAccess* acc = rd ? rd : wr;
    if (!acc)
        return r->view;
    RenderGraphStorage& s = *storage(graph);
    return resolve_view(s, r, view_signature_for(r, *acc));
}


static bool subres_declared(const PassNode* pass, const ResourceNode* r, ResourceHandle h, uint32_t mip, uint32_t layer)
{
    for (uint32_t i = 0; i < pass->accessCount; ++i) {
        const ResourceAccess& a = pass->accesses[i];
        if (a.handle.id != h.id)
            continue;
        uint32_t mipEnd = a.mipCount ? a.baseMip + a.mipCount : r->mipLevelCount;
        uint32_t layerEnd = a.layerCount ? a.baseLayer + a.layerCount : node_layers(r);
        if (mip >= a.baseMip && mip < mipEnd && layer >= a.baseLayer && layer < layerEnd)
            return true;
    }
    return false;
}

WGPUTextureView PassContext::view(TextureHandle h, uint32_t mip, uint32_t layer) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture && "ctx.view: only textures can create a view");
    Q_ASSERT(pass);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return {};
    }
    Q_ASSERT(find_pass_access(pass, h) && "ctx.view: resource not declared as an access in this pass's setup");
    if (!r->texture) { // imported: the caller-owned view spans the whole resource
        Q_ASSERT(mip == 0 && layer == 0 && "subresource selection on an imported texture is ignored (caller owns the view)");
        return r->view;
    }
    Q_ASSERT(mip < r->mipLevelCount && "ctx.view: mip past the texture's mip count");
    Q_ASSERT(layer < node_layers(r) && "ctx.view: layer past the texture's layer count");
    Q_ASSERT(subres_declared(pass, r, h, mip, layer) && "ctx.view: mip/layer outside every range this pass declared on the resource");
    ViewKey key {
        .format = r->format,
        .dimension = default_view_dim(r, 1), // single-layer subview -> never the 2DArray arm
        .aspect = WGPUTextureAspect_All,
        .baseMipLevel = mip,
        .mipLevelCount = 1,
        .baseArrayLayer = layer,
        .arrayLayerCount = 1,
    };
    return resolve_view(*storage(graph), r, key);
}

WGPUTexture PassContext::texture(TextureHandle h) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return {};
    }
    Q_ASSERT(find_pass_access(pass, h) && "ctx.texture: resource not declared as an access in this pass's setup");
    return r->texture;
}

WGPUBuffer PassContext::buffer(BufferHandle h) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Buffer);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return {};
    }
    Q_ASSERT(find_pass_access(pass, h) && "ctx.buffer: resource not declared as an access in this pass's setup");
    Q_ASSERT(r->buffer && "ctx.buffer(): resource declared but never realized (no live writer)");
    return r->buffer;
}

WGPUBindGroupEntry PassContext::bind(uint32_t binding, ResourceHandle h) const
{
    if (h.kind == ResourceKind::Texture)
        return webgpu::bind(binding, view(TextureHandle { h }));
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    const ResourceAccess* a = find_bind_access(pass, h);
    if (a && a->bufSize)
        return webgpu::bind(binding, buffer(BufferHandle { h }), a->bufOffset, a->bufSize);
    return webgpu::bind(binding, buffer(BufferHandle { h }), 0, r ? r->bufferSize : 0);
}

WGPUBindGroupEntry PassContext::bind(uint32_t binding, TextureHandle h, uint32_t mip, uint32_t layer) const
{
    return webgpu::bind(binding, view(h, mip, layer));
}

WGPUExtent3D PassContext::texture_size(TextureHandle h) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return {};
    }
    return r->resolved;
}

WGPUExtent3D PassContext::texture_size(TextureHandle h, uint32_t mip) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return {};
    }
    Q_ASSERT(mip < (r->mipLevelCount ? r->mipLevelCount : 1) && "ctx.texture_size: mip past the texture's mip count");
    uint32_t d = r->resolved.depthOrArrayLayers ? r->resolved.depthOrArrayLayers : 1;
    if (r->dimension == WGPUTextureDimension_3D)
        d = mip_dim(d, mip); // depth shifts, array layers don't
    return WGPUExtent3D { mip_dim(r->resolved.width, mip), mip_dim(r->resolved.height, mip), d };
}

WGPUTextureFormat PassContext::format(TextureHandle h) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return WGPUTextureFormat_Undefined;
    }
    Q_ASSERT(r->format != WGPUTextureFormat_Undefined && "ctx.format: imported texture without a format; pass it to import_texture");
    return r->format;
}

uint32_t PassContext::mip_count(TextureHandle h) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return 0;
    }
    Q_ASSERT(r->mipLevelCount && "ctx.mip_count: mip count is 0");
    return r->mipLevelCount;
}

uint32_t PassContext::sample_count(TextureHandle h) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return 0;
    }
    Q_ASSERT(r->sampleCount && "ctx.sample_count: sample count is 0");
    return r->sampleCount;
}

uint64_t PassContext::buffer_size(BufferHandle h) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Buffer);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    if (!r) {
        return {};
    }
    return r->bufferSize;
}

PassContext::BufferCopyInfo PassContext::buffer_copy_info(BufferHandle src, BufferHandle dst) const
{
    Q_ASSERT(src.id != 0 && dst.id != 0);
    Q_ASSERT(src.kind == ResourceKind::Buffer && dst.kind == ResourceKind::Buffer && "buffer_copy_info: both handles must be buffers");
    const ResourceAccess* sa = find_copy_access(pass, src, false, "buffer_copy_info: no copy_buffer() with this handle as src declared in this pass");
    const ResourceAccess* da = find_copy_access(pass, dst, true, "buffer_copy_info: no copy_buffer() with this handle as dst declared in this pass");
    ResourceNode* sn = find_node(graph, src);
    ResourceNode* dn = find_node(graph, dst);
    Q_ASSERT(sn && dn && "buffer_copy_info: handle is 0 or from another graph");
    Q_ASSERT(sn->buffer && dn->buffer && "buffer_copy_info: buffer declared but never realized");

    return { sn->buffer, sa->bufOffset, dn->buffer, da->bufOffset, sa->bufSize };
}

static WGPUTexelCopyTextureInfo copy_texture_info(RenderGraph* graph, PassNode* pass, ResourceHandle h, bool wantDst, WGPUOrigin3D origin, WGPUTextureAspect aspect)
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Texture);
    ResourceNode* r = find_node(graph, h);
    Q_ASSERT(r != nullptr && "failed to find node! Handle is 0 or from another graph");
    Q_ASSERT(r->texture
        && "copy_*_info: imported texture has no backing WGPUTexture; pass the WGPUTexture to "
           "import_texture to enable copies, or copy via a render-pass blit instead");
    const ResourceAccess* a = find_copy_access(pass,
        h,
        wantDst,
        wantDst ? "copy_dst_info: no copy with this handle as dst declared in this pass"
                : "copy_src_info: no copy with this handle as src declared in this pass");
    origin.z += a->baseLayer;
    return WGPUTexelCopyTextureInfo { .texture = r->texture, .mipLevel = a->baseMip, .origin = origin, .aspect = aspect };
}

WGPUTexelCopyTextureInfo PassContext::copy_src_info(TextureHandle h, WGPUOrigin3D origin, WGPUTextureAspect aspect) const
{
    return copy_texture_info(graph, pass, h, false, origin, aspect);
}

WGPUTexelCopyTextureInfo PassContext::copy_dst_info(TextureHandle h, WGPUOrigin3D origin, WGPUTextureAspect aspect) const
{
    return copy_texture_info(graph, pass, h, true, origin, aspect);
}

WGPUExtent3D PassContext::copy_extent_src(TextureHandle h) const
{
    return copy_extent_for(graph, pass, h, false);
}

WGPUExtent3D PassContext::copy_extent_dst(TextureHandle h) const
{
    return copy_extent_for(graph, pass, h, true);
}

WGPUTexelCopyBufferInfo PassContext::copy_src_buffer(BufferHandle h, WGPUTexelCopyBufferLayout layout) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Buffer);
    find_copy_access(pass, h, /*wantDst=*/false, "copy_src_buffer: no copy with this handle as src declared in this pass");
    return WGPUTexelCopyBufferInfo { .layout = layout, .buffer = buffer(h) };
}

WGPUTexelCopyBufferInfo PassContext::copy_dst_buffer(BufferHandle h, WGPUTexelCopyBufferLayout layout) const
{
    Q_ASSERT(h.id != 0);
    Q_ASSERT(h.kind == ResourceKind::Buffer);
    find_copy_access(pass, h, /*wantDst=*/true, "copy_dst_buffer: no copy with this handle as dst declared in this pass");
    return WGPUTexelCopyBufferInfo { .layout = layout, .buffer = buffer(h) };
}

static void release_resources(RenderGraph* rg);

static constexpr bool is_mappable(WGPUBufferUsage usage)
{
    return (usage & (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite)) != 0;
}

// create the GPU resources compile() worked out
static void realize_graph(RenderGraph* rg, WGPUDevice device)
{
    auto t0 = std::chrono::steady_clock::now();
    RenderGraphStorage& s = *storage(rg);
    if (s.m_state == RenderGraphState::Failed)
        return;
    if (s.m_state != RenderGraphState::Compiled) {
        push_error(s, "realize() reached a graph that was never compiled. This is an internal error, not an authoring one.");
        return;
    }

    // inferred transient attachments get the memoryless usage bit only where the platform supports it
#ifdef __EMSCRIPTEN__
    const bool transientOK = rg_web_has_transient_attachment() != 0;
#else
    const bool transientOK = wgpuDeviceHasFeature(device, kTransientAttachmentsFeature);
#endif
    s.m_allocator->transientFeatureOn = transientOK;

    PersistentResourcePool& pool = s.m_allocator->pool;
    TransientResourcePool& transient = s.m_allocator->transient;

    auto pool_used = [](const ResourceNode* r) { return r->kind == ResourceKind::Texture ? r->texUsage != 0 : r->bufUsage != 0; };

    // touch + union usage into the entry. curr and prev must be co-used, or the rotation would surface a
    // never-written slot. compile() already rejects that asymmetry.
    for (ResourceNode* r = s.m_resouces; r; r = r->next) {
        if (!r->persistent || !pool_used(r))
            continue;
        PersistentResourcePool::Entry* e = pool.touch(r->id, r->history ? PersistentResourcePool::kLayers : 1, r->historyHash, r->kind);
        if (r->kind == ResourceKind::Texture)
            e->usage |= r->texUsage;
        else
            e->bufUsage |= r->bufUsage;
    }

    // realize + point each layer node at its rotated slot
    for (ResourceNode* r = s.m_resouces; r; r = r->next) {
        if (!r->persistent || !pool_used(r))
            continue;
        PersistentResourcePool::Entry* e = pool.find(r->id); // touched above -> non-null
        if (!e) {
            push_error(s,
                "persistent resource \"%.*s\" is used but has no pool entry from the touch pass. "
                "This is an internal error, not an authoring one.",
                RG_NAME(r->id));
            return;
        }
        // preconditions realize_*_entry relies on and cannot report itself: usage only ever grows, and a
        // recreate never lands twice in one frame.
        const bool usageGrewOnly = r->kind == ResourceKind::Texture ? (e->usageAtCreate & ~e->usage) == 0 : (e->bufUsageAtCreate & ~e->bufUsage) == 0;
        const bool wouldRecreate = r->kind == ResourceKind::Texture
            ? PersistentResourcePool::tex_entry_would_recreate(*e, tex_signature(r), e->usage)
            : PersistentResourcePool::buf_entry_would_recreate(*e, r->bufferSize, e->bufUsage);
        if (!usageGrewOnly || (wouldRecreate && e->createdClock == pool.evictClock)) {
            push_error(s,
                "persistent resource \"%.*s\": %s. This is an internal error, not an authoring one.",
                RG_NAME(r->id),
                !usageGrewOnly ? "its pooled usage shrank since the entry was created"
                               : "its layer nodes disagree on the descriptor, so the pool entry would be recreated twice in one frame");
            return;
        }
        uint32_t sl = pool.slot(*e, r->historyIndex);
        if (r->kind == ResourceKind::Texture) {
            pool.realize_texture_entry(e, device, tex_signature(r));
            r->texture = e->tex[sl];
            r->view = e->view[sl];
        } else {
            pool.realize_buffer_entry(e, device, r->bufferSize);
            r->buffer = e->buf[sl];
        }
        // .prev holds valid history only if the entry survived from a prior frame, was not recreated this
        // frame, and was used last frame.
        r->historyValid = r->history && (e->createdClock != pool.evictClock) && (e->prevTouched == pool.evictClock - 1);
    }

    // aliasing (compile() phase 4): one acquire per slot with the union usage, then point every member at
    // it. per slot, not per member, keeps the claim count right. m_slotCount is 0 when aliasing is off,
    // leaving the loops below to realize each transient alone.
    for (uint32_t i = 0; i < s.m_slotCount; ++i) {
        PhysicalResource& ph = s.m_slots[i];
        if (ph.kind == ResourceKind::Texture) {
            transient.acquire(device, ph.sig, ph.texUsage, ph.identity, ph.texture, ph.view);
            continue;
        }
        if (is_mappable(ph.bufUsage)) {
            push_error(s,
                "alias slot %u asks for a mappable transient buffer; transient buffers are never mappable, "
                "import a caller-owned buffer for readback or staging. This is an internal error, not an "
                "authoring one.",
                i);
            return;
        }
        transient.acquire(device, ph.bufferSize, ph.bufUsage, ph.identity, ph.buffer);
    }
    for (ResourceNode* r = s.m_resouces; r; r = r->next)
        if (r->aliasSlot != ResourceNode::kNoSlot) {
            PhysicalResource& ph = s.m_slots[r->aliasSlot];
            r->texture = ph.texture;
            r->view = ph.view;
            r->buffer = ph.buffer;
        }

    for (ResourceNode* r = s.m_resouces; r; r = r->next) {
        // graph-owned per-frame TEXTURES not on an alias slot: reuse a pooled texture matching this
        // descriptor. release_resources() leaves it alone, the pool evicts at end_frame().
        if (r->is_external() || r->kind != ResourceKind::Texture || !r->texUsage || r->aliasSlot != ResourceNode::kNoSlot)
            continue;
        // memoryless hint for an inferred transient attachment. the pool keys on usage, so these never
        // collide with ordinary render targets.
        WGPUTextureUsage usage = r->texUsage;
        if (transientOK && r->transientAttachment)
            usage |= WGPUTextureUsage_TransientAttachment;
        transient.acquire(device, tex_signature(r), usage, r->id.name.data, r->texture, r->view);
    }
    for (ResourceNode* r = s.m_resouces; r; r = r->next) {
        // graph-owned per-frame BUFFERS not on an alias slot.
        if (r->is_external() || r->kind != ResourceKind::Buffer || !r->bufUsage || r->aliasSlot != ResourceNode::kNoSlot)
            continue;
        if (is_mappable(r->bufUsage)) {
            push_error(s,
                "transient buffer \"%.*s\" asks for a mappable usage; transient buffers are never mappable, "
                "import a caller-owned buffer for readback or staging. This is an internal error, not an "
                "authoring one.",
                RG_NAME(r->id));
            return;
        }
        transient.acquire(device, r->bufferSize, r->bufUsage, r->id.name.data, r->buffer);
    }

    // point each texture node at the subview cache of its realized physical texture, so ctx.view() reuses
    // subresource views across frames. by handle, and after every acquire, so it cannot dangle.
    for (ResourceNode* r = s.m_resouces; r; r = r->next)
        if (r->kind == ResourceKind::Texture && r->texture)
            r->subviews = find_subview_list(s.m_allocator, r->texture);

    auto t1 = std::chrono::steady_clock::now();
    s.timing_realize_us = std::chrono::duration<float, std::micro>(t1 - t0).count();
}

void RenderGraph::execute(WGPUDevice device, WGPUQueue queue, WGPUCommandEncoder encoder, bool enableProfiling)
{
    RenderGraphStorage& s = *storage(this);
    if (s.m_state != RenderGraphState::Compiled) {
        Q_ASSERT(s.m_state == RenderGraphState::Failed && "execute(): graph not compiled; compile() missing, or execute() called twice");
        return;
    }

    ScopedScratch scratch(s.m_allocator->scratch);
    s.viewScratchCap = 8;
    s.viewScratch = scratch.alloc<WGPUTextureView>(s.viewScratchCap);

    realize_graph(this, device);
    if (s.m_state != RenderGraphState::Compiled)
        return; // realize failed: the pass bodies would encode against unrealized resources

    auto t0 = std::chrono::steady_clock::now();

    // opt-in GPU timing: lazily build the query set and buffers, then grab a free read-back ring slot. with
    // every slot awaiting its map callback, skip profiling this frame rather than stall.
    GpuProfiler& prof = s.m_allocator->profiler;
    bool profiling = enableProfiling;
    int slotIdx = -1;
    // one pendingSlot per allocator, so only one profiled graph per frame. run this one unprofiled rather
    // than clobber an earlier graph's results.
    if (profiling && prof.pendingSlot != -1) {
        qDebug("[RenderGraph] profiling skipped for this graph: an earlier graph this frame is already profiled\n");
        profiling = false;
    }
    if (profiling) {
        prof.init(device);
        // disable profiling when no free slot is found
        slotIdx = prof.free_slot();
        if (slotIdx < 0)
            profiling = false;
    }
    GpuProfiler::Slot* slot = profiling ? &prof.ring[slotIdx] : nullptr;
    uint32_t pi = 0; // begin/end pair index, advanced only for passes that run, keeps the resolve range contiguous

    // bracket each contiguous run of same-prefix passes in an encoder debug group, so a RenderDoc/PIX
    // capture shows collapsible regions. push/pop happen between passes, so they are balanced.
    WGPUStringView openGroup {};
    for (PassNode* p = s.m_passes; p; p = p->next) {
        Q_ASSERT(p->kind != PassKind::None && "a pass of kind None is not allowed");
        WGPUStringView grp = group_prefix(p->id.name);
        if (!sv_eq(grp, openGroup)) {
            if (sv_length(openGroup))
                wgpuCommandEncoderPopDebugGroup(encoder);
            if (sv_length(grp))
                wgpuCommandEncoderPushDebugGroup(encoder, grp);
            openGroup = grp;
        }

        // an initialize() pass that survived the cull bakes this frame: stamp each target baked, so next
        // frame's compile() skips it. recorded here, not in compile(), so no frame claims a bake it never ran.
        if (p->initCount && p->exec_fn)
            for (uint32_t i = 0; i < p->initCount; ++i)
                if (ResourceNode* t = find_node(this, p->initTargets[i].target))
                    if (PersistentResourcePool::Entry* e = s.m_allocator->pool.find(t->id)) {
                        e->initHash = p->initTargets[i].hash;
                        e->baked = true;
                    }

        PassContext ctx {};
        ctx.encoder = encoder;
        ctx.graph = this;
        ctx.queue = queue;
        ctx.device = device;
        ctx.pass = p;
        s.viewScratchN = 0; // per-pass: attachment + body ctx.view() views accumulate here, freed after the body

        // time this pass iff profiling is live, the pass records a body, and the set has a free pair.
        // Transfer passes need RG_TIME_TRANSFER_PASSES, they use off-spec encoder timestamps.
#ifdef RG_TIME_TRANSFER_PASSES
        constexpr bool kTimeTransfer = true;
#else
        constexpr bool kTimeTransfer = false;
#endif
        const bool timeThis = profiling && p->exec_fn && pi < GpuProfiler::kMaxPasses && (kTimeTransfer || p->kind != PassKind::Transfer);
        WGPUPassTimestampWrites tw { .querySet = prof.querySet, .beginningOfPassWriteIndex = 2 * pi, .endOfPassWriteIndex = 2 * pi + 1 };
        if (timeThis)
            slot->names[pi] = p->id.name; // interned view, stable until the read-back lands frames later

        if (p->kind == PassKind::Compute && p->exec_fn) {
            WGPUComputePassDescriptor cd { .label = p->id.name, .timestampWrites = timeThis ? &tw : nullptr };
            ctx.compute_pass = wgpuCommandEncoderBeginComputePass(encoder, &cd);
            p->exec_fn(p->exec_obj, ctx);
            wgpuComputePassEncoderEnd(ctx.compute_pass);
            wgpuComputePassEncoderRelease(ctx.compute_pass);
        } else if (p->kind == PassKind::Graphics) {
            // no exec_fn guard: a body-less graphics pass still begins and ends, so its clear loadOps run
            // gather declared attachments from the access list -> WebGPU render pass descriptor
            WGPURenderPassColorAttachment color[kMaxColorAttachments] {};
            // color() slots may be sparse, so pre-seed every entry as a valid null attachment. depthSlice
            // is the one field whose null value is not 0.
            for (uint32_t i = 0; i < kMaxColorAttachments; ++i)
                color[i].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
            uint32_t maxColorSlot = 0; // highest explicit slot seen -> colorAttachmentCount = maxColorSlot + 1
            bool anyColor = false;
            WGPURenderPassDepthStencilAttachment depth {};
            bool hasDepth = false;

            // an attachment view is exactly one subresource, a render target cannot span a mip chain or an
            // array. attachment accesses carry no ViewRange, so this is the (baseMip, baseLayer) slice.
            auto attach_view = [&](ResourceNode* r, const ResourceAccess& a) -> WGPUTextureView {
                Q_ASSERT(a.baseMip < r->mipLevelCount && "attachment baseMip past the texture's mip count");
                Q_ASSERT((!r->texture || a.baseLayer < node_layers(r)) && "attachment baseLayer past the texture's layer count");
                return resolve_view(s, r, view_signature_for(r, a));
            };

            for (uint32_t i = 0; i < p->accessCount; ++i) {
                const ResourceAccess& a = p->accesses[i];
                ResourceNode* r = find_node(this, a.handle);
                if (!r)
                    continue;
                if (a.type == AccessType::ColorAttachment && a.colorIndex < kMaxColorAttachments) { // color() asserted range; release backstop
                    color[a.colorIndex] = WGPURenderPassColorAttachment {
                        .view = attach_view(r, a),
                        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                        .resolveTarget = nullptr, // patched below if a resolve() in this pass targets this slot
                        .loadOp = a.loadOp,
                        .storeOp = a.storeOp,
                        .clearValue = WGPUColor { a.clearColor[0], a.clearColor[1], a.clearColor[2], a.clearColor[3] }, // f32 -> WGPUColor(double)
                    };
                    if (!anyColor || a.colorIndex > maxColorSlot)
                        maxColorSlot = a.colorIndex;
                    anyColor = true;
                } else if (a.type == AccessType::ResolveAttachment && a.colorIndex < kMaxColorAttachments) {
                    // resolve() stamped the color slot it targets onto colorIndex, so patch that slot.
                    color[a.colorIndex].resolveTarget = attach_view(r, a);
                } else if (a.type == AccessType::DepthStencilAttachment || a.type == AccessType::DepthStencilReadOnly) {
                    depth = WGPURenderPassDepthStencilAttachment {
                        .view = attach_view(r, a),
                        .depthLoadOp = a.loadOp,
                        .depthStoreOp = a.storeOp,
                        .depthClearValue = a.clearDepth,
                        .depthReadOnly = a.type == AccessType::DepthStencilReadOnly,
                        .stencilLoadOp = a.stencilLoadOp,
                        .stencilStoreOp = a.stencilStoreOp,
                        .stencilClearValue = a.stencilClear,
                        .stencilReadOnly = a.type == AccessType::DepthStencilReadOnly,
                    };
                    hasDepth = true;
                }
            }

            WGPURenderPassDescriptor rd {
                .label = p->id.name,
                .colorAttachmentCount = anyColor ? maxColorSlot + 1 : 0,
                .colorAttachments = color,
                .depthStencilAttachment = hasDepth ? &depth : nullptr,
                .timestampWrites = timeThis ? &tw : nullptr,
            };
            ctx.render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &rd);
            if (p->exec_fn)
                p->exec_fn(p->exec_obj, ctx);
            wgpuRenderPassEncoderEnd(ctx.render_pass);
            wgpuRenderPassEncoderRelease(ctx.render_pass);
        } else { // Transfer: body records straight onto the encoder. no pass object -> bracket with encoder timestamps.
            if (p->exec_fn) {
#ifdef RG_TIME_TRANSFER_PASSES // off-spec: the device must be created with the allow_unsafe_apis toggle
                if (timeThis)
                    wgpuCommandEncoderWriteTimestamp(encoder, prof.querySet, 2 * pi);
#endif
                p->exec_fn(p->exec_obj, ctx);
#ifdef RG_TIME_TRANSFER_PASSES
                if (timeThis)
                    wgpuCommandEncoderWriteTimestamp(encoder, prof.querySet, 2 * pi + 1);
#endif
            }
        }

        if (timeThis)
            ++pi;

        // free the pass-scoped subresource views, attachments and ctx.view() alike
        for (uint32_t i = 0; i < s.viewScratchN; ++i)
            wgpuTextureViewRelease(s.viewScratch[i]);
    }
    if (sv_length(openGroup))
        wgpuCommandEncoderPopDebugGroup(encoder); // close the last open group

    // resolve the queries we wrote into the staging buffer, then stage them into the chosen ring slot. the
    // copy rides this frame's submit, which collect_gpu_timings() maps once queued.
    if (profiling && pi > 0) {
        wgpuCommandEncoderResolveQuerySet(encoder, prof.querySet, 0, 2 * pi, prof.resolveBuf, 0);
        wgpuCommandEncoderCopyBufferToBuffer(encoder, prof.resolveBuf, 0, slot->buf, 0, uint64_t(pi) * 2 * sizeof(uint64_t));
        slot->count = pi;
        prof.pendingSlot = slotIdx;
    }

    s.m_state = RenderGraphState::Finished;
    release_resources(this);
    auto t1 = std::chrono::steady_clock::now();
    s.timing_execute_us = std::chrono::duration<float, std::micro>(t1 - t0).count();
}

// kick the async read-back of the slot execute() just filled. after the queue submit, the copy must be
// queued first. the callback lands a couple of frames on, during an instance event pump.
void RenderGraph::collect_gpu_timings()
{
    GpuProfiler& prof = storage(this)->m_allocator->profiler;
    if (prof.pendingSlot < 0)
        return;
    GpuProfiler::Slot* slot = &prof.ring[prof.pendingSlot];
    prof.pendingSlot = -1;
    slot->pending = true;

    WGPUBufferMapCallbackInfo cb {};
    cb.mode = WGPUCallbackMode_AllowSpontaneous;
    cb.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* u1, void* u2) {
        auto* slot = static_cast<GpuProfiler::Slot*>(u1);
        auto* prof = static_cast<GpuProfiler*>(u2);
        if (status == WGPUMapAsyncStatus_Success) {
            const auto* t = static_cast<const uint64_t*>(wgpuBufferGetConstMappedRange(slot->buf, 0, slot->count * 2 * sizeof(uint64_t)));
            if (t) {
                prof->resultCount = slot->count;
                for (uint32_t i = 0; i < slot->count; ++i) {
                    const uint64_t b = t[2 * i], e = t[2 * i + 1];
                    prof->resultUs[i] = (e > b) ? float((e - b) / 1000.0) : 0.0f; // ns -> us; clamp reorders
                    prof->resultNames[i] = slot->names[i]; // interned, so it outlives the slot's reuse
                }
                ++prof->resultId; // signal the history sampler that a fresh read-back landed
            }
            wgpuBufferUnmap(slot->buf);
        }
        slot->pending = false;
    };
    cb.userdata1 = slot;
    cb.userdata2 = &prof;
    wgpuBufferMapAsync(slot->buf, WGPUMapMode_Read, 0, slot->count * 2 * sizeof(uint64_t), cb);
}

ErrorMessage* RenderGraph::get_errors() const
{
    RenderGraphStorage& s = *storage(const_cast<RenderGraph*>(this));
    Q_ASSERT(s.frameId == s.m_allocator->frameId && "get_errors(): graph outlived its frame, read errors before the next begin_frame()");
    return s.m_errors;
}

static void release_resources(RenderGraph* rg)
{
    RenderGraphStorage& s = *storage(rg);
    Q_ASSERT(s.m_state == RenderGraphState::Finished);
    for (ResourceNode* r = s.m_resouces; r; r = r->next) {
        if (r->is_external())
            continue;
        r->texture = nullptr;
        r->view = nullptr;
        r->buffer = nullptr;
    }

    for (uint32_t i = 0; i < s.m_slotCount; ++i) {
        s.m_slots[i].texture = nullptr;
        s.m_slots[i].view = nullptr;
        s.m_slots[i].buffer = nullptr;
    }

    s.m_allocator->transient.release_claims();
}

// which resource kind an access is legal
static constexpr bool access_allows_kind(AccessType t, ResourceKind k)
{
    switch (t) {
    case AccessType::ColorAttachment:
    case AccessType::DepthStencilAttachment:
    case AccessType::DepthStencilReadOnly:
    case AccessType::ResolveAttachment:
    case AccessType::Sampled:
        return k == ResourceKind::Texture;
    case AccessType::Uniform:
    case AccessType::Vertex:
    case AccessType::Index:
    case AccessType::Indirect:
        return k == ResourceKind::Buffer;
    default: // StorageRead/Write, CopySrc/Dst: both kinds are legal
        return true;
    }
}

static void use(PassNode* pass, RenderGraph* graph,
    ResourceHandle handle,
    AccessType type,
    WGPULoadOp load = WGPULoadOp_Undefined,
    WGPUStoreOp store = WGPUStoreOp_Undefined,
    WGPUColor clear = {},
    float clearDepth = {},
    uint32_t baseMip = 0,
    uint32_t baseLayer = 0,
    ViewRange range = {},
    WGPULoadOp stencilLoad = WGPULoadOp_Undefined,
    WGPUStoreOp stencilStore = WGPUStoreOp_Undefined,
    uint32_t stencilClear = 0)
{
    Q_ASSERT(handle.id != 0);
    Q_ASSERT(handle.id < storage(graph)->next_resource_id && "stale or foreign ResourceHandle; not created by this frame's graph");
    Q_ASSERT(handle.generation == storage(graph)->generation && "stale/foreign ResourceHandle: not created by this graph");
    Q_ASSERT(access_allows_kind(type, handle.kind) && "handle kind disagrees with this access; a handle was converted across kinds");

    // WebGPU permits several writable-storage uses in one scope. the graph is stricter, it cannot
    // synchronize two writes inside a pass. Transfer passes are exempt, each copy is its own usage scope.
    const bool w = access_is_write(type);
    if (pass->kind != PassKind::Transfer)
        for (uint32_t i = 0; i < pass->accessCount; ++i) {
            if (pass->accesses[i].handle.id != handle.id)
                continue;
            if (in_pass_accesses_conflict(type,
                    baseMip,
                    range.mipCount,
                    baseLayer,
                    range.layerCount,
                    range.aspect,
                    pass->accesses[i].type,
                    pass->accesses[i].baseMip,
                    pass->accesses[i].mipCount,
                    pass->accesses[i].baseLayer,
                    pass->accesses[i].layerCount,
                    pass->accesses[i].viewAspect)) {
                qDebug("[RenderGraph] error: pass \"%.*s\" uses resource id %u %s in one pass; a "
                            "written resource must be its only use in the pass.\n",
                    RG_NAME(pass->id),
                    handle.id,
                    (w && access_is_write(pass->accesses[i].type)) ? "as more than one write (unsynchronized)" : "as both written and read");
                Q_ASSERT(false && "RenderGraph: illegal in-pass resource usage (read+write or double write in one pass)");
            }
        }

    if (pass->accessCount == pass->accessCap) {
        GraphAllocator* A = storage(graph)->m_allocator;
        uint32_t oldCap = pass->accessCap;
        uint32_t newCap = oldCap ? oldCap * 2 : 4;
        ResourceAccess* buf = nullptr;
        if (oldCap)
            buf = static_cast<ResourceAccess*>(
                A->front.extend_tail(pass->accesses, (size_t)oldCap * sizeof(ResourceAccess), (size_t)(newCap - oldCap) * sizeof(ResourceAccess)));
        if (!buf) {
            buf = A->alloc<ResourceAccess>(newCap);
            if (pass->accessCount)
                std::memcpy(buf, pass->accesses, (size_t)pass->accessCount * sizeof(ResourceAccess));
        }
        pass->accesses = buf;
        pass->accessCap = newCap;
    }

    pass->accesses[pass->accessCount++] = ResourceAccess {
        .handle = handle,
        .clearColor = { float(clear.r), float(clear.g), float(clear.b), float(clear.a) },
        .clearDepth = clearDepth,
        .loadOp = load,
        .storeOp = store,
        .stencilLoadOp = stencilLoad,
        .stencilStoreOp = stencilStore,
        .viewDim = range.dim,
        .viewAspect = range.aspect,
        .baseLayer = uint16_t(baseLayer),
        .layerCount = uint16_t(range.layerCount),
        .type = type,
        .stencilClear = uint8_t(stencilClear),
        .baseMip = uint8_t(baseMip),
        .mipCount = uint8_t(range.mipCount),
    };
}

void PassBuilder::color(TextureHandle handle, uint32_t attachmentIndex, ColorAttachment a)
{
    const WGPULoadOp load = a.load;
    const WGPUStoreOp store = a.store;
    const WGPUColor clear = a.clear;
    const uint32_t baseMip = a.sub.mip;
    const uint32_t baseLayer = a.sub.layer;

    Q_ASSERT(handle.id != 0);
    Q_ASSERT(m_pass->kind == PassKind::Graphics && "color attachment is only legal for graphics passes.");

    if (attachmentIndex >= kMaxColorAttachments) {
        qDebug("[RenderGraph] error: pass \"%.*s\" declares color attachment slot %u >= %u "
                    "(WebGPU maxColorAttachments); attachment on resource id %u dropped at execute.\n",
            RG_NAME(m_pass->id),
            attachmentIndex,
            kMaxColorAttachments,
            handle.id);
        Q_ASSERT(false && "RenderGraph: color() attachmentIndex out of range");
        return;
    }
    // a slot binds exactly one resource per pass.
    for (uint32_t i = 0; i < m_pass->accessCount; ++i)
        if (m_pass->accesses[i].type == AccessType::ColorAttachment && m_pass->accesses[i].colorIndex == attachmentIndex) {
            qDebug("[RenderGraph] error: pass \"%.*s\" declares color attachment slot %u twice; "
                        "each slot binds one resource.\n",
                RG_NAME(m_pass->id),
                attachmentIndex);
            Q_ASSERT(false && "RenderGraph: duplicate color() attachmentIndex in one pass");
            return;
        }

    use(m_pass, m_graph, handle, AccessType::ColorAttachment, load, store, clear, {}, baseMip, baseLayer);
    m_pass->accesses[m_pass->accessCount - 1].colorIndex = uint8_t(attachmentIndex);
}

void PassBuilder::resolve(TextureHandle src, TextureHandle target, Subresource sub)
{
    const uint32_t baseMip = sub.mip;
    const uint32_t baseLayer = sub.layer;

    Q_ASSERT(src.id != 0);
    Q_ASSERT(target.id != 0);
    Q_ASSERT(m_pass->kind == PassKind::Graphics && "resolve target is only legal for graphics passes.");

    uint8_t srcSlot = 0;
    bool foundSrc = false;
    for (uint32_t i = 0; i < m_pass->accessCount; ++i)
        if (m_pass->accesses[i].type == AccessType::ColorAttachment && m_pass->accesses[i].handle.id == src.id) {
            srcSlot = m_pass->accesses[i].colorIndex;
            foundSrc = true;
            break;
        }
    if (!foundSrc) {
        qDebug("[RenderGraph] error: pass \"%.*s\" calls resolve() whose src (resource id %u) is not "
                    "a color attachment declared in this pass; declare its color() first.\n",
            RG_NAME(m_pass->id),
            src.id);
        Q_ASSERT(false && "RenderGraph: resolve() src is not a color attachment in this pass");
        return;
    }
    // one resolve target per color slot.
    for (uint32_t i = 0; i < m_pass->accessCount; ++i)
        if (m_pass->accesses[i].type == AccessType::ResolveAttachment && m_pass->accesses[i].colorIndex == srcSlot) {
            qDebug("[RenderGraph] error: pass \"%.*s\" declares two resolve targets for color slot %u; "
                        "one resolve target per color attachment.\n",
                RG_NAME(m_pass->id),
                srcSlot);
            Q_ASSERT(false && "RenderGraph: two resolve() targets for one color slot in a pass");
            return;
        }

    use(m_pass, m_graph, target, AccessType::ResolveAttachment, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, baseMip, baseLayer);
    m_pass->accesses[m_pass->accessCount - 1].colorIndex = srcSlot;
}

static void assert_single_depth(PassNode* pass)
{
    for (uint32_t i = 0; i < pass->accessCount; ++i) {
        AccessType t = pass->accesses[i].type;
        if (t == AccessType::DepthStencilAttachment || t == AccessType::DepthStencilReadOnly) {
            qDebug("[RenderGraph] error: pass \"%.*s\" declares a second depth-stencil attachment; "
                        "a render pass has one depth-stencil slot, so only the last would take effect.\n",
                RG_NAME(pass->id));
            Q_ASSERT(false && "RenderGraph: two depth-stencil attachments in one pass");
        }
    }
}

void PassBuilder::depth_stencil(TextureHandle handle, DepthStencilAttachment a)
{
    const WGPULoadOp load = a.load;
    const WGPUStoreOp store = a.store;
    const float clearDepth = a.clearDepth;
    const uint32_t baseMip = a.sub.mip;
    const uint32_t baseLayer = a.sub.layer;
    const WGPULoadOp stencilLoad = a.stencilLoad;
    const WGPUStoreOp stencilStore = a.stencilStore;
    const uint32_t stencilClear = a.stencilClear;

    Q_ASSERT(handle.id != 0);
    Q_ASSERT(m_pass->kind == PassKind::Graphics && "depth-stencil attachment is only legal for graphics passes.");
    assert_single_depth(m_pass);
    use(m_pass, m_graph, handle, AccessType::DepthStencilAttachment, load, store, {}, clearDepth, baseMip, baseLayer, {}, stencilLoad, stencilStore, stencilClear);
}

void PassBuilder::depth_stencil_read_only(TextureHandle handle, Subresource sub)
{
    const uint32_t baseMip = sub.mip;
    const uint32_t baseLayer = sub.layer;

    Q_ASSERT(handle.id != 0);
    Q_ASSERT(m_pass->kind == PassKind::Graphics && "depth-stencil attachment is only legal for graphics passes.");
    assert_single_depth(m_pass);
    use(m_pass, m_graph, handle, AccessType::DepthStencilReadOnly, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, baseMip, baseLayer);
}

void PassBuilder::sampled(TextureHandle handle, ViewRange range)
{
    const uint32_t baseMip = range.baseMip;
    const uint32_t baseLayer = range.baseLayer;

    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::Sampled, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, baseMip, baseLayer, range);
}

void PassBuilder::storage_read(TextureHandle handle, ViewRange range)
{
    const uint32_t baseMip = range.baseMip;
    const uint32_t baseLayer = range.baseLayer;

    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::StorageRead, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, baseMip, baseLayer, range);
}

void PassBuilder::storage_write(TextureHandle handle, ViewRange range)
{
    const uint32_t baseMip = range.baseMip;
    const uint32_t baseLayer = range.baseLayer;

    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::StorageWrite, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, baseMip, baseLayer, range);
}

void PassBuilder::storage_read_write(TextureHandle handle, ViewRange range)
{
    Q_ASSERT(handle.id != 0);
    storage_read(handle, range);
    storage_write(handle, range);
}

// records a declared bind range on the access use() just appended. ONLY USE after calling use()
static void set_bind_range(RenderGraph* graph, PassNode* pass, BufferHandle handle, BufferRange range)
{
    if (!range.offset && !range.size)
        return;
    ResourceNode* n = find_node(graph, handle);
    Q_ASSERT(n && "bind range: handle is 0 or from another graph");
    const uint64_t sz = range.size ? range.size : (n->bufferSize > range.offset ? n->bufferSize - range.offset : 0);
    pass->accesses[pass->accessCount - 1].bufOffset = range.offset;
    pass->accesses[pass->accessCount - 1].bufSize = sz;
}

void PassBuilder::storage_read(BufferHandle handle, BufferRange range)
{
    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::StorageRead);
    set_bind_range(m_graph, m_pass, handle, range);
}

void PassBuilder::storage_write(BufferHandle handle, BufferRange range)
{
    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::StorageWrite);
    set_bind_range(m_graph, m_pass, handle, range);
}

void PassBuilder::storage_read_write(BufferHandle handle, BufferRange range)
{
    Q_ASSERT(handle.id != 0);
    storage_read(handle, range);
    storage_write(handle, range);
}

void PassBuilder::uniform(BufferHandle handle, BufferRange range)
{
    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::Uniform);
    set_bind_range(m_graph, m_pass, handle, range);
}

void PassBuilder::host_write(BufferHandle handle)
{
    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::CopyDst);
}

void PassBuilder::copy_texture(TextureHandle src, TextureHandle dst, Subresource srcSub, Subresource dstSub)
{
    const uint32_t srcMip = srcSub.mip, srcLayer = srcSub.layer;
    const uint32_t dstMip = dstSub.mip, dstLayer = dstSub.layer;

    Q_ASSERT(src.id != 0 && dst.id != 0);
    Q_ASSERT(m_pass->kind == PassKind::Transfer && "copy_texture() is only legal in Transfer passes");
    Q_ASSERT(!(src.id == dst.id && srcMip == dstMip && srcLayer == dstLayer) && "copy_texture: src and dst are the same subresource");
    use(m_pass, m_graph, src, AccessType::CopySrc, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, srcMip, srcLayer);
    use(m_pass, m_graph, dst, AccessType::CopyDst, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, dstMip, dstLayer);
}

void PassBuilder::copy_texture_to_buffer(TextureHandle src, BufferHandle dst, Subresource srcSub)
{
    const uint32_t srcMip = srcSub.mip, srcLayer = srcSub.layer;

    Q_ASSERT(src.id != 0 && dst.id != 0);
    Q_ASSERT(m_pass->kind == PassKind::Transfer && "copy_texture_to_buffer() is only legal in Transfer passes");
    use(m_pass, m_graph, src, AccessType::CopySrc, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, srcMip, srcLayer);
    use(m_pass, m_graph, dst, AccessType::CopyDst);
}

void PassBuilder::copy_buffer_to_texture(BufferHandle src, TextureHandle dst, Subresource dstSub)
{
    const uint32_t dstMip = dstSub.mip, dstLayer = dstSub.layer;

    Q_ASSERT(src.id != 0 && dst.id != 0);
    Q_ASSERT(m_pass->kind == PassKind::Transfer && "copy_buffer_to_texture() is only legal in Transfer passes");
    use(m_pass, m_graph, src, AccessType::CopySrc);
    use(m_pass, m_graph, dst, AccessType::CopyDst, WGPULoadOp_Undefined, WGPUStoreOp_Undefined, {}, {}, dstMip, dstLayer);
}

void PassBuilder::copy_buffer(BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
    Q_ASSERT(src.id != 0 && dst.id != 0);
    Q_ASSERT(m_pass->kind == PassKind::Transfer && "copy_buffer() is only legal in Transfer passes");
    Q_ASSERT(src.id != dst.id && "copy_buffer: WebGPU forbids a same-buffer copyBufferToBuffer");
    Q_ASSERT((srcOffset % 4 == 0 && dstOffset % 4 == 0 && size % 4 == 0) && "copy_buffer: offsets and size must be multiples of 4 (WebGPU)");
    ResourceNode* sn = find_node(m_graph, src);
    ResourceNode* dn = find_node(m_graph, dst);
    Q_ASSERT(sn && dn && "copy_buffer: handle is 0 or from another graph");

    const uint64_t n = size ? size : (sn->bufferSize > srcOffset ? sn->bufferSize - srcOffset : 0);
    use(m_pass, m_graph, src, AccessType::CopySrc);
    m_pass->accesses[m_pass->accessCount - 1].bufOffset = srcOffset;
    m_pass->accesses[m_pass->accessCount - 1].bufSize = n;
    use(m_pass, m_graph, dst, AccessType::CopyDst);
    m_pass->accesses[m_pass->accessCount - 1].bufOffset = dstOffset;
    m_pass->accesses[m_pass->accessCount - 1].bufSize = n;
    // full define for aliasing only when this copy provably overwrites every dst byte
    m_pass->accesses[m_pass->accessCount - 1].bufFullDefine = (dstOffset == 0 && n == dn->bufferSize);
}

void PassBuilder::vertex_buffer(BufferHandle handle)
{
    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::Vertex);
}

void PassBuilder::index_buffer(BufferHandle handle)
{
    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::Index);
}

void PassBuilder::indirect_buffer(BufferHandle handle)
{
    Q_ASSERT(handle.id != 0);
    use(m_pass, m_graph, handle, AccessType::Indirect);
}

void PassBuilder::initialize(ResourceHandle target, uint64_t hash)
{
    Q_ASSERT(target.id != 0);
    Q_ASSERT(target.id < storage(m_graph)->next_resource_id && "stale or foreign ResourceHandle; not created by this frame's graph");
    if (target.id == 0)
        return;
    for (uint32_t i = 0; i < m_pass->initCount; ++i)
        Q_ASSERT(m_pass->initTargets[i].target.id != target.id && "initialize() called twice for the same target in one pass");
    if (m_pass->initCount >= PassNode::kMaxInitTargets) {
        qDebug("[RenderGraph] error: pass \"%.*s\" declares more than %u initialize() targets. target on resource id %u dropped.",
            RG_NAME(m_pass->id),
            PassNode::kMaxInitTargets,
            target.id);
        Q_ASSERT(false && "RenderGraph: more than kMaxInitTargets initialize() targets in one pass");
        return;
    }

    if (!m_pass->initTargets) {
        GraphAllocator* A = storage(m_graph)->m_allocator;
        m_pass->initTargets = A->alloc<PassNode::InitTarget>(PassNode::kMaxInitTargets);
    }
    m_pass->initTargets[m_pass->initCount++] = { target, hash };
}

void PassBuilder::force_keep()
{
    m_pass->forceKeep = true;
}

} // namespace webgpu::rg
