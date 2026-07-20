/*****************************************************************************
 * weBIGeo
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

#include "RenderGraphPanel.h"

#include "ImGuiManager.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "webgpu/base/RenderGraph_internal.h"


using namespace webgpu;
using namespace webgpu::rg;
using namespace webgpu::rg::Internal;


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4456 4457 4100 4189 4505 4996)
#endif

// short labels/tints for the node boxes
static const char* rg_kind_name(PassKind k)
{
    switch (k) {
    case PassKind::Graphics: return "gfx";
    case PassKind::Compute:  return "compute";
    case PassKind::Transfer: return "transfer";
    default:                 return "none";
    }
}
static ImU32 rg_kind_color(PassKind k)
{
    switch (k) {
    case PassKind::Graphics: return IM_COL32(54, 96, 156, 255);
    case PassKind::Compute:  return IM_COL32(156, 96, 40, 255);
    case PassKind::Transfer: return IM_COL32(56, 120, 70, 255);
    default:                 return IM_COL32(90, 90, 90, 255);
    }
}
static const char* rg_access_name(AccessType t)
{
    switch (t) {
    case AccessType::ColorAttachment:        return "color";
    case AccessType::DepthStencilAttachment: return "depth";
    case AccessType::DepthStencilReadOnly:   return "depth(ro)";
    case AccessType::Sampled:                return "sampled";
    case AccessType::StorageRead:            return "storage(r)";
    case AccessType::StorageWrite:           return "storage(w)";
    case AccessType::Uniform:                return "uniform";
    case AccessType::CopySrc:                return "copy(src)";
    case AccessType::CopyDst:                return "copy(dst)";
    case AccessType::Vertex:                 return "vertex";
    case AccessType::Index:                  return "index";
    case AccessType::Indirect:               return "indirect";
    }
    return "?";
}
// textures cool, buffers warm
static ImU32 rg_resource_color(ResourceKind k)
{
    return k == ResourceKind::Texture ? IM_COL32(70, 120, 170, 255)
                                            : IM_COL32(170, 120, 60, 255);
}

// short format label. anything this sample does not create falls back to the raw enum, so an unexpected
// recreate still shows something legible.
static const char* rg_format_name(WGPUTextureFormat f)
{
    switch (f)
    {
    // 8-bit formats
    case WGPUTextureFormat_R8Unorm:              return "R8Unorm";
    case WGPUTextureFormat_R8Snorm:              return "R8Snorm";
    case WGPUTextureFormat_R8Uint:               return "R8Uint";
    case WGPUTextureFormat_R8Sint:               return "R8Sint";

    // 16-bit formats
    case WGPUTextureFormat_R16Uint:              return "R16Uint";
    case WGPUTextureFormat_R16Sint:              return "R16Sint";
    case WGPUTextureFormat_R16Float:             return "R16Float";

    case WGPUTextureFormat_RG8Unorm:             return "RG8Unorm";
    case WGPUTextureFormat_RG8Snorm:             return "RG8Snorm";
    case WGPUTextureFormat_RG8Uint:              return "RG8Uint";
    case WGPUTextureFormat_RG8Sint:              return "RG8Sint";

    // 32-bit formats
    case WGPUTextureFormat_R32Float:             return "R32Float";
    case WGPUTextureFormat_R32Uint:              return "R32Uint";
    case WGPUTextureFormat_R32Sint:              return "R32Sint";

    case WGPUTextureFormat_RG16Uint:             return "RG16Uint";
    case WGPUTextureFormat_RG16Sint:             return "RG16Sint";
    case WGPUTextureFormat_RG16Float:            return "RG16Float";

    case WGPUTextureFormat_RGBA8Unorm:           return "RGBA8Unorm";
    case WGPUTextureFormat_RGBA8UnormSrgb:       return "RGBA8UnormSrgb";
    case WGPUTextureFormat_RGBA8Snorm:           return "RGBA8Snorm";
    case WGPUTextureFormat_RGBA8Uint:            return "RGBA8Uint";
    case WGPUTextureFormat_RGBA8Sint:            return "RGBA8Sint";

    case WGPUTextureFormat_BGRA8Unorm:           return "BGRA8Unorm";
    case WGPUTextureFormat_BGRA8UnormSrgb:       return "BGRA8UnormSrgb";

    case WGPUTextureFormat_RGB10A2Uint:          return "RGB10A2Uint";
    case WGPUTextureFormat_RGB10A2Unorm:         return "RGB10A2Unorm";
    case WGPUTextureFormat_RG11B10Ufloat:        return "RG11B10Ufloat";

    // 64-bit formats
    case WGPUTextureFormat_RG32Float:            return "RG32Float";
    case WGPUTextureFormat_RG32Uint:             return "RG32Uint";
    case WGPUTextureFormat_RG32Sint:             return "RG32Sint";

    case WGPUTextureFormat_RGBA16Uint:           return "RGBA16Uint";
    case WGPUTextureFormat_RGBA16Sint:           return "RGBA16Sint";
    case WGPUTextureFormat_RGBA16Float:          return "RGBA16Float";

    // 128-bit formats
    case WGPUTextureFormat_RGBA32Float:          return "RGBA32Float";
    case WGPUTextureFormat_RGBA32Uint:           return "RGBA32Uint";
    case WGPUTextureFormat_RGBA32Sint:           return "RGBA32Sint";

    // depth / stencil
    case WGPUTextureFormat_Stencil8:             return "Stencil8";
    case WGPUTextureFormat_Depth16Unorm:         return "Depth16Unorm";
    case WGPUTextureFormat_Depth24Plus:          return "Depth24Plus";
    case WGPUTextureFormat_Depth24PlusStencil8: return "Depth24PlusStencil8";
    case WGPUTextureFormat_Depth32Float:         return "Depth32Float";
    case WGPUTextureFormat_Depth32FloatStencil8:return "Depth32FloatStencil8";

    // BC compressed
    case WGPUTextureFormat_BC1RGBAUnorm:         return "BC1RGBAUnorm";
    case WGPUTextureFormat_BC1RGBAUnormSrgb:     return "BC1RGBAUnormSrgb";
    case WGPUTextureFormat_BC2RGBAUnorm:         return "BC2RGBAUnorm";
    case WGPUTextureFormat_BC2RGBAUnormSrgb:     return "BC2RGBAUnormSrgb";
    case WGPUTextureFormat_BC3RGBAUnorm:         return "BC3RGBAUnorm";
    case WGPUTextureFormat_BC3RGBAUnormSrgb:     return "BC3RGBAUnormSrgb";
    case WGPUTextureFormat_BC4RUnorm:            return "BC4RUnorm";
    case WGPUTextureFormat_BC4RSnorm:            return "BC4RSnorm";
    case WGPUTextureFormat_BC5RGUnorm:           return "BC5RGUnorm";
    case WGPUTextureFormat_BC5RGSnorm:           return "BC5RGSnorm";
    case WGPUTextureFormat_BC6HRGBUfloat:        return "BC6HRGBUfloat";
    case WGPUTextureFormat_BC6HRGBFloat:         return "BC6HRGBFloat";
    case WGPUTextureFormat_BC7RGBAUnorm:         return "BC7RGBAUnorm";
    case WGPUTextureFormat_BC7RGBAUnormSrgb:     return "BC7RGBAUnormSrgb";

    // ETC2
    case WGPUTextureFormat_ETC2RGB8Unorm:             return "ETC2RGB8Unorm";
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:         return "ETC2RGB8UnormSrgb";
    case WGPUTextureFormat_ETC2RGB8A1Unorm:           return "ETC2RGB8A1Unorm";
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:       return "ETC2RGB8A1UnormSrgb";
    case WGPUTextureFormat_ETC2RGBA8Unorm:            return "ETC2RGBA8Unorm";
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:        return "ETC2RGBA8UnormSrgb";
    case WGPUTextureFormat_EACR11Unorm:               return "EACR11Unorm";
    case WGPUTextureFormat_EACR11Snorm:               return "EACR11Snorm";
    case WGPUTextureFormat_EACRG11Unorm:              return "EACRG11Unorm";
    case WGPUTextureFormat_EACRG11Snorm:              return "EACRG11Snorm";

    // ASTC
    case WGPUTextureFormat_ASTC4x4Unorm:              return "ASTC4x4Unorm";
    case WGPUTextureFormat_ASTC4x4UnormSrgb:          return "ASTC4x4UnormSrgb";
    case WGPUTextureFormat_ASTC5x4Unorm:              return "ASTC5x4Unorm";
    case WGPUTextureFormat_ASTC5x4UnormSrgb:          return "ASTC5x4UnormSrgb";
    case WGPUTextureFormat_ASTC5x5Unorm:              return "ASTC5x5Unorm";
    case WGPUTextureFormat_ASTC5x5UnormSrgb:          return "ASTC5x5UnormSrgb";
    case WGPUTextureFormat_ASTC6x5Unorm:              return "ASTC6x5Unorm";
    case WGPUTextureFormat_ASTC6x5UnormSrgb:          return "ASTC6x5UnormSrgb";
    case WGPUTextureFormat_ASTC6x6Unorm:              return "ASTC6x6Unorm";
    case WGPUTextureFormat_ASTC6x6UnormSrgb:          return "ASTC6x6UnormSrgb";
    case WGPUTextureFormat_ASTC8x5Unorm:              return "ASTC8x5Unorm";
    case WGPUTextureFormat_ASTC8x5UnormSrgb:          return "ASTC8x5UnormSrgb";
    case WGPUTextureFormat_ASTC8x6Unorm:              return "ASTC8x6Unorm";
    case WGPUTextureFormat_ASTC8x6UnormSrgb:          return "ASTC8x6UnormSrgb";
    case WGPUTextureFormat_ASTC8x8Unorm:              return "ASTC8x8Unorm";
    case WGPUTextureFormat_ASTC8x8UnormSrgb:          return "ASTC8x8UnormSrgb";
    case WGPUTextureFormat_ASTC10x5Unorm:             return "ASTC10x5Unorm";
    case WGPUTextureFormat_ASTC10x5UnormSrgb:         return "ASTC10x5UnormSrgb";
    case WGPUTextureFormat_ASTC10x6Unorm:             return "ASTC10x6Unorm";
    case WGPUTextureFormat_ASTC10x6UnormSrgb:         return "ASTC10x6UnormSrgb";
    case WGPUTextureFormat_ASTC10x8Unorm:             return "ASTC10x8Unorm";
    case WGPUTextureFormat_ASTC10x8UnormSrgb:         return "ASTC10x8UnormSrgb";
    case WGPUTextureFormat_ASTC10x10Unorm:            return "ASTC10x10Unorm";
    case WGPUTextureFormat_ASTC10x10UnormSrgb:        return "ASTC10x10UnormSrgb";
    case WGPUTextureFormat_ASTC12x10Unorm:            return "ASTC12x10Unorm";
    case WGPUTextureFormat_ASTC12x10UnormSrgb:        return "ASTC12x10UnormSrgb";
    case WGPUTextureFormat_ASTC12x12Unorm:            return "ASTC12x12Unorm";
    case WGPUTextureFormat_ASTC12x12UnormSrgb:        return "ASTC12x12UnormSrgb";

    default:
        break;
    }

    static char buf[32];
    std::snprintf(buf, sizeof(buf), "TextureFormat(%u)", (unsigned)f);
    return buf;
}

// usage bits the pool keys on
static void rg_usage_str(WGPUTextureUsage u, char* out, size_t n)
{
    std::snprintf(out, n, "%s%s%s%s%s",
        (u & WGPUTextureUsage_RenderAttachment) ? "A" : "",
        (u & WGPUTextureUsage_TextureBinding)   ? "T" : "",
        (u & WGPUTextureUsage_StorageBinding)   ? "S" : "",
        (u & WGPUTextureUsage_CopySrc)          ? "r" : "",
        (u & WGPUTextureUsage_CopyDst)          ? "w" : "");
}

// usage bits the buffer usage keys on
static void rg_buf_usage_str(WGPUBufferUsage u, char* out, size_t n)
{
    std::snprintf(out, n, "%s%s%s%s%s%s%s",
        (u & WGPUBufferUsage_Uniform)  ? "U" : "",
        (u & WGPUBufferUsage_Storage)  ? "S" : "",
        (u & WGPUBufferUsage_Vertex)   ? "V" : "",
        (u & WGPUBufferUsage_Index)    ? "I" : "",
        (u & WGPUBufferUsage_Indirect) ? "X" : "",
        (u & WGPUBufferUsage_CopySrc)  ? "r" : "",
        (u & WGPUBufferUsage_CopyDst)  ? "w" : "");
}

static uint64_t rg_entry_bytes(const TransientResourcePool::Entry& e)
{
    return texture_bytes(e.sig.size, e.sig.format, e.sig.mipLevelCount, e.sig.sampleCount, e.sig.dim);
}

// byte count -> short human string. NOTE(Huerbe): no GB tier, a transient pool is a few MB.
static void rg_bytes_str(uint64_t bytes, char* out, size_t n)
{
    if      (bytes >= (1u << 20)) std::snprintf(out, n, "%.1f MB", bytes / (1024.0 * 1024.0));
    else if (bytes >= (1u << 10)) std::snprintf(out, n, "%.1f KB", bytes / 1024.0);
    else                          std::snprintf(out, n, "%llu B",  (unsigned long long)bytes);
}

// argb-alpha tweak + warm/cool access tints, shared by the DAG pins and the lifetime bars
static ImU32 rg_with_alpha(ImU32 c, ImU32 a) { return (c & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT); }
static constexpr ImU32 kRGWrite = IM_COL32(232, 145, 64, 255);   // write / output pin
static constexpr ImU32 kRGRead = IM_COL32(74, 158, 206, 255);   // read / input pin
static constexpr ImU32 kRGExt = IM_COL32(118, 196, 132, 255);   // external-input source node (imported read)
static constexpr ImU32 kRGPresent = IM_COL32(196, 122, 214, 255);   // present / display sink node (imported output)
static constexpr ImU32 kRGDead = IM_COL32(196, 104, 92, 255);   // graph-owned write nothing consumes (wasted store)

// stable colour per group, hashed from the prefix
static ImU32 group_color(WGPUStringView prefix)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sv_length(prefix); ++i) { h ^= (uint8_t)prefix.data[i]; h *= 16777619u; }
    static const ImU32 pal[] = {
        IM_COL32(120, 180, 230, 230), IM_COL32(230, 170, 90, 230), IM_COL32(150, 210, 140, 230),
        IM_COL32(210, 140, 200, 230), IM_COL32(220, 205, 110, 230), IM_COL32(140, 205, 210, 230),
    };
    return pal[h % (sizeof pal / sizeof pal[0])];
}

// ID-stack-independent key for a group's collapse state. ImGui::GetID is seeded by the current window,
// so the pre-layout read and the in-canvas click write would hash one string to different ids.
static ImGuiID rg_grp_key(WGPUStringView prefix)
{
    ImU32 h = 2166136261u;
    for (const char* s = "rg.grp."; *s; ++s) { h ^= (uint8_t)*s; h *= 16777619u; }
    for (size_t i = 0; i < sv_length(prefix); ++i) { h ^= (uint8_t)prefix.data[i]; h *= 16777619u; }
    return (ImGuiID)h;
}

// DAG view -----------------------------------------------------------------------------------------
// one box per pass in dependency columns, one pin per access (reads left, writes right), edges run
// producer-output -> consumer-input. hovering a pin lights the upstream producer cone. reads the internal
// node structs directly and assumes a compiled, realized graph.

static constexpr int kRgDagMax = 128;
static constexpr int kRgGPinMax = 32;   // max interface pins drawn on a collapsed group node (silently capped)

// one laid-out pass box. its index in box[] is the execution-order index, so it doubles as the
// adjacency / cone index.
struct RgDagBox { PassNode* p; int layer; ImVec2 tl; float w, h; int nIn, nOut; };

// one access is one pin, writes land on the right
static bool rg_pass_writes(PassNode* p, uint32_t id)
{
    for (uint32_t i = 0; i < p->accessCount; ++i)
        if (p->accesses[i].handle.id == id && access_is_write(p->accesses[i].type)) return true;
    return false;
}

// does this access consume the resource's prior contents, i.e. earn an input pin? plain reads do, and so
// does a LoadOp_Load attachment, which therefore gets BOTH an input and an output pin.
static bool rg_access_reads(const ResourceAccess& a)
{
    if (!access_is_write(a.type)) return true;
    return (a.type == AccessType::ColorAttachment || a.type == AccessType::DepthStencilAttachment)
        && a.loadOp == WGPULoadOp_Load;
}

// 0 = no common write, 1 = at DIFFERENT subresources (parallel cascades, not a conflict), 2 = at the
// SAME subresource (a real WAW)
static int rg_shared_write(PassNode* a, PassNode* b)
{
    int r = 0;
    for (uint32_t i = 0; i < a->accessCount; ++i) {
        if (!access_is_write(a->accesses[i].type)) continue;
        for (uint32_t j = 0; j < b->accessCount; ++j) {
            if (!access_is_write(b->accesses[j].type) || a->accesses[i].handle.id != b->accesses[j].handle.id) continue;
            if (a->accesses[i].baseLayer == b->accesses[j].baseLayer && a->accesses[i].baseMip == b->accesses[j].baseMip) return 2;
            r = 1;
        }
    }
    return r;
}

// index of pass in box[], or -1
static int rg_box_index(const RgDagBox* box, int n, PassNode* p)
{
    for (int i = 0; i < n; ++i) if (box[i].p == p) return i;
    return -1;
}

// the highest-execution-index writer of `id` among p's predecessors. compile() always links the RAW
// producer into adjacency and a later writer would start a version p cannot see, so the latest
// writer-predecessor IS the producer. -1 = no in-graph producer, so an external input pin.
static int rg_producer_of(const RgDagBox* box, int n, PassNode* p, uint32_t id)
{
    int best = -1;
    for (NodeAdjacency* a = p->adjacency; a; a = a->next) {
        if (!rg_pass_writes(a->pass, id)) continue;
        int idx = rg_box_index(box, n, a->pass);
        if (idx > best) best = idx;
    }
    return best;
}

// output-pin slot index of resource id on pass p, or -1
static int rg_out_slot(PassNode* p, uint32_t id)
{
    int slot = 0;
    for (uint32_t k = 0; k < p->accessCount; ++k)
        if (access_is_write(p->accesses[k].type)) { if (p->accesses[k].handle.id == id) return slot; ++slot; }
    return -1;
}

// the read side of rg_out_slot. matches the encounter-order slot the pin loop assigns, so it lands on
// the right pin centre.
static int rg_in_slot(PassNode* p, uint32_t id)
{
    int slot = 0;
    for (uint32_t k = 0; k < p->accessCount; ++k)
        if (rg_access_reads(p->accesses[k])) { if (p->accesses[k].handle.id == id) return slot; ++slot; }
    return -1;
}

// external interface of the pass run [gi, gj): reads produced outside become in-pins, writes consumed
// outside (or imported/persistent) become out-pins, interior resources get no pin. both lists dedup and
// cap at kRgGPinMax silently. one walk for every group node.
static void rg_group_interface(RenderGraph* rg, const RgDagBox* box, int n, int gi, int gj,
    uint32_t* inId, int& nIn, uint32_t* outId, int& nOut)
{
    nIn = 0; nOut = 0;
    for (int k = gi; k < gj; ++k) {
        PassNode* p = box[k].p;
        for (uint32_t ai = 0; ai < p->accessCount; ++ai) {
            if (!rg_access_reads(p->accesses[ai])) continue;
            uint32_t id = p->accesses[ai].handle.id;
            int prod = rg_producer_of(box, n, p, id);
            if (prod >= gi && prod < gj) continue;
            bool seen = false; for (int s = 0; s < nIn; ++s) seen |= inId[s] == id;
            if (!seen && nIn < kRgGPinMax) inId[nIn++] = id;
        }
    }
    for (int k = gi; k < gj; ++k) {
        PassNode* p = box[k].p;
        for (uint32_t ai = 0; ai < p->accessCount; ++ai) {
            if (!access_is_write(p->accesses[ai].type)) continue;
            uint32_t id = p->accesses[ai].handle.id;
            ResourceNode* r = find_node(rg, { id });
            bool external = r && (r->imported || r->persistent);   // history write leaves to next frame -> group output
            for (int j = 0; j < n && !external; ++j) {
                if (j >= gi && j < gj) continue;
                if (rg_in_slot(box[j].p, id) < 0) continue;
                int pr = rg_producer_of(box, n, box[j].p, id);
                if (pr >= gi && pr < gj) external = true;
            }
            if (!external) continue;
            bool seen = false; for (int s = 0; s < nOut; ++s) seen |= outId[s] == id;
            if (!seen && nOut < kRgGPinMax) outId[nOut++] = id;
        }
    }
}

// the upstream producer cone: DFS over predecessor edges. iterative, a long chain would overflow the
// stack.
static void rg_mark_cone(const RgDagBox* box, int n, int seed, bool* inCone)
{
    if (seed < 0) return;
    int stack[kRgDagMax], sp = 0;
    stack[sp++] = seed; inCone[seed] = true;
    while (sp) {
        PassNode* p = box[stack[--sp]].p;
        for (NodeAdjacency* a = p->adjacency; a; a = a->next) {
            int idx = rg_box_index(box, n, a->pass);
            if (idx >= 0 && !inCone[idx]) { inCone[idx] = true; stack[sp++] = idx; }
        }
    }
}

// case-insensitive name filter
static bool rg_name_has(WGPUStringView name, const char* needle)
{
    if (!needle || !needle[0]) return true;
    if (!name.data) return false;
    size_t q = 0; while (needle[q]) ++q;
    auto lc = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; };
    for (size_t i = 0; i + q <= name.length; ++i) {
        size_t j = 0; while (j < q && lc(name.data[i + j]) == lc(needle[j])) ++j;
        if (j == q) return true;
    }
    return false;
}

// squared distance from p to segment ab, for edge hit-testing
static float rg_seg_d2(ImVec2 p, ImVec2 a, ImVec2 b)
{
    float vx = b.x - a.x, vy = b.y - a.y, wx = p.x - a.x, wy = p.y - a.y;
    float L = vx * vx + vy * vy, t = L > 0 ? (wx * vx + wy * vy) / L : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    float dx = a.x + t * vx - p.x, dy = a.y + t * vy - p.y;
    return dx * dx + dy * dy;
}

// a true graph output: writes an imported resource whose value leaves the frame. compile()'s p->sink
// also fires for history-only writers, which are not graph outputs, so the halo skips them.
static bool rg_pass_is_sink(RenderGraph* rg, PassNode* p)
{
    for (uint32_t i = 0; i < p->accessCount; ++i) {
        if (!access_is_write(p->accesses[i].type)) continue;
        ResourceNode* r = find_node(rg, p->accesses[i].handle);
        if (r && r->imported) return true;
    }
    return false;
}

// shorten `s` until it fits `avail` px, marking the cut with "..". always keeps the START: clipping a
// right-aligned label from the left turns "clouds.lo_color" into "louds.lo_color", a different resource.
static void rg_fit_text(char* s, float avail)
{
    if (avail <= 0.0f) { s[0] = '\0'; return; }
    if (ImGui::CalcTextSize(s).x <= avail) return;
    for (size_t n = std::strlen(s); n >= 2; --n) {
        s[n - 2] = '.'; s[n - 1] = '.'; s[n] = '\0';   // drop one real char, keep the ".." marker
        if (ImGui::CalcTextSize(s).x <= avail) return;
    }
    s[0] = '\0';   // nothing fits
}

// round for textures, square for buffers. filled = normal, hollow = external input. the square
// half-extent equals the circle radius, so one hover ring frames either shape.
static void rg_draw_pin(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool filled, bool buffer)
{
    if (buffer) {
        ImVec2 a(c.x - r, c.y - r), b(c.x + r, c.y + r);
        if (filled) dl->AddRectFilled(a, b, col, 1.5f);
        else        dl->AddRect(a, b, col, 1.5f, 0, 2.0f);
    }
    else {
        if (filled) dl->AddCircleFilled(c, r, col, 12);
        else        dl->AddCircle(c, r, col, 12, 2.0f);
    }
}

// signed horizontal control-point offset for an edge bezier. a plain dx*0.5 suits horizontal hops, but
// once the drop dwarfs the run it draws a detour around an obstacle that is not there. fade the tangent
// out with the run/drop ratio, floored so shallow hops keep the rounded look. MUST be shared by the draw
// and the hit-test, or hovering stops matching where the edge is drawn.
static float rg_edge_tangent(ImVec2 a, ImVec2 b)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    float k = ady > 1e-3f ? adx / ady : 1.0f;
    k = k < 0.35f ? 0.35f : (k > 1.0f ? 1.0f : k);
    return dx * 0.5f * k;
}

// ImDrawList has no dashed stroke, so sample the curve and draw every other span. marks the history
// feedback links as cross-frame.
static void rg_dashed_cubic(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImVec2 p2, ImVec2 p3, ImU32 col, float th)
{
    constexpr int kSeg = 24;
    ImVec2 prev = p0;
    for (int i = 1; i <= kSeg; ++i) {
        float t = (float)i / kSeg, u = 1 - t, w0 = u*u*u, w1 = 3*u*u*t, w2 = 3*u*t*t, w3 = t*t*t;
        ImVec2 q(w0*p0.x + w1*p1.x + w2*p2.x + w3*p3.x, w0*p0.y + w1*p1.y + w2*p2.y + w3*p3.y);
        if (i & 1) dl->AddLine(prev, q, col, th);   // every other span = a dash
        prev = q;
    }
}

// arrowhead at `tip`, marking the read end of a feedback link
static void rg_arrowhead(ImDrawList* dl, ImVec2 from, ImVec2 tip, ImU32 col, float sz)
{
    float dx = tip.x - from.x, dy = tip.y - from.y, L = std::sqrt(dx*dx + dy*dy);
    if (L < 1e-3f) return;
    dx /= L; dy /= L;
    ImVec2 a(tip.x - dx*sz - dy*sz*0.5f, tip.y - dy*sz + dx*sz*0.5f);
    ImVec2 b(tip.x - dx*sz + dy*sz*0.5f, tip.y - dy*sz - dx*sz*0.5f);
    dl->AddTriangleFilled(tip, a, b, col);
}

// ---- DAG intermediate representation. a node is a pass or a virtual endpoint, linked by RgEdges. the
// pass-graph (box[]) still drives layout, the draw consumes this IR for the nodes + virtual links. groups
// live in the GView side-table. see docs/rendergraph-nested-layout.md.
struct RgNode {
    enum class Kind : uint8_t { Pass, Group, Virtual };
    Kind kind{};
    PassNode*      pass{};      // kind::Pass
    ResourceNode*  res{};       // kind::Virtual (the endpoint's resource)
    WGPUStringView label{};     // kind::Virtual caption
    ImVec2 pos{}; float w = 0, h = 0; int col = 0;              // layout result -> consumed by draw
    ImU32 tint = 0;             // kind::Virtual base colour (read/write/imported/present)
};
struct RgEdge {
    int srcNode = -1, dstNode = -1; uint16_t srcPin = 0, dstPin = 0; uint32_t resId = 0;
    enum class Kind : uint8_t { Raw, History, Fanout } kind = Kind::Raw;
};

// barycenter crossing-reduction + y-relax over a layered node set (Sugiyama). lcol[c] = node indices in
// column c, reordered in place. lleft/lright[i] = neighbours, h[i] = height, writes cy[i]. shared by the
// top-level layout and each expanded group's interior.
// clus/clusPar: per-node cluster id + per-cluster parent. keeps every cluster's members contiguous, so an
// expanded group's members never interleave with outsiders and its border stays a clean hull.
// thin: this layered node is an edge lane, not a box. charging a lane a full row inflates every column an
// edge crosses, shoving real boxes apart for traffic.
static void rg_barycenter_relax(std::vector<int>* lcol, int maxCol,
    const std::vector<std::vector<int>>& lleft, const std::vector<std::vector<int>>& lright,
    const std::vector<float>& h, std::vector<float>& cy, float rowGap,
    const int* clus = nullptr, const int* clusPar = nullptr, int nClus = 0, const char* thin = nullptr)
{
    int LN = (int)cy.size();
    static std::vector<int> lrank; lrank.assign(LN, 0);
    auto reix = [&](int c) { for (int r = 0; r < (int)lcol[c].size(); ++r) lrank[lcol[c][r]] = r; };
    for (int c = 0; c <= maxCol; ++c) reix(c);
    auto bnb = [&](int li, bool right) { const std::vector<int>& nb = right ? lright[li] : lleft[li]; float s = 0; int c = 0; for (int x : nb) { s += lrank[x]; ++c; } return c ? s / c : -1.0f; };
    auto lsweep = [&](bool backward) {
        int from = backward ? maxCol - 1 : 1, to = backward ? -1 : maxCol + 1, step = backward ? -1 : 1;
        for (int c = from; c != to; c += step) {
            std::vector<int>& m = lcol[c];
            std::vector<float> key(m.size());
            for (int j = 0; j < (int)m.size(); ++j) { float b = bnb(m[j], backward); key[j] = b < 0 ? (float)lrank[m[j]] : b; }
            for (int a = 1; a < (int)m.size(); ++a) { int mv = m[a]; float kv = key[a]; int b = a - 1; while (b >= 0 && key[b] > kv) { m[b + 1] = m[b]; key[b + 1] = key[b]; --b; } m[b + 1] = mv; key[b + 1] = kv; }
            reix(c);
        }
        };
    lsweep(true); lsweep(false); lsweep(true);
    // re-sort each column so a cluster's members stay together. clusters are weighted by their members'
    // mean rank, so a group still floats to its barycenter, it just moves as one block.
    if (clus) {
        // root..innermost chain of cluster ids, depth returned. shallow, path length <= ~4.
        auto chain = [&](int li, int* out) -> int {
            int tmp[16], d = 0; for (int c = clus[li]; c >= 0 && d < 16; c = clusPar[c]) tmp[d++] = c;
            for (int i = 0; i < d; ++i) out[i] = tmp[d - 1 - i]; return d;
        };
        static std::vector<float> cavg; static std::vector<int> ccnt;
        for (int c = 0; c <= maxCol; ++c) {
            std::vector<int>& m = lcol[c];
            cavg.assign(nClus, 0.0f); ccnt.assign(nClus, 0);
            for (int li : m) { int ch[16]; int d = chain(li, ch); for (int i = 0; i < d; ++i) { cavg[ch[i]] += lrank[li]; ++ccnt[ch[i]]; } }
            for (int x = 0; x < nClus; ++x) if (ccnt[x]) cavg[x] /= ccnt[x];
            // compare down the cluster paths, weighting a shared-prefix cluster by its mean rank and a
            // node's own level by its rank. first differing weight decides, ties keep stable order.
            auto before = [&](int a, int b) -> bool {
                int ca[16], cb[16]; int da = chain(a, ca), db = chain(b, cb);
                for (int d = 0;; ++d) {
                    if (d < da && d < db && ca[d] == cb[d]) continue;   // same cluster here -> go deeper
                    float wa = d < da ? cavg[ca[d]] : (float)lrank[a];
                    float wb = d < db ? cavg[cb[d]] : (float)lrank[b];
                    if (wa != wb) return wa < wb;
                    return lrank[a] < lrank[b];
                }
            };
            for (int i = 1; i < (int)m.size(); ++i) { int v = m[i], j = i - 1; while (j >= 0 && before(v, m[j])) { m[j + 1] = m[j]; --j; } m[j + 1] = v; }
            reix(c);
        }
    }
    // gap only BETWEEN two real nodes. a folded member is zero-height and must not reserve a row, or a
    // collapsed group leaves a tall empty column. an edge lane gets a sliver, since charging it a whole
    // rowGap scaled a column's height with the traffic crossing it.
    auto gap = [&](int a, int b) {
        if (h[a] <= 0.5f || h[b] <= 0.5f) return 0.0f;
        if (thin && (thin[a] || thin[b])) return rowGap * 0.5f;
        return rowGap;
    };
    for (int c = 0; c <= maxCol; ++c) { float y = 0; int prev = -1; for (int li : lcol[c]) { if (prev >= 0) y += gap(prev, li); cy[li] = y + h[li] * 0.5f; y += h[li]; prev = li; } }
    for (int it = 0; it < 8; ++it)
        for (int c = 0; c <= maxCol; ++c) {
            std::vector<int>& m = lcol[c];
            for (int r = 0; r < (int)m.size(); ++r) {
                int li = m[r]; float s = 0; int cnt = 0;
                for (int x : lleft[li])  { s += cy[x]; ++cnt; }
                for (int x : lright[li]) { s += cy[x]; ++cnt; }
                if (!cnt) continue;
                float d = s / cnt;
                float lo = r > 0                ? cy[m[r - 1]] + (h[m[r - 1]] + h[li]) * 0.5f + gap(m[r - 1], li) : -1e30f;
                float hi = r + 1 < (int)m.size() ? cy[m[r + 1]] - (h[li] + h[m[r + 1]]) * 0.5f - gap(li, m[r + 1]) :  1e30f;
                cy[li] = d < lo ? lo : d > hi ? hi : d;
            }
        }
}

// the first `segs` dotted segments of `name`: "bloom.down.0" with segs=2 -> "bloom.down". empty if the
// name has fewer. generalizes group_prefix (the segs==1 case) so a group tree can partition each level.
static WGPUStringView group_prefix_n(WGPUStringView name, int segs)
{
    size_t n = sv_length(name); int seen = 0;
    for (size_t i = 0; i < n; ++i)
        if (name.data[i] == '.' && ++seen == segs) return WGPUStringView{ name.data, i };
    return (seen + 1 == segs) ? name : WGPUStringView{};   // exactly `segs` segments -> whole name is the prefix
}

// a contiguous run [gi, gj) of box[], contiguous because passes are declared grouped. gtree[0] is the
// root. kids are deeper sub-runs, and passes no kid covers are this node's leaf members. layout fields are
// filled post-order by layout_gnode, relative to the node's content origin.
struct GNode {
    WGPUStringView prefix;     // full dotted prefix at this depth ("bloom", "bloom.down"); empty at root
    int gi, gj, depth;
    bool collapsed;
    std::vector<int> kids;     // child GNode indices in the gtree arena
    float w, h;                // box (collapsed) or region (expanded) size, content-local
    int nIn, nOut;
    uint32_t inId[kRgGPinMax], outId[kRgGPinMax];
    float inY[kRgGPinMax], outY[kRgGPinMax];   // interface pin y, relative to the node top
};

// build the group tree over box[gi, gj), returning the new node's index. partitions into maximal sub-runs
// sharing their first depth+1 segments, and a run of >=2 that is not the whole range becomes a child.
// collapse state is the tri-state ImGuiStorage the draw uses: 0 follow-default, 1 open, 2 closed.
static int build_gtree(std::vector<GNode>& gtree, const RgDagBox* box, int n,
    ImGuiStorage* grpStore, bool collapseDefault, int gi, int gj, int depth, WGPUStringView prefix)
{
    int self = (int)gtree.size();
    gtree.push_back(GNode{});
    GNode g{};
    g.prefix = prefix; g.gi = gi; g.gj = gj; g.depth = depth;
    if (sv_length(prefix)) { int st = grpStore->GetInt(rg_grp_key(prefix), 0); g.collapsed = st == 2 ? true : st == 1 ? false : collapseDefault; }
    for (int a = gi; a < gj;) {
        WGPUStringView sub = group_prefix_n(box[a].p->id.name, depth + 1);
        int b = a + 1;
        while (b < gj && sv_length(sub) && sv_eq(group_prefix_n(box[b].p->id.name, depth + 1), sub)) ++b;
        if (sv_length(sub) && b - a >= 2 && !(a == gi && b == gj))
            g.kids.push_back(build_gtree(gtree, box, n, grpStore, collapseDefault, a, b, depth + 1, sub));
        a = b;
    }
    gtree[self] = std::move(g);
    return self;
}

// the OUTERMOST collapsed group ancestor of pass k, -1 if visible at every level. a pass with an owner is
// hidden, and the owner's first member is the rep standing in for the whole subtree. top-level and nested
// collapse differ only in where the rep cell lands, not in how membership is decided.
static void rg_mark_collapsed(const std::vector<GNode>& gt, int* collOwner, int node, int owner)
{
    const GNode& g = gt[node];
    int myOwner = owner;
    if (owner < 0 && sv_length(g.prefix) && g.collapsed) myOwner = node;
    if (myOwner >= 0) for (int k = g.gi; k < g.gj; ++k) if (collOwner[k] < 0) collOwner[k] = myOwner;
    for (int kid : g.kids) rg_mark_collapsed(gt, collOwner, kid, myOwner);
}

// the innermost EXPANDED gtree node containing pass k, and its parent. drives the cluster-contiguity
// ordering so an expanded group and its expanded subgroups lay out as one block. a collapsed group folds,
// so its subtree is not descended and members keep their nearest expanded ancestor.
static void rg_assign_clusters(const std::vector<GNode>& gt, int* clusterOf, int* clusterParent, int node)
{
    const GNode& g = gt[node];
    for (int kid : g.kids) clusterParent[kid] = node;
    if (sv_length(g.prefix)) {
        if (g.collapsed) return;   // folded subtree -> members keep the parent expanded cluster
        for (int k = g.gi; k < g.gj; ++k) clusterOf[k] = node;
    }
    for (int kid : g.kids) rg_assign_clusters(gt, clusterOf, clusterParent, kid);
}

// leaves a box horizontally, S-curves to a flat top lane, runs across, S-curves back down. horizontal
// tangents at the pins AND the plateau, so no corner is sharp.
static void rg_over_arc(ImDrawList* dl, ImVec2 a, ImVec2 b, float topY, ImU32 col, float th, float maxSh)
{
    float dx = b.x - a.x, adx = dx < 0 ? -dx : dx, dir = dx < 0 ? -1.0f : 1.0f;
    float sh = adx * 0.5f; if (sh > maxSh) sh = maxSh;   // shoulder run; shrinks for short hops so the two S's don't overlap
    ImVec2 aTop(a.x + dir * sh, topY), bTop(b.x - dir * sh, topY);
    float m1 = a.x + dir * sh * 0.5f, m2 = b.x - dir * sh * 0.5f;
    dl->AddBezierCubic(a, ImVec2(m1, a.y), ImVec2(m1, topY), aTop, col, th);
    if ((bTop.x - aTop.x) * dir > 0.5f) dl->AddLine(aTop, bTop, col, th);
    dl->AddBezierCubic(bTop, ImVec2(m2, topY), ImVec2(m2, b.y), b, col, th);
}

static void rg_draw_dag(RenderGraph* rg, RenderGraphStorage& s)
{
    float kBoxW = 190.0f, kColGap = 65.0f, kRowGap = 50.0f;
    float kHeaderH = 22.0f, kFooterH = 14.0f, kPinRowH = 18.0f, kMinBodyH = 12.0f;
    float kPinR = 5.0f, kPinHit = 8.0f;
    float kRegionPad = 24.0f;   // padding from a group's member bbox out to its hull border

    // resolved BEFORE layout, so positions, scrolling and font all use one zoom this frame. resolving it
    // after layout drew the graph at the old scale for a frame and flickered.
    static ImVec2 scrolling(0, 0);
    static bool userMoved = false;          // true once the user pans/zooms; until then keep the graph centred
    static bool s_canvasHovered = false;    // last frame's hover (input is read before the canvas item exists)
    static ImVec2 s_winPos(0, 0);           // last frame's canvas top-left, for the cursor-anchored zoom

    // 1.0 is closest, the authored sizes, and the wheel only zooms OUT toward the cursor. every layout
    // constant scales by it so layout and draw stay one coordinate system. text scales separately.
    static float zoom = 1.0f;
    {
        ImGuiIO& io = ImGui::GetIO();
        if (s_canvasHovered && io.MouseWheel != 0.0f) {
            float z0 = zoom;
            zoom *= io.MouseWheel > 0 ? 1.1f : 1.0f / 1.1f;
            if (zoom > 1.0f) zoom = 1.0f;
            if (zoom < 0.2f) zoom = 0.2f;
            float r = zoom / z0;   // positions rescale by r this frame; anchor the canvas point under the cursor
            scrolling.x = io.MousePos.x - s_winPos.x - (io.MousePos.x - s_winPos.x - scrolling.x) * r;
            scrolling.y = io.MousePos.y - s_winPos.y - (io.MousePos.y - s_winPos.y - scrolling.y) * r;
            userMoved = true;
        }
    }
    const float z = zoom;
    kBoxW *= z; kColGap *= z; kRowGap *= z;
    kHeaderH *= z; kFooterH *= z; kPinRowH *= z; kMinBodyH *= z;
    kPinR *= z; kPinHit *= z;
    kRegionPad *= z;


    // frame-boundary endpoints, toggled from the toolbar. read before layout, so disabling them also drops
    // their layout influence.
    static bool showVirtual = true;
    // imported buffers: on = one source node fanning faint edges to every reader, off = a node per use
    // site. read here so it shapes the layout too.
    static bool fanBuffers = true;
    // read before layout, so a collapsed group reserves its compact slot the same frame the checkbox flips
    static bool collapseDefault = false;
    // captured in the OUTER window so the pre-layout reads and the in-canvas click writes hit one store
    ImGuiStorage* grpStore = ImGui::GetStateStorage();

    // ---- layout pass, sink-anchored Sugiyama. column = longest distance TO a sink over the FULL
    // dependency graph, so sinks land rightmost and a WAW-ordered pass still sits in order rather than
    // floating off as a fake sink. crossing reduction counts only the DRAWN edges, since WAW has no pin to
    // cross. one barycenter sweep back from the sinks, then a settle.
    RgDagBox box[kRgDagMax];
    int n = 0;
    for (PassNode* p = s.m_passes; p && n < kRgDagMax; p = p->next, ++n) {
        int nIn = 0, nOut = 0;   // a Load attachment counts as both (read-modify-write)
        for (uint32_t k = 0; k < p->accessCount; ++k) {
            if (rg_access_reads(p->accesses[k]))      ++nIn;
            if (access_is_write(p->accesses[k].type)) ++nOut;
        }
        int rows = nIn > nOut ? nIn : nOut;
        float h = kHeaderH + (rows ? rows * kPinRowH : kMinBodyH) + kFooterH;
        box[n] = { p, 0, ImVec2(0, 0), kBoxW, h, nIn, nOut };
    }
    // so the toolbar can flag a graph larger than the DAG cap, which truncates silently
    int rgPassTotal = 0; for (PassNode* q = s.m_passes; q; q = q->next) ++rgPassTotal;

    // longest path to a sink over all deps. reverse exec order visits sinks first and relaxes each
    // producer, then column = maxDist - dist.
    int dist[kRgDagMax] = {}, maxDist = 0;
    for (int i = n - 1; i >= 0; --i) {
        for (NodeAdjacency* a = box[i].p->adjacency; a; a = a->next) {
            int q = rg_box_index(box, n, a->pass);
            if (q >= 0 && dist[i] + 1 > dist[q]) dist[q] = dist[i] + 1;
        }
        if (dist[i] > maxDist) maxDist = dist[i];
    }

    // passes writing one resource at DIFFERENT subresources with no data dependency, e.g. CSM cascades.
    // the whole-resource WAW would string them across columns, so union them onto one level instead.
    // anc[v][u] means u is a transitive ancestor of v, so a real chain like a mip pyramid is not
    // mistaken for parallel.
    static bool anc[kRgDagMax][kRgDagMax];
    for (int v = 0; v < n; ++v) for (int u = 0; u < n; ++u) anc[v][u] = false;
    for (int v = 0; v < n; ++v) {
        PassNode* p = box[v].p;
        for (uint32_t k = 0; k < p->accessCount; ++k) {
            if (!rg_access_reads(p->accesses[k])) continue;
            int u = rg_producer_of(box, n, p, p->accesses[k].handle.id);
            if (u < 0) continue;
            anc[v][u] = true;
            for (int w = 0; w < n; ++w) if (anc[u][w]) anc[v][w] = true;
        }
    }
    int groupRep[kRgDagMax];
    for (int i = 0; i < n; ++i) groupRep[i] = i;
    auto find = [&](int x) { while (groupRep[x] != x) { groupRep[x] = groupRep[groupRep[x]]; x = groupRep[x]; } return x; };
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (rg_shared_write(box[i].p, box[j].p) == 1 && !anc[i][j] && !anc[j][i]) groupRep[find(i)] = find(j);
    for (int i = 0; i < n; ++i) groupRep[i] = find(i);   // flatten to roots

    // snap each group to its rightmost member so siblings share one level, then compact the empties
    int colOf[kRgDagMax], groupCol[kRgDagMax] = {};
    for (int i = 0; i < n; ++i) colOf[i] = maxDist - dist[i];
    for (int i = 0; i < n; ++i) if (colOf[i] > groupCol[groupRep[i]]) groupCol[groupRep[i]] = colOf[i];
    for (int i = 0; i < n; ++i) colOf[i] = groupCol[groupRep[i]];
    int remap[kRgDagMax]; for (int c = 0; c <= maxDist; ++c) remap[c] = -1;
    for (int i = 0; i < n; ++i) remap[colOf[i]] = 1;
    int nextCol = 0; for (int c = 0; c <= maxDist; ++c) if (remap[c] == 1) remap[c] = nextCol++;
    for (int i = 0; i < n; ++i) colOf[i] = remap[colOf[i]];
    maxDist = nextCol ? nextCol - 1 : 0;

    // built here, where columns are final, and consumed by the layout + draw. NOTE(Huerbe): not yet wired
    // into layout, the reservation blocks below still run. this only stands the structure up.
    static std::vector<GNode> gtree; gtree.clear();
    build_gtree(gtree, box, n, grpStore, collapseDefault, 0, n, 0, WGPUStringView{});
    IM_ASSERT(gtree[0].gi == 0 && gtree[0].gj == n);
    int collOwner[kRgDagMax]; for (int i = 0; i < n; ++i) collOwner[i] = -1;
    rg_mark_collapsed(gtree, collOwner, 0, -1);
    // a pass inside ANY collapsed group is not drawn as its own box, the group draws one node in its place
    bool drawHidden[kRgDagMax]; for (int i = 0; i < n; ++i) drawHidden[i] = collOwner[i] >= 0;

    // ---- collapse folding, pre-layout. every OUTERMOST collapsed group folds to ONE node at its rep
    // cell, and the other members get zero height and snap onto the rep's column so they reserve nothing.
    // EXPANDED groups reserve no slot: their members stay first-class nodes in their own columns, held
    // together by the cluster-contiguity pass and hulled later.
    float effH[kRgDagMax];
    for (int i = 0; i < n; ++i) effH[i] = box[i].h;
    for (int o = 0; o < (int)gtree.size(); ++o) {
        GNode& g = gtree[o];
        if (!sv_length(g.prefix) || !g.collapsed) continue;
        if (collOwner[g.gi] != o) continue;   // shadowed by a collapsed ancestor -> that outer group is the rep
        uint32_t inId[kRgGPinMax], outId[kRgGPinMax]; int ni = 0, no = 0;
        rg_group_interface(rg, box, n, g.gi, g.gj, inId, ni, outId, no);
        int rows = ni > no ? ni : no;
        float compactH = kHeaderH + (rows ? rows * kPinRowH : kMinBodyH) + kFooterH;
        int colA = colOf[g.gi]; for (int k = g.gi; k < g.gj; ++k) if (colOf[k] < colA) colA = colOf[k];   // leftmost member column
        for (int k = g.gi; k < g.gj; ++k) { effH[k] = (k == g.gi ? compactH : 0.0f); colOf[k] = colA; }
    }

    // cluster ids + parent links for the layout ordering
    int clusterOf[kRgDagMax]; for (int i = 0; i < n; ++i) clusterOf[i] = -1;
    static std::vector<int> clusterParent; clusterParent.assign(gtree.size(), -1);
    rg_assign_clusters(gtree, clusterOf, clusterParent.data(), 0);

    // recompute columns with each collapsed group contracted to its rep: an edge inside costs 0, a
    // crossing edge 1. a consumer of a collapsed sub-chain then lands right after the rep instead of where
    // the unfolded chain stranded it. expanded groups reproduce the original columns exactly.
    {
        int rep[kRgDagMax]; for (int i = 0; i < n; ++i) rep[i] = collOwner[i] >= 0 ? gtree[collOwner[i]].gi : i;
        int dist2[kRgDagMax]; for (int i = 0; i < n; ++i) dist2[i] = 0; int maxD2 = 0;
        for (int i = n - 1; i >= 0; --i)
            for (NodeAdjacency* a = box[i].p->adjacency; a; a = a->next) {
                int q = rg_box_index(box, n, a->pass); if (q < 0) continue;   // a->pass = producer of i
                int w = rep[i] == rep[q] ? 0 : 1;
                if (dist2[rep[i]] + w > dist2[rep[q]]) dist2[rep[q]] = dist2[rep[i]] + w;
            }
        for (int i = 0; i < n; ++i) if (dist2[rep[i]] > maxD2) maxD2 = dist2[rep[i]];
        for (int i = 0; i < n; ++i) colOf[i] = maxD2 - dist2[rep[i]];   // members of a group share their rep's column
        // re-snap parallel-writer groups to one column, then compact the empties
        int gc[kRgDagMax]; for (int i = 0; i < n; ++i) gc[i] = 0;
        for (int i = 0; i < n; ++i) if (colOf[i] > gc[groupRep[i]]) gc[groupRep[i]] = colOf[i];
        for (int i = 0; i < n; ++i) colOf[i] = gc[groupRep[i]];
        maxDist = maxD2;
        for (int c = 0; c <= maxDist; ++c) remap[c] = -1;
        for (int i = 0; i < n; ++i) remap[colOf[i]] = 1;
        nextCol = 0; for (int c = 0; c <= maxDist; ++c) if (remap[c] == 1) remap[c] = nextCol++;
        for (int i = 0; i < n; ++i) colOf[i] = remap[colOf[i]];
        maxDist = nextCol ? nextCol - 1 : 0;
    }

    // ---- virtual nodes: frame-boundary endpoints drawn as real DAG nodes, so the column/barycenter/y
    // code places them like any pass. each attaches to ONE pass pin, a read one column before its reader and
    // a write one column after its writer, so a widely-read texture gets a small node per use site rather
    // than one node fanning edges everywhere. three kinds, all gated by the toolbar toggle:
    //   * history: curr (written this frame for next) and prev (last frame's curr). cross-frame, so no
    //     in-frame edge joins the pair.
    //   * external input: an imported resource read with no in-graph writer. a texture gets a node per
    //     reader, a buffer gets ONE source fanning faint edges, so a uniform does not swamp the view.
    //   * present: an imported resource that IS written, the swapchain.
    // an endpoint for a pass inside an expanded group parks in the column JUST OUTSIDE it, so the relax
    // reserves space outside the hull rather than at member_col +/-1, which can land inside the border.
    static std::vector<int> vReadCol, vWriteCol; vReadCol.assign(n, 0); vWriteCol.assign(n, 0);
    for (int i = 0; i < n; ++i) { vReadCol[i] = colOf[i] - 1; vWriteCol[i] = colOf[i] + 1; }
    {
        static std::vector<int> sMin, sMax; sMin.assign(gtree.size(), 1 << 30); sMax.assign(gtree.size(), -1);
        for (int o = 1; o < (int)gtree.size(); ++o) {
            GNode& g = gtree[o]; if (!sv_length(g.prefix) || g.collapsed) continue;
            if (collOwner[g.gi] >= 0 && collOwner[g.gi] != o) continue;
            for (int k = g.gi; k < g.gj; ++k) { if (colOf[k] < sMin[o]) sMin[o] = colOf[k]; if (colOf[k] > sMax[o]) sMax[o] = colOf[k]; }
        }
        for (int i = 0; i < n; ++i) {
            if (collOwner[i] >= 0) continue;   // folded -> routes to a compact pin, not a hull pin
            int best = -1, bd = 1 << 30;
            for (int o = 1; o < (int)gtree.size(); ++o) {
                GNode& g = gtree[o]; if (!sv_length(g.prefix) || g.collapsed || sMax[o] < 0) continue;
                if (g.gi <= i && i < g.gj && g.depth < bd) { bd = g.depth; best = o; }
            }
            if (best >= 0) { vReadCol[i] = sMin[best] - 1; vWriteCol[i] = sMax[best] + 1; }
        }
    }
    struct TNode { bool isRead; int passBox, pin; ResourceNode* res; int col; float w, h; const char* cap; ImU32 tint; int li; };
    static std::vector<TNode> tnodes; tnodes.clear();
    auto push_tnode = [&](bool isRead, int passBox, int pin, ResourceNode* res, const char* cap, ImU32 tint) {
        char b[48]; std::snprintf(b, sizeof b, "%.*s", (int)res->id.name.length, res->id.name.data ? res->id.name.data : "?");
        ImVec2 ns = ImGui::CalcTextSize(b), cs = ImGui::CalcTextSize(cap);
        float w = ((ns.x > cs.x ? ns.x : cs.x) + 16) * zoom, h = (ns.y + cs.y + 10) * zoom;
        tnodes.push_back({ isRead, passBox, pin, res, isRead ? vReadCol[passBox] : vWriteCol[passBox], w, h, cap, tint, -1 });
    };
    if (showVirtual)
        for (ResourceNode* r = s.m_resouces; r; r = r->next) {
            // keyed on `history`, NOT `persistent`, which also covers plain create_persistent_* resources.
            // keying this arm on persistent swallowed those, leaving them with no endpoint at all.
            if (r->history) {   // history: write node at the curr writer, read node at each prev reader
                bool curr = r->historyIndex == 0;
                for (int i = 0; i < n; ++i) {
                    int sl = curr ? rg_out_slot(box[i].p, r->handle.id) : rg_in_slot(box[i].p, r->handle.id);
                    if (sl >= 0) push_tnode(!curr, i, sl, r, curr ? "next frame" : "last frame", curr ? kRGWrite : kRGRead);
                }
                continue;
            }
            // imported is caller-owned, plain persistent is pool-backed and carried in place. either way
            // the value crosses the frame boundary rather than being produced here, so both earn an endpoint.
            if (!r->imported && !r->persistent) {
                // graph-owned and written but never read, so the store is wasted and StoreOp_Discard would
                // make it memoryless. worth a marker rather than a dangling pin.
                int lw = -1, lwSlot = -1, readers = 0;
                for (int i = 0; i < n; ++i) {
                    int sl = rg_out_slot(box[i].p, r->handle.id);
                    if (sl >= 0) { lw = i; lwSlot = sl; }
                    if (rg_in_slot(box[i].p, r->handle.id) >= 0) ++readers;
                }
                if (lw >= 0 && readers == 0) push_tnode(false, lw, lwSlot, r, "dead end", kRGDead);
                continue;
            }
            const char* readCap   = r->imported ? "imported" : "persistent";
            const char* writeCap  = r->imported ? "present"  : "persists";
            const ImU32 writeTint = r->imported ? kRGPresent : kRGWrite;
            int lwb = -1, lws = -1;   // last writer, if any
            for (int i = 0; i < n; ++i) { int sl = rg_out_slot(box[i].p, r->handle.id); if (sl >= 0) { lwb = i; lws = sl; } }
            // source and sink are independent, not an either/or on whether anything writes this. the
            // swapchain is both: blitted with LoadOp_Load, so keying the source off `lwb < 0` hid the input.
            auto externalRead = [&](int i) {
                return rg_in_slot(box[i].p, r->handle.id) >= 0 && rg_producer_of(box, n, box[i].p, r->handle.id) < 0;
            };
            if (r->imported && r->kind == ResourceKind::Buffer && fanBuffers) {
                // a uniform is read almost everywhere, so emit ONE source at its earliest reader and let
                // the draw fan faint edges out. imported only, the IR's fan-out edge kind keys on that too.
                int best = -1, bestSl = -1;
                for (int i = 0; i < n; ++i) { if (!externalRead(i)) continue; if (best < 0 || colOf[i] < colOf[best]) { best = i; bestSl = rg_in_slot(box[i].p, r->handle.id); } }
                if (best >= 0) push_tnode(true, best, bestSl, r, readCap, kRGExt);
            }
            else   // texture, persistent buffer, or an imported buffer with fan-out off -> node at each reader pin
                for (int i = 0; i < n; ++i) if (externalRead(i)) push_tnode(true, i, rg_in_slot(box[i].p, r->handle.id), r, readCap, kRGExt);
            if (lwb >= 0)              // written in-graph -> sink node at the last writer (present / persists)
                push_tnode(false, lwb, lws, r, writeCap, writeTint);
        }
    // keep columns in [0, maxDist], shifting right if a read node landed left of 0
    int tmin = 0; for (TNode& t : tnodes) if (t.col < tmin) tmin = t.col;
    if (tmin < 0) { for (int i = 0; i < n; ++i) colOf[i] -= tmin; maxDist -= tmin; for (TNode& t : tnodes) t.col -= tmin; }
    for (TNode& t : tnodes) if (t.col > maxDist) maxDist = t.col;

    // ===== layered routing. an edge skipping columns gets a dummy in each crossed column, reserving a
    // lane so it never hides behind a box.
    struct LNode { int col, box, tn; float x, y; };       // box == -1 => dummy/history; tn >= 0 => history node
    struct REdge { int src, dst, sOut, dIn; uint32_t id; int chainN, chain[kRgDagMax]; };
    static std::vector<LNode> lnode; lnode.clear();
    static std::vector<REdge> edge;  edge.clear();
    for (int i = 0; i < n; ++i) lnode.push_back({ colOf[i], i, -1, 0, 0 });   // reals: lnode index == box index

    // a read pin fed by its producer + parallel-writer siblings, each with a dummy chain
    for (int i = 0; i < n; ++i) {
        PassNode* p = box[i].p; int inS = 0;
        for (uint32_t k = 0; k < p->accessCount; ++k) {
            if (!rg_access_reads(p->accesses[k])) continue;
            uint32_t id = p->accesses[k].handle.id; int dIn = inS++;
            int prod = rg_producer_of(box, n, p, id);
            if (prod < 0) continue;
            for (int w = 0; w < n; ++w) {
                if (groupRep[w] != groupRep[prod] || colOf[w] >= colOf[i]) continue;
                int sOut = rg_out_slot(box[w].p, id);
                if (sOut < 0) continue;
                REdge e{ w, i, sOut, dIn, id, 0, {} };
                for (int c = colOf[w] + 1; c < colOf[i]; ++c) { e.chain[e.chainN++] = (int)lnode.size(); lnode.push_back({ c, -1, -1, 0, 0 }); }
                edge.push_back(e);
            }
        }
    }

    // history nodes join the layered list as their own nodes, box == -1 and tn >= 0
    for (int ti = 0; ti < (int)tnodes.size(); ++ti) { tnodes[ti].li = (int)lnode.size(); lnode.push_back({ tnodes[ti].col, -1, ti, 0, 0 }); }

    // neighbour lists for the barycenter, per-column members built after the remap below
    const int LN = (int)lnode.size();
    static std::vector<std::vector<int>> lleft, lright; lleft.assign(LN, {}); lright.assign(LN, {});
    for (REdge& e : edge) {
        int prev = e.src;
        for (int t = 0; t < e.chainN; ++t) { lright[prev].push_back(e.chain[t]); lleft[e.chain[t]].push_back(prev); prev = e.chain[t]; }
        lright[prev].push_back(e.dst); lleft[e.dst].push_back(prev);
    }
    // a read node sits left of its reader, a write node right of its writer. one link each is enough to
    // order them and align them to the pass's row.
    for (TNode& t : tnodes)
        if (t.isRead) { lright[t.li].push_back(t.passBox); lleft[t.passBox].push_back(t.li); }
        else          { lright[t.passBox].push_back(t.li); lleft[t.li].push_back(t.passBox); }

    // a real box carries its pass's cluster, a dummy carries the deepest expanded group containing BOTH
    // ends of its edge, so interior lanes stay in the band and boundary lanes stay out. history = -1.
    static std::vector<int> lclus; lclus.assign(LN, -1);
    for (int i = 0; i < n; ++i) lclus[i] = clusterOf[i];
    {
        static std::vector<char> anc; anc.assign(gtree.size(), 0);
        auto lca = [&](int a, int b) -> int {
            if (a < 0 || b < 0) return -1;
            for (int x = a; x >= 0; x = clusterParent[x]) anc[x] = 1;
            int r = -1; for (int x = b; x >= 0; x = clusterParent[x]) if (anc[x]) { r = x; break; }
            for (int x = a; x >= 0; x = clusterParent[x]) anc[x] = 0;
            return r;
        };
        for (REdge& e : edge) { int ec = lca(clusterOf[e.src], clusterOf[e.dst]); for (int t = 0; t < e.chainN; ++t) lclus[e.chain[t]] = ec; }
    }

    // ---- independent work graphs, laid out separately. disconnected components were right-anchored to the
    // global sink column, so they shared buckets and the barycenter + width passes cross-influenced them.
    // give each component its own disjoint dense column range from its base. a component is gap-free within
    // its span, so base + (col - min) remaps it densely and the ranges sum to <= n columns. each bucket then
    // holds one component, and a vertical band separates them.
    static std::vector<int> comp; comp.assign(LN, 0);
    static std::vector<int> compMinCol; compMinCol.assign(LN, 1 << 30);
    {
        for (int li = 0; li < LN; ++li) comp[li] = li;
        auto cfind = [&](int x) { while (comp[x] != x) { comp[x] = comp[comp[x]]; x = comp[x]; } return x; };
        for (int li = 0; li < LN; ++li) { for (int x : lleft[li]) comp[cfind(li)] = cfind(x); for (int x : lright[li]) comp[cfind(li)] = cfind(x); }
        // a group run draws as one bordered region, so force it into one component. clear-only members have
        // no read edge, so each would otherwise split off and the banding would stack them.
        for (int ga = 0; ga < n;) {
            WGPUStringView pre = group_prefix(box[ga].p->id.name);
            int gb = ga + 1;
            while (gb < n && sv_length(pre) && sv_eq(group_prefix(box[gb].p->id.name), pre)) ++gb;
            if (sv_length(pre) && gb - ga >= 2) for (int k = ga + 1; k < gb; ++k) comp[cfind(ga)] = cfind(k);
            ga = gb;
        }
        for (int li = 0; li < LN; ++li) comp[li] = cfind(li);
        // compact each root to a first-seen id, which is exec order since reals come first
        static std::vector<int> cid; cid.assign(LN, -1); int numComp = 0;
        for (int li = 0; li < LN; ++li) if (cid[comp[li]] < 0) cid[comp[li]] = numComp++;
        static std::vector<int> cMin, cMax; cMin.assign(numComp, 1 << 30); cMax.assign(numComp, -1);
        for (int li = 0; li < LN; ++li) { int c = cid[comp[li]], oc = lnode[li].col; if (oc < cMin[c]) cMin[c] = oc; if (oc > cMax[c]) cMax[c] = oc; }
        static std::vector<int> cBase; cBase.assign(numComp, 0); int acc = 0;
        for (int c = 0; c < numComp; ++c) { cBase[c] = acc; acc += cMax[c] - cMin[c] + 1; }
        for (int li = 0; li < LN; ++li) { int c = cid[comp[li]]; lnode[li].col = cBase[c] + (lnode[li].col - cMin[c]); }
        for (int i = 0; i < n; ++i) colOf[i] = lnode[i].col;   // reals: lnode i == box i; keep colW indexing in sync
        maxDist = acc > 0 ? acc - 1 : 0;
        for (int li = 0; li < LN; ++li) if (lnode[li].col < compMinCol[comp[li]]) compMinCol[comp[li]] = lnode[li].col;
    }

    // per-column members, now that columns are final per-component
    static std::vector<int> lcol[kRgDagMax];
    for (int c = 0; c <= maxDist; ++c) lcol[c].clear();
    for (int li = 0; li < LN; ++li) lcol[lnode[li].col].push_back(li);

    // materialize node heights first: a box gets its reserved height, a history node its own, a dummy a lane
    const float kLane = 16.0f * zoom;
    static std::vector<float> lh; lh.assign(LN, 0.0f);
    for (int li = 0; li < LN; ++li) { int b = lnode[li].box; lh[li] = b >= 0 ? effH[b] : (lnode[li].tn >= 0 ? tnodes[lnode[li].tn].h : kLane); }
    static std::vector<float> cy; cy.assign(LN, 0.0f);
    // neither a box nor an endpoint is a bare edge lane, so it reserves a sliver not a row
    static std::vector<char> lthin; lthin.assign(LN, 0);
    for (int li = 0; li < LN; ++li) lthin[li] = (lnode[li].box < 0 && lnode[li].tn < 0) ? 1 : 0;
    rg_barycenter_relax(lcol, maxDist, lleft, lright, lh, cy, kRowGap, lclus.data(), clusterParent.data(), (int)gtree.size(), lthin.data());

    // stack the components into disjoint vertical bands, each shifted below the previous one's extent
    {
        static std::vector<float> cTop, cBot, cOff; static std::vector<char> seen;
        cTop.assign(LN, 1e30f); cBot.assign(LN, -1e30f); cOff.assign(LN, 0.0f); seen.assign(LN, 0);
        for (int li = 0; li < LN; ++li) { float t = cy[li] - lh[li] * 0.5f, b = cy[li] + lh[li] * 0.5f; if (t < cTop[comp[li]]) cTop[comp[li]] = t; if (b > cBot[comp[li]]) cBot[comp[li]] = b; }
        float cursor = 0.0f;
        for (int li = 0; li < LN; ++li) { int c = comp[li]; if (seen[c]) continue; seen[c] = 1; cOff[c] = cursor - cTop[c]; cursor += (cBot[c] - cTop[c]) + kRowGap * 2.0f; }
        for (int li = 0; li < LN; ++li) cy[li] += cOff[comp[li]];
    }

    // shove non-members out of each expanded group's band so the hull encloses ONLY its members, e.g. a
    // history node the relax floated into a column the group spans. deepest-first, so a node ends up outside
    // the OUTERMOST band it intruded on.
    {
        auto inSub = [&](int li, int anc) { for (int c = lclus[li]; c >= 0; c = clusterParent[c]) if (c == anc) return true; return false; };
        for (int o = (int)gtree.size() - 1; o >= 1; --o) {
            GNode& g = gtree[o];
            if (!sv_length(g.prefix) || g.collapsed) continue;          // only expanded groups draw a hull
            if (collOwner[g.gi] >= 0 && collOwner[g.gi] != o) continue;  // shadowed by a collapsed ancestor -> not drawn
            float bandTop = 1e30f, bandBot = -1e30f; int colMin = 1 << 30, colMax = -1;
            for (int li = 0; li < LN; ++li) if (inSub(li, o)) {
                if (cy[li] - lh[li] * 0.5f < bandTop) bandTop = cy[li] - lh[li] * 0.5f;
                if (cy[li] + lh[li] * 0.5f > bandBot) bandBot = cy[li] + lh[li] * 0.5f;
                int c = lnode[li].col; if (c < colMin) colMin = c; if (c > colMax) colMax = c;
            }
            if (colMax < colMin) continue;
            float hullTop = bandTop - kRegionPad - kHeaderH, hullBot = bandBot + kRegionPad, mid = (bandTop + bandBot) * 0.5f;
            const float kClear = kRowGap * 0.5f;
            // decide first, then place. evicted only if it is a non-member, sits under the hull's columns,
            // is not wired to a member (those belong AT the border), and actually overlaps the band.
            static std::vector<char> evict; evict.assign(LN, 0);
            for (int li = 0; li < LN; ++li) {
                if (inSub(li, o)) continue;                                      // a member / interior dummy belongs inside
                int c = lnode[li].col;
                if (c < colMin || c > colMax) continue;                          // not under the hull
                bool linked = false;
                for (int x : lleft[li])  if (inSub(x, o)) { linked = true; break; }
                if (!linked) for (int x : lright[li]) if (inSub(x, o)) { linked = true; break; }
                if (linked) continue;
                float t = cy[li] - lh[li] * 0.5f, b = cy[li] + lh[li] * 0.5f;
                if (b <= hullTop || t >= hullBot) continue;                      // already clear
                evict[li] = 1;
            }
            // place each evicted node in the nearest free gap on its side. two things at once: several
            // evictions in one column must stack (the frontier), and a slot must not land on a node staying
            // put there (the walk). the exempt linked endpoints are exactly that hazard, parking at the
            // border where the frontier starts. walking past only what is in the way keeps the node near the
            // pass it annotates.
            static std::vector<float> topFront, botFront;
            topFront.assign(maxDist + 1, hullTop - kClear);
            botFront.assign(maxDist + 1, hullBot + kClear);
            for (int li = 0; li < LN; ++li) {
                if (!evict[li]) continue;
                const int c = lnode[li].col;
                const float h = lh[li];
                const bool up = cy[li] < mid;
                float edge = up ? topFront[c] : botFront[c];   // free boundary to grow from
                for (int guard = 0; guard < LN + 2; ++guard) {
                    const float t = up ? edge - h : edge, b = up ? edge : edge + h;
                    int hit = -1;
                    for (int x = 0; x < LN && hit < 0; ++x) {
                        if (x == li || evict[x] || lnode[x].col != c) continue;
                        // only dodge things that draw a box. a bare dummy is an edge lane, and a busy column
                        // carries one per through-edge, which is how endpoints ended up hundreds of px away.
                        if (lnode[x].box < 0 && lnode[x].tn < 0) continue;
                        const float xt = cy[x] - lh[x] * 0.5f, xb = cy[x] + lh[x] * 0.5f;
                        if (b > xt && t < xb) hit = x;
                    }
                    if (hit < 0) break;
                    edge = up ? cy[hit] - lh[hit] * 0.5f - kClear : cy[hit] + lh[hit] * 0.5f + kClear;
                }
                cy[li] = up ? edge - h * 0.5f : edge + h * 0.5f;
                if (up) topFront[c] = edge - h - kClear; else botFront[c] = edge + h + kClear;
            }
        }
    }

    // positions + the canvas-local bounding box, used to centre the view
    float gxMin = 1e30f, gyMin = 1e30f, gxMax = -1e30f, gyMax = -1e30f;
    // every node is one box wide, expanded groups reserve no wide region, so column x is a uniform cadence
    float colW[kRgDagMax], colX[kRgDagMax];
    for (int c = 0; c <= maxDist; ++c) colW[c] = kBoxW;
    // an endpoint is centred in its column and a long resource name can outgrow a pass box, so widen the
    // column to fit. otherwise it overhangs kColGap both sides and eventually lands on the neighbours.
    for (const TNode& t : tnodes) { int c = lnode[t.li].col; if (t.w > colW[c]) colW[c] = t.w; }
    colX[0] = 0; for (int c = 1; c <= maxDist; ++c) colX[c] = colX[c - 1] + colW[c - 1] + kColGap;
    for (int c = 0; c <= maxDist; ++c) {
        for (int li : lcol[c]) {
            float cx = colX[c] - colX[compMinCol[comp[li]]];   // left-align this component to x=0
            if (lnode[li].box >= 0) {
                int b = lnode[li].box; box[b].tl = ImVec2(cx, cy[li] - effH[b] * 0.5f); box[b].layer = c;
                if (cx < gxMin) gxMin = cx; if (cx + box[b].w > gxMax) gxMax = cx + box[b].w;
                if (box[b].tl.y < gyMin) gyMin = box[b].tl.y; if (box[b].tl.y + effH[b] > gyMax) gyMax = box[b].tl.y + effH[b];
            }
            else {
                lnode[li].x = cx; lnode[li].y = cy[li];   // dummy: column left edge + lane centre
                int t = lnode[li].tn;
                if (t >= 0) {   // history node: centre in the (possibly widened) column slot, include in the view bbox
                    float ctr = cx + colW[c] * 0.5f, hw = tnodes[t].w * 0.5f, hh = tnodes[t].h * 0.5f;
                    lnode[li].x = ctr;
                    if (ctr - hw < gxMin) gxMin = ctr - hw; if (ctr + hw > gxMax) gxMax = ctr + hw;
                    if (cy[li] - hh < gyMin) gyMin = cy[li] - hh; if (cy[li] + hh > gyMax) gyMax = cy[li] + hh;
                }
            }
        }
    }
    // ---- lane pull. the relax minimises CROSSINGS, not detour, so a lane can end up far past the box it
    // dodged and turn a short hop into a canvas-spanning swoop. pull each lane onto the straight line
    // between its pins, then push it clear. obstacles include lanes already placed this pass, which is what
    // stops parallel edges collapsing onto one ideal line. the push direction is fixed on the first hit, so
    // a lane wedged between two obstacles walks out one side instead of oscillating.
    {
        static std::vector<char> laneSet; laneSet.assign(LN, 0);
        auto extent = [&](int x, float& t, float& b) -> bool {   // vertical extent of an obstacle, false = not one
            if (lnode[x].box >= 0) { t = box[lnode[x].box].tl.y; b = t + effH[lnode[x].box]; return effH[lnode[x].box] > 0.5f; }
            if (lnode[x].tn >= 0)  { const float h = tnodes[lnode[x].tn].h; t = lnode[x].y - h * 0.5f; b = lnode[x].y + h * 0.5f; return true; }
            if (laneSet[x])        { t = lnode[x].y - kLane * 0.5f; b = lnode[x].y + kLane * 0.5f; return true; }
            return false;
        };
        // hull band of every expanded group, mirroring the draw's border rect, so a lane can be held inside
        // the group it belongs to
        static std::vector<float> gTop, gBot; static std::vector<int> gcMin, gcMax;
        gTop.assign(gtree.size(), 0.0f); gBot.assign(gtree.size(), 0.0f);
        gcMin.assign(gtree.size(), 1 << 30); gcMax.assign(gtree.size(), -1);
        for (int o = 1; o < (int)gtree.size(); ++o) {
            const GNode& g = gtree[o];
            if (!sv_length(g.prefix) || g.collapsed) continue;
            if (collOwner[g.gi] >= 0 && collOwner[g.gi] != o) continue;
            float t0 = 1e30f, b0 = -1e30f;
            for (int k = g.gi; k < g.gj; ++k) {
                if (effH[k] <= 0.5f) continue;   // folded member reserves nothing
                if (box[k].tl.y < t0) t0 = box[k].tl.y;
                if (box[k].tl.y + effH[k] > b0) b0 = box[k].tl.y + effH[k];
                if (colOf[k] < gcMin[o]) gcMin[o] = colOf[k];
                if (colOf[k] > gcMax[o]) gcMax[o] = colOf[k];
            }
            if (b0 < t0) { gcMax[o] = -1; continue; }
            gTop[o] = t0 - kRegionPad - kHeaderH; gBot[o] = b0 + kRegionPad;
        }
        // two rounds, and the order is the point. a lane with an endpoint inside a group must stay in that
        // group's often-few-px band or the edge visibly leaves the hull. a pass-through edge has no such
        // constraint, so it claims last, or it takes the interior slots first and evicts the constrained one.
        for (int round = 0; round < 2; ++round)
        for (REdge& e : edge) {
            if (!e.chainN) continue;
            const bool grouped = clusterOf[e.src] >= 0 || clusterOf[e.dst] >= 0;
            if ((round == 0) != grouped) continue;
            const float sy = box[e.src].tl.y + kHeaderH + e.sOut * kPinRowH + kPinRowH * 0.5f;
            const float dy = box[e.dst].tl.y + kHeaderH + e.dIn  * kPinRowH + kPinRowH * 0.5f;
            const float sx = box[e.src].tl.x + box[e.src].w, ex = box[e.dst].tl.x;
            for (int t = 0; t < e.chainN; ++t) {
                const int li = e.chain[t], c = lnode[li].col;
                const float lx = lnode[li].x + kBoxW * 0.5f;
                float f = ex > sx ? (lx - sx) / (ex - sx) : 0.5f;
                f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
                float want = sy + (dy - sy) * f;   // straight-line y at this column
                // does either end of this edge sit inside group o
                auto belongsTo = [&](int o) {
                    const GNode& g = gtree[o];
                    return (e.src >= g.gi && e.src < g.gj) || (e.dst >= g.gi && e.dst < g.gj);
                };
                // inside a group the edge heads for that group's BORDER pin, not its far endpoint, so the
                // global source->dest line is the wrong target: a destination below the hull aims the lane
                // straight out through the floor. hold it in the band of every group it belongs to.
                auto holdInOwn = [&](float y) {
                    for (int o = 1; o < (int)gtree.size(); ++o) {
                        if (gcMax[o] < gcMin[o] || c < gcMin[o] || c > gcMax[o] || !belongsTo(o)) continue;
                        const float lo = gTop[o] + kLane, hi = gBot[o] - kLane;
                        if (hi > lo) y = y < lo ? lo : (y > hi ? hi : y);
                    }
                    return y;
                };
                // walk out from `start` until nothing overlaps. a FOREIGN hull counts as one solid obstacle:
                // pushing just outside it up front was not enough, since a later step of this walk would
                // deposit the lane back inside.
                auto settle = [&](float start, int dir) {
                    float y = start;
                    for (int guard = 0; guard < LN + 8; ++guard) {
                        bool hit = false; float ht = 0, hb = 0;
                        for (int x = 0; x < LN && !hit; ++x) {
                            if (x == li || lnode[x].col != c) continue;
                            float xt, xb;
                            if (!extent(x, xt, xb)) continue;
                            if (y + kLane * 0.5f > xt && y - kLane * 0.5f < xb) { hit = true; ht = xt; hb = xb; }
                        }
                        for (int o = 1; o < (int)gtree.size() && !hit; ++o) {
                            if (gcMax[o] < gcMin[o] || c < gcMin[o] || c > gcMax[o] || belongsTo(o)) continue;
                            const float xt = gTop[o] - kLane, xb = gBot[o] + kLane;
                            if (y + kLane * 0.5f > xt && y - kLane * 0.5f < xb) { hit = true; ht = xt; hb = xb; }
                        }
                        if (!hit) break;
                        y = dir < 0 ? ht - kLane * 0.5f - 4.0f : hb + kLane * 0.5f + 4.0f;
                    }
                    return y;
                };
                want = holdInOwn(want);
                // try BOTH sides and keep the nearer resting place. the nearer side of the FIRST obstacle can
                // hide a whole group hull to climb, where the other way clears one box.
                const float up = settle(want, -1), down = settle(want, +1);
                want = holdInOwn((want - up <= down - want) ? up : down);
                lnode[li].y = want; laneSet[li] = 1;
            }
        }
    }

    if (gxMax < gxMin) { gxMin = gyMin = gxMax = gyMax = 0; }   // empty graph

    // ---- IR mirror: RgNodes index-aligned to box[], then Virtual endpoints, plus RgEdges. the draw
    // consumes this for the nodes + virtual links, box[]/edge[] still drives layout and the solid edges.
    static std::vector<RgNode> rgn; rgn.clear();
    static std::vector<RgEdge> rge; rge.clear();
    for (int i = 0; i < n; ++i) {
        RgNode nd{}; nd.kind = RgNode::Kind::Pass; nd.pass = box[i].p;
        nd.label = box[i].p->id.name; nd.pos = box[i].tl; nd.w = box[i].w; nd.h = box[i].h; nd.col = box[i].layer;
        rgn.push_back(nd);
    }
    for (REdge& e : edge) {
        RgEdge ge{}; ge.srcNode = e.src; ge.dstNode = e.dst;
        ge.srcPin = (uint16_t)e.sOut; ge.dstPin = (uint16_t)e.dIn; ge.resId = e.id; ge.kind = RgEdge::Kind::Raw;
        rge.push_back(ge);
    }
    // one Virtual RgNode at its laid-out spot + an RgEdge per link to the anchor pin, a fan source emitting
    // one per reader. additive for now, the draw still reads tnodes.
    for (TNode& t : tnodes) {
        int vi = (int)rgn.size();
        RgNode nd{}; nd.kind = RgNode::Kind::Virtual; nd.res = t.res;
        nd.label = WGPUStringView{ t.cap, t.cap ? strlen(t.cap) : 0 };
        nd.pos = ImVec2(lnode[t.li].x - t.w * 0.5f, lnode[t.li].y - t.h * 0.5f);
        nd.w = t.w; nd.h = t.h; nd.col = t.col; nd.tint = t.tint;
        rgn.push_back(nd);
        RgEdge::Kind ek = t.res->persistent ? RgEdge::Kind::History : RgEdge::Kind::Raw;
        bool fan = fanBuffers && t.isRead && t.res->imported && t.res->kind == ResourceKind::Buffer;
        if (fan)
            for (int i = 0; i < n; ++i) { int sl = rg_in_slot(box[i].p, t.res->handle.id); if (sl < 0) continue;
                rge.push_back(RgEdge{ vi, i, 0, (uint16_t)sl, t.res->handle.id, RgEdge::Kind::Fanout }); }
        else if (t.isRead) rge.push_back(RgEdge{ vi, t.passBox, 0, (uint16_t)t.pin, t.res->handle.id, ek });
        else               rge.push_back(RgEdge{ t.passBox, vi, (uint16_t)t.pin, 0, t.res->handle.id, ek });
    }

    // reset/centre the view + a name filter that dims non-matching passes
    static char filter[64] = "";
    bool doReset = ImGui::Button("Reset view");
    ImGui::SameLine(); ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##rgfilter", "filter passes...", filter, sizeof filter);
    ImGui::SameLine(); ImGui::Checkbox("virtual nodes", &showVirtual);
    ImGui::SameLine(); ImGui::Checkbox("fan-out buffers", &fanBuffers);   // off -> one virtual node per buffer use site
    ImGui::SameLine(); ImGui::Checkbox("collapse groups", &collapseDefault);   // default state; a group header click overrides
    if (rgPassTotal > n) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.30f, 1), "|  showing %d of %d passes (cap %d)", n, rgPassTotal, kRgDagMax); }
    const bool filterActive = filter[0] != 0;

    // drag left/middle to pan, wheel to zoom, handled at the top before layout. no scrollbar, navigation is
    // panning, so a big graph is not boxed in.
    ImGui::BeginChild("rg_canvas", ImVec2(0, 0), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowFontScale(zoom);   // text scales with the canvas; restored to 1.0 before the overlays
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 winPos = ImGui::GetCursorScreenPos();
    const ImVec2 winSize = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("canvas", ImVec2(winSize.x > 0 ? winSize.x : 1, winSize.y > 0 ? winSize.y : 1));
    const bool canvasHovered = ImGui::IsItemHovered();
    const bool canvasActive = ImGui::IsItemActive();
    s_canvasHovered = canvasHovered; s_winPos = winPos;   // for next frame's wheel zoom (resolved pre-layout)
    bool panned = false;
    if (canvasActive && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))) {
        scrolling.x += io.MouseDelta.x; scrolling.y += io.MouseDelta.y; userMoved = true; panned = true;
    }
    // snap to the graph's top-left until the user pans, riding through the frames where the content region
    // is still settling. reset and a double-click re-arm it.
    if (doReset || (canvasHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))) userMoved = false;
    if (doReset) zoom = 1.0f;   // reset view also returns to the closest zoom
    if (!userMoved && !panned && winSize.x > 1 && winSize.y > 1) {
        const float kCornerPad = 40.0f;
        scrolling.x = kCornerPad - gxMin;
        scrolling.y = kCornerPad - gyMin;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin(winPos.x + scrolling.x, winPos.y + scrolling.y);   // panned top-left; node coords add this

    // faint grid, scrolling with the canvas
    const float kGrid = 48.0f;
    for (float gx = std::fmod(scrolling.x, kGrid); gx < winSize.x; gx += kGrid)
        dl->AddLine(ImVec2(winPos.x + gx, winPos.y), ImVec2(winPos.x + gx, winPos.y + winSize.y), IM_COL32(255, 255, 255, 14));
    for (float gy = std::fmod(scrolling.y, kGrid); gy < winSize.y; gy += kGrid)
        dl->AddLine(ImVec2(winPos.x, winPos.y + gy), ImVec2(winPos.x + winSize.x, winPos.y + gy), IM_COL32(255, 255, 255, 14));

    // ---- group runs: a contiguous run of passes sharing a dotted-name prefix is one logical group.
    // expanded draws a labelled frame behind the members, collapsed draws one synthetic node carrying the
    // external pins. the collapse flag is a tri-state in ImGuiStorage (0 follow-default, 1 open, 2 closed),
    // so it survives the per-frame rebuild and the toolbar can flip every group at once.
    struct GView {
        int gi, gj, depth; WGPUStringView prefix; bool collapsed; ImGuiID key;   // depth 1 = top-level group, >=2 = subgroup
        ImVec2 bb0, bb1, h0, h1;                                    // box rect + header (click) rect, screen
        int nIn, nOut; uint32_t inId[kRgGPinMax], outId[kRgGPinMax];
        ImVec2 inC[kRgGPinMax], outC[kRgGPinMax]; bool inDrawn[kRgGPinMax];
    };
    static std::vector<GView> groups; groups.clear();
    static std::vector<int> nodeGView; nodeGView.assign(gtree.size(), -1);   // gtree node index -> its GView in groups (subgroups), else -1
    int groupOf[kRgDagMax]; for (int i = 0; i < n; ++i) groupOf[i] = -1;
    // the barycentre of the member pins it serves INSIDE and the external pins it serves OUTSIDE. weighing
    // only the inside puts the pin beside its members but leaves the outside to tangle. for a COLLAPSED
    // group the inside term is constant, so the result orders by the outside anchors, which is what you want
    // when the interior is not drawn.
    auto ifaceY = [&](int gi, int gj, uint32_t id, bool write, float fb) -> float {
        float inSum = 0, outSum = 0; int inCnt = 0, outCnt = 0;
        for (int k = 0; k < n; ++k) {
            const bool inside = k >= gi && k < gj;
            // an out-pin is fed by member writers and consumed by outside readers, an in-pin is the mirror
            const int slot = inside ? (write ? rg_out_slot(rgn[k].pass, id) : rg_in_slot(rgn[k].pass, id))
                                    : (write ? rg_in_slot(rgn[k].pass, id)  : rg_out_slot(rgn[k].pass, id));
            if (slot < 0) continue;
            const float y = origin.y + rgn[k].pos.y + kHeaderH + slot * kPinRowH + kPinRowH * 0.5f;
            if (inside) { inSum += y; ++inCnt; } else { outSum += y; ++outCnt; }
        }
        if (inCnt && outCnt) return (inSum / inCnt + outSum / outCnt) * 0.5f;
        if (inCnt)  return inSum / inCnt;
        if (outCnt) return outSum / outCnt;
        return fb;
    };
    // the y anchors one interface pin serves on one side of the border
    static constexpr int kRGAnchor = 16;
    auto ifaceAnchors = [&](int gi, int gj, uint32_t id, bool write, bool inside, float* out) -> int {
        int c = 0;
        for (int k = 0; k < n && c < kRGAnchor; ++k) {
            const bool isIn = k >= gi && k < gj;
            if (isIn != inside) continue;
            const int slot = isIn ? (write ? rg_out_slot(rgn[k].pass, id) : rg_in_slot(rgn[k].pass, id))
                                  : (write ? rg_in_slot(rgn[k].pass, id)  : rg_out_slot(rgn[k].pass, id));
            if (slot < 0) continue;
            out[c++] = origin.y + rgn[k].pos.y + kHeaderH + slot * kPinRowH + kPinRowH * 0.5f;
        }
        return c;
    };
    // order a group's interface pins by counting crossings directly rather than via a barycentre, which is
    // only a proxy and breaks when the two sides disagree: averaging then splits the difference and leaves
    // crossings on both. count, per adjacent pair, how many edge pairs invert either way and swap when that
    // lowers it. pin counts are tiny, so adjacent exchange is plenty and always terminates.
    auto orderIface = [&](int gi, int gj, uint32_t* ids, int cnt, bool write) {
        if (cnt < 2) return;
        static float inY[kRgGPinMax][kRGAnchor], outY[kRgGPinMax][kRGAnchor];
        int inN[kRgGPinMax], outN[kRgGPinMax], perm[kRgGPinMax];
        for (int i = 0; i < cnt; ++i) {
            inN[i]  = ifaceAnchors(gi, gj, ids[i], write, true,  inY[i]);
            outN[i] = ifaceAnchors(gi, gj, ids[i], write, false, outY[i]);
            perm[i] = i;
        }
        auto inv = [&](int p, int q) {   // crossings with p placed above q, both sides
            int c = 0;
            for (int a = 0; a < inN[p]; ++a)  for (int b = 0; b < inN[q]; ++b)  if (inY[p][a]  > inY[q][b])  ++c;
            for (int a = 0; a < outN[p]; ++a) for (int b = 0; b < outN[q]; ++b) if (outY[p][a] > outY[q][b]) ++c;
            return c;
        };
        for (int pass = 0; pass < cnt; ++pass) {
            bool moved = false;
            for (int i = 0; i + 1 < cnt; ++i) {
                const int p = perm[i], q = perm[i + 1];
                if (inv(q, p) < inv(p, q)) { perm[i] = q; perm[i + 1] = p; moved = true; }
            }
            if (!moved) break;
        }
        uint32_t tmp[kRgGPinMax];
        for (int i = 0; i < cnt; ++i) tmp[i] = ids[perm[i]];
        for (int i = 0; i < cnt; ++i) ids[i] = tmp[i];
    };
    // de-overlap a border's pins AND keep the run on that border in one step. members in one row want the
    // same y, which would stack every pin on a point, so enforce a min gap then recentre on the desired mean.
    // the two-sided barycentre can land past the group's span, so the run is SHIFTED into [lo, hi] rather
    // than clamped, which would restack exactly the spacing the gap pass just bought. it compresses only when
    // the band cannot hold cnt pins at `gap`, and then evenly.
    // NOTE: the gap is enforced in ARRAY order, which orderIface already chose to minimise crossings.
    // re-sorting by y would silently undo that whenever the two disagree, the very case orderIface exists for.
    auto fitPins = [&](ImVec2* pins, int cnt, float gap, float lo, float hi) {
        if (cnt <= 0 || hi <= lo) return;
        if (cnt == 1) { pins[0].y = pins[0].y < lo ? lo : (pins[0].y > hi ? hi : pins[0].y); return; }
        float before = 0, after = 0, prev = -1e30f;
        for (int i = 0; i < cnt; ++i) before += pins[i].y;
        for (int i = 0; i < cnt; ++i) { if (pins[i].y < prev + gap) pins[i].y = prev + gap; prev = pins[i].y; after += pins[i].y; }
        for (int i = 0; i < cnt; ++i) pins[i].y += (before - after) / cnt;   // recentre on the desired mean
        const float top = pins[0].y, bot = pins[cnt - 1].y, span = hi - lo;
        if (bot - top > span) {   // band too tight for the ideal gap: even spacing, still never coincident
            const float g2 = span / (cnt - 1);
            for (int i = 0; i < cnt; ++i) pins[i].y = lo + g2 * i;
            return;
        }
        const float shift = top < lo ? lo - top : (bot > hi ? hi - bot : 0.0f);
        if (shift != 0.0f) for (int i = 0; i < cnt; ++i) pins[i].y += shift;
    };
    for (int gi = 0; gi < n;) {
        WGPUStringView pre = group_prefix(box[gi].p->id.name);
        int gj = gi + 1;
        while (gj < n && sv_length(pre) && sv_eq(group_prefix(box[gj].p->id.name), pre)) ++gj;
        if (!(sv_length(pre) && gj - gi >= 2)) { gi = gj; continue; }

        float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
        for (int k = gi; k < gj; ++k) {
            float ax = origin.x + box[k].tl.x, ay = origin.y + box[k].tl.y;
            if (ax < x0) x0 = ax; if (ay < y0) y0 = ay;
            if (ax + box[k].w > x1) x1 = ax + box[k].w; if (ay + box[k].h > y1) y1 = ay + box[k].h;
        }

        GView g{}; g.gi = gi; g.gj = gj; g.depth = 1; g.prefix = pre; g.key = rg_grp_key(pre);
        int st = grpStore->GetInt(g.key, 0);
        g.collapsed = st == 2 ? true : st == 1 ? false : collapseDefault;

        // for collapsed AND expanded groups: a member read produced outside becomes an in-pin, an
        // external/imported/persistent member write an out-pin, and interior resources get no pin.
        rg_group_interface(rg, box, n, gi, gj, g.inId, g.nIn, g.outId, g.nOut);
        orderIface(gi, gj, g.inId, g.nIn, false);
        orderIface(gi, gj, g.outId, g.nOut, true);

        if (!g.collapsed) {
            // a hull border around the member boxes. the cluster-contiguity ordering keeps them a tight
            // block, so the bbox is theirs alone. pad past any nested subgroup border to enclose it.
            ImU32 gcol = group_color(pre);
            ImVec2 a(x0 - kRegionPad, y0 - kRegionPad - kHeaderH), b(x1 + kRegionPad, y1 + kRegionPad);
            dl->AddRectFilled(a, b, rg_with_alpha(gcol, 22), 6.0f);
            dl->AddRect(a, b, gcol, 6.0f, 0, 2.0f);
            char lbl[64]; std::snprintf(lbl, sizeof lbl, "[-] %.*s  x%d", (int)sv_length(pre), pre.data, gj - gi);
            dl->AddText(ImVec2(a.x + 6, a.y + 3), gcol, lbl);
            g.bb0 = a; g.bb1 = b; g.h0 = a; g.h1 = ImVec2(b.x, a.y + kHeaderH);
            // pins on the hull edges, each aligned to the members it serves, so boundary edges run straight
            // across instead of arcing up to a top-aligned row
            const float pinLo = a.y + kHeaderH + kPinRowH * 0.5f, pinHi = b.y - kPinRowH * 0.5f;
            for (int s = 0; s < g.nIn; ++s)  g.inC[s]  = ImVec2(a.x, ifaceY(gi, gj, g.inId[s],  false, a.y + kHeaderH + s * kPinRowH + kPinRowH * 0.5f));
            for (int s = 0; s < g.nOut; ++s) g.outC[s] = ImVec2(b.x, ifaceY(gi, gj, g.outId[s], true,  a.y + kHeaderH + s * kPinRowH + kPinRowH * 0.5f));
            fitPins(g.inC, g.nIn, kPinRowH, pinLo, pinHi); fitPins(g.outC, g.nOut, kPinRowH, pinLo, pinHi);
        }
        else {
            int rows = g.nIn > g.nOut ? g.nIn : g.nOut;
            float needH = kHeaderH + (rows ? rows * kPinRowH : kMinBodyH) + kFooterH;
            // collapse over the FIRST member's slot, already an overlap-free position, not the member bbox,
            // which for a multi-column group would cover the nodes laid out between its members.
            // NOTE(Huerbe): no relayout, so a group with more pins than the anchor's rows grows down and can
            // touch a neighbour. promote collapsed groups to single layout nodes if that ever bites.
            ImVec2 a(origin.x + rgn[gi].pos.x, origin.y + rgn[gi].pos.y);
            ImVec2 b(a.x + kBoxW, a.y + needH);   // == effH[gi], the slot reserved before layout
            g.bb0 = a; g.bb1 = b; g.h0 = a; g.h1 = ImVec2(b.x, a.y + kHeaderH);
            for (int s = 0; s < g.nIn; ++s)  g.inC[s]  = ImVec2(a.x, a.y + kHeaderH + s * kPinRowH + kPinRowH * 0.5f);
            for (int s = 0; s < g.nOut; ++s) g.outC[s] = ImVec2(b.x, a.y + kHeaderH + s * kPinRowH + kPinRowH * 0.5f);
        }

        int idxG = (int)groups.size(); groups.push_back(g);
        for (int k = gi; k < gj; ++k) groupOf[k] = idxG;
        // map this top-level group to its gtree node too, so collOwner-keyed lookups find its compact pin.
        // without it, collapsing a top-level group sends member edges to their own undrawn pins.
        for (int gn = 1; gn < (int)gtree.size(); ++gn) if (gtree[gn].depth == 1 && gtree[gn].gi == gi && gtree[gn].gj == gj) { nodeGView[gn] = idxG; break; }
        gi = gj;
    }

    // every gtree node below the top level becomes a GView too, so the same draw / click / pin passes treat
    // it like a top-level group. nests to any depth.
    // pass 1: rects deepest-first, so a parent's border encloses its child borders with a gap and members
    // never sit flush. pass 2 builds the GViews parent-first.
    static std::vector<ImVec2> subA, subB; subA.assign(gtree.size(), ImVec2(0, 0)); subB.assign(gtree.size(), ImVec2(0, 0));
    const float kSubPad = 11.0f * zoom;   // gap from a subgroup border to its members / nested child borders
    for (int gni = (int)gtree.size() - 1; gni >= 1; --gni) {
        GNode& g2 = gtree[gni];
        if (g2.depth < 2 || !sv_length(g2.prefix)) continue;
        if (collOwner[g2.gi] >= 0 && collOwner[g2.gi] != gni) continue;   // inside a collapsed ancestor -> not drawn
        if (g2.collapsed) {   // compact node over the rep cell (sized compact at layout time via repH)
            uint32_t ii[kRgGPinMax], oo[kRgGPinMax]; int ni = 0, no = 0; rg_group_interface(rg, box, n, g2.gi, g2.gj, ii, ni, oo, no);
            int rows = ni > no ? ni : no;
            ImVec2 a(origin.x + rgn[g2.gi].pos.x, origin.y + rgn[g2.gi].pos.y);
            subA[gni] = a; subB[gni] = ImVec2(a.x + kBoxW, a.y + kHeaderH + (rows ? rows * kPinRowH : kMinBodyH) + kFooterH);
            continue;
        }
        float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
        for (int k = g2.gi; k < g2.gj; ++k) {
            if (collOwner[k] >= 0 && k != gtree[collOwner[k]].gi) continue;   // folded member -> not framed
            float kx = origin.x + rgn[k].pos.x, ky = origin.y + rgn[k].pos.y;
            if (kx < x0) x0 = kx; if (ky < y0) y0 = ky;
            if (kx + rgn[k].w > x1) x1 = kx + rgn[k].w; if (ky + rgn[k].h > y1) y1 = ky + rgn[k].h;
        }
        for (int kid : g2.kids) {   // enclose child subgroup borders (computed already, this pass runs deepest-first)
            if (subB[kid].x < subA[kid].x) continue;
            if (subA[kid].x < x0) x0 = subA[kid].x; if (subA[kid].y < y0) y0 = subA[kid].y;
            if (subB[kid].x > x1) x1 = subB[kid].x; if (subB[kid].y > y1) y1 = subB[kid].y;
        }
        if (x1 < x0) continue;
        subA[gni] = ImVec2(x0 - kSubPad, y0 - kSubPad - kHeaderH); subB[gni] = ImVec2(x1 + kSubPad, y1 + kSubPad);
    }
    for (int gni = 1; gni < (int)gtree.size(); ++gni) {
        GNode& g2 = gtree[gni];
        if (g2.depth < 2 || !sv_length(g2.prefix)) continue;
        if (collOwner[g2.gi] >= 0 && collOwner[g2.gi] != gni) continue;
        if (subB[gni].x < subA[gni].x) continue;   // nothing visible
        GView g{}; g.gi = g2.gi; g.gj = g2.gj; g.depth = g2.depth; g.prefix = g2.prefix;
        g.key = rg_grp_key(g2.prefix); g.collapsed = g2.collapsed;
        rg_group_interface(rg, box, n, g2.gi, g2.gj, g.inId, g.nIn, g.outId, g.nOut);
        orderIface(g2.gi, g2.gj, g.inId, g.nIn, false);
        orderIface(g2.gi, g2.gj, g.outId, g.nOut, true);
        ImVec2 a = subA[gni], b = subB[gni];
        g.bb0 = a; g.bb1 = b; g.h0 = a; g.h1 = ImVec2(b.x, a.y + kHeaderH);
        if (!g.collapsed) {   // expanded: bordered region + label (the compact-node pass draws the collapsed ones)
            ImU32 sc = group_color(g2.prefix);
            dl->AddRect(a, b, sc, 5.0f, 0, 1.5f);
            WGPUStringView sn = g2.prefix; size_t off = 0; for (size_t i = 0; i < sv_length(sn); ++i) if (sn.data[i] == '.') off = i + 1;
            char slbl[48]; std::snprintf(slbl, sizeof slbl, "[-] %.*s x%d", (int)(sv_length(sn) - off), sn.data + off, g2.gj - g2.gi);
            dl->AddText(ImVec2(a.x + 4, a.y + 1), sc, slbl);
        }
        // expanded aligns pins to the members they serve so edges run across, collapsed top-aligns them
        for (int s = 0; s < g.nIn; ++s)  g.inC[s]  = ImVec2(a.x, g.collapsed ? a.y + kHeaderH + s * kPinRowH + kPinRowH * 0.5f : ifaceY(g2.gi, g2.gj, g.inId[s],  false, a.y + kHeaderH + s * kPinRowH + kPinRowH * 0.5f));
        for (int s = 0; s < g.nOut; ++s) g.outC[s] = ImVec2(b.x, g.collapsed ? a.y + kHeaderH + s * kPinRowH + kPinRowH * 0.5f : ifaceY(g2.gi, g2.gj, g.outId[s], true,  a.y + kHeaderH + s * kPinRowH + kPinRowH * 0.5f));
        if (!g.collapsed) {
            const float pinLo = a.y + kHeaderH + kPinRowH * 0.5f, pinHi = b.y - kPinRowH * 0.5f;
            fitPins(g.inC, g.nIn, kPinRowH, pinLo, pinHi); fitPins(g.outC, g.nOut, kPinRowH, pinLo, pinHi);
        }
        nodeGView[gni] = (int)groups.size();
        groups.push_back(g);
    }

    // pin centres, screen space. reads fill left slots in encounter order, writes fill right the same way.
    auto inPin = [&](int b, int slot) { return ImVec2(origin.x + rgn[b].pos.x,
        origin.y + rgn[b].pos.y + kHeaderH + slot * kPinRowH + kPinRowH * 0.5f); };
    auto outPin = [&](int b, int slot) { return ImVec2(origin.x + rgn[b].pos.x + rgn[b].w,
        origin.y + rgn[b].pos.y + kHeaderH + slot * kPinRowH + kPinRowH * 0.5f); };
    // would the direct a->b cubic clip a box other than its two? sampled, an exact intersection is overkill.
    auto directClear = [&](ImVec2 a, ImVec2 b, int src, int dst) {
        const float dx = rg_edge_tangent(a, b);
        const ImVec2 c1(a.x + dx, a.y), c2(b.x - dx, b.y);
        for (int s = 1; s < 16; ++s) {
            const float t = s / 16.0f, u = 1 - t;
            const float w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
            const ImVec2 q(w0 * a.x + w1 * c1.x + w2 * c2.x + w3 * b.x, w0 * a.y + w1 * c1.y + w2 * c2.y + w3 * b.y);
            for (int i = 0; i < n; ++i) {
                if (i == src || i == dst || drawHidden[i]) continue;
                const float bx = origin.x + rgn[i].pos.x, by = origin.y + rgn[i].pos.y;
                if (q.x >= bx - 2 && q.x <= bx + rgn[i].w + 2 && q.y >= by - 2 && q.y <= by + rgn[i].h + 2) return false;
            }
        }
        return true;
    };
    // screen-space polyline of an edge, shared by hit-test + draw. the lane chain only exists to stop an
    // edge hiding behind a box, and a relaxed lane is what makes an edge detour, so take the lanes only when
    // the direct run actually hits something.
    auto edgePoints = [&](const REdge& e, ImVec2* pts) -> int {
        const ImVec2 sp = outPin(e.src, e.sOut), dp = inPin(e.dst, e.dIn);
        if (e.chainN == 0 || directClear(sp, dp, e.src, e.dst)) { pts[0] = sp; pts[1] = dp; return 2; }
        int np = 0;
        pts[np++] = sp;
        for (int t = 0; t < e.chainN; ++t) { float lx = origin.x + lnode[e.chain[t]].x, ly = origin.y + lnode[e.chain[t]].y; pts[np++] = ImVec2(lx, ly); pts[np++] = ImVec2(lx + kBoxW, ly); }
        pts[np++] = dp;
        return np;
    };
    bool matchBox[kRgDagMax];
    for (int i = 0; i < n; ++i) matchBox[i] = rg_name_has(rgn[i].pass->id.name, filter);

    // ---- hovered pin. manual rect test, pins are small and overlap the box button.
    int hovB = -1, hovSlot = -1; bool hovWrite = false; uint32_t hovId = 0, hovMip = 0, hovLayer = 0; AccessType hovType{};
    if (canvasHovered && !canvasActive) {
        for (int i = 0; i < n && hovB < 0; ++i) {
            if (drawHidden[i]) continue;   // hidden inside a collapsed group
            int inS = 0, outS = 0; PassNode* p = rgn[i].pass;
            for (uint32_t k = 0; k < p->accessCount && hovB < 0; ++k) {
                const ResourceAccess& acc = p->accesses[k];
                if (rg_access_reads(acc)) {   // input pin (left)
                    ImVec2 c = inPin(i, inS);
                    if (ImGui::IsMouseHoveringRect(ImVec2(c.x - kPinHit, c.y - kPinHit), ImVec2(c.x + kPinHit, c.y + kPinHit)))
                        { hovB = i; hovSlot = inS; hovWrite = false; hovId = acc.handle.id; hovType = acc.type; hovMip = acc.baseMip; hovLayer = acc.baseLayer; }
                    ++inS;
                }
                if (hovB < 0 && access_is_write(acc.type)) {   // output pin (right)
                    ImVec2 c = outPin(i, outS);
                    if (ImGui::IsMouseHoveringRect(ImVec2(c.x - kPinHit, c.y - kPinHit), ImVec2(c.x + kPinHit, c.y + kPinHit)))
                        { hovB = i; hovSlot = outS; hovWrite = true; hovId = acc.handle.id; hovType = acc.type; hovMip = acc.baseMip; hovLayer = acc.baseLayer; }
                    ++outS;
                }
            }
        }
    }
    // ---- group border pins, hovered like a member's pin so cone + tooltip + lock work. map the border pin
    // to a representative member, then reuse the hovered-pin path below. the per-pass test above already
    // catches expanded member pins, this adds the ones on the region edge.
    if (hovB < 0 && canvasHovered && !canvasActive)
        for (GView& g : groups) {
            if (hovB >= 0) continue;
            // both collapsed and expanded groups carry selectable interface pins
            for (int s = 0; s < g.nIn && hovB < 0; ++s) {
                ImVec2 c = g.inC[s];
                if (!ImGui::IsMouseHoveringRect(ImVec2(c.x - kPinHit, c.y - kPinHit), ImVec2(c.x + kPinHit, c.y + kPinHit))) continue;
                for (int k = g.gi; k < g.gj && hovB < 0; ++k) { PassNode* mp = rgn[k].pass;
                    for (uint32_t ai = 0; ai < mp->accessCount; ++ai)
                        if (mp->accesses[ai].handle.id == g.inId[s] && rg_access_reads(mp->accesses[ai]))
                            { hovB = k; hovWrite = false; hovId = g.inId[s]; hovSlot = rg_in_slot(mp, g.inId[s]); hovType = mp->accesses[ai].type; hovMip = mp->accesses[ai].baseMip; hovLayer = mp->accesses[ai].baseLayer; break; }
                }
            }
            for (int s = 0; s < g.nOut && hovB < 0; ++s) {
                ImVec2 c = g.outC[s];
                if (!ImGui::IsMouseHoveringRect(ImVec2(c.x - kPinHit, c.y - kPinHit), ImVec2(c.x + kPinHit, c.y + kPinHit))) continue;
                for (int k = g.gi; k < g.gj && hovB < 0; ++k) { PassNode* mp = rgn[k].pass;
                    for (uint32_t ai = 0; ai < mp->accessCount; ++ai)
                        if (mp->accesses[ai].handle.id == g.outId[s] && access_is_write(mp->accesses[ai].type))
                            { hovB = k; hovWrite = true; hovId = g.outId[s]; hovSlot = rg_out_slot(mp, g.outId[s]); hovType = mp->accesses[ai].type; hovMip = mp->accesses[ai].baseMip; hovLayer = mp->accesses[ai].baseLayer; break; }
                }
            }
        }

    // only for the fallback reads/writes tooltip when no pin caught the mouse
    int hovBox = -1;
    if (hovB < 0 && canvasHovered && !canvasActive)
        for (int i = 0; i < n; ++i) {
            if (drawHidden[i]) continue;   // hidden inside a collapsed group
            ImVec2 tl(origin.x + rgn[i].pos.x, origin.y + rgn[i].pos.y);
            if (ImGui::IsMouseHoveringRect(tl, ImVec2(tl.x + rgn[i].w, tl.y + rgn[i].h))) { hovBox = i; break; }
        }

    // ---- click a pin to LOCK its cone, click empty canvas to clear
    static int lockB = -1, lockSlot = -1; static bool lockWrite = false; static uint32_t lockId = 0;
    static bool pressed = false; static ImVec2 pressAt;
    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { pressed = true; pressAt = io.MousePos; }
    if (pressed && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        pressed = false;
        float mdx = io.MousePos.x - pressAt.x, mdy = io.MousePos.y - pressAt.y;
        if (mdx * mdx + mdy * mdy < 16) {   // a click, not a pan
            int hitG = -1;   // a group header click toggles that group's collapse state, overriding the default
            for (int gI = 0; gI < (int)groups.size(); ++gI)
                if (io.MousePos.x >= groups[gI].h0.x && io.MousePos.x <= groups[gI].h1.x &&
                    io.MousePos.y >= groups[gI].h0.y && io.MousePos.y <= groups[gI].h1.y) hitG = gI;
            if (hitG >= 0) grpStore->SetInt(groups[hitG].key, groups[hitG].collapsed ? 1 : 2);   // flip effective state
            else if (hovB >= 0) {
                if (lockB == hovB && lockWrite == hovWrite && lockSlot == hovSlot) lockB = -1;   // toggle off
                else { lockB = hovB; lockWrite = hovWrite; lockSlot = hovSlot; lockId = hovId; }
            }
            else if (hovBox >= 0) {   // a pass-body click selects the whole pass (lockSlot -1 / lockId 0 = no pin)
                if (lockB == hovBox && lockSlot == -1) lockB = -1;   // toggle off
                else { lockB = hovBox; lockWrite = false; lockSlot = -1; lockId = 0; }
            }
            else lockB = -1;
        }
    }
    if (lockB >= n) lockB = -1;   // stale lock after the graph shrank

    // slot of resource `id` among a group's interface pins, -1 = none
    auto gpin_slot = [](const uint32_t* ids, int cnt, uint32_t id) { for (int i = 0; i < cnt; ++i) if (ids[i] == id) return i; return -1; };
    // the group's compact-node pin when the member sits in a collapsed group, else the member's own pin
    auto memberOut = [&](int k, uint32_t id) -> ImVec2 {
        if (collOwner[k] >= 0) { int gv = nodeGView[collOwner[k]]; if (gv >= 0) { int sl = gpin_slot(groups[gv].outId, groups[gv].nOut, id); if (sl >= 0) return groups[gv].outC[sl]; } }
        int ss = rg_out_slot(rgn[k].pass, id); return outPin(k, ss >= 0 ? ss : 0);
    };
    auto memberIn = [&](int k, uint32_t id) -> ImVec2 {
        if (collOwner[k] >= 0) { int gv = nodeGView[collOwner[k]]; if (gv >= 0) { int sl = gpin_slot(groups[gv].inId, groups[gv].nIn, id); if (sl >= 0) return groups[gv].inC[sl]; } }
        int ds = rg_in_slot(rgn[k].pass, id); return inPin(k, ds >= 0 ? ds : 0);
    };
    // terminals + the lane chain + any crossed group interface pins, ordered left-to-right by x, which also
    // nests correctly. shared by hit-test and draw so a rerouted edge is hoverable where it is drawn.
    // caller's wp must hold >= 3*kRgDagMax points.
    auto buildEdgePath = [&](const REdge& e, ImVec2* wp) -> int {
        ImVec2 sp = collOwner[e.src] >= 0 ? memberOut(e.src, e.id) : outPin(e.src, e.sOut);
        ImVec2 dp = collOwner[e.dst] >= 0 ? memberIn(e.dst, e.id)  : inPin(e.dst, e.dIn);
        int exitG[kRgGPinMax], entryG[kRgGPinMax], nex = 0, nen = 0;
        for (int gx = 0; gx < (int)groups.size(); ++gx) {
            GView& g = groups[gx]; if (g.collapsed) continue;
            bool hS = g.gi <= e.src && e.src < g.gj, hD = g.gi <= e.dst && e.dst < g.gj;
            if (hS && !hD && gpin_slot(g.outId, g.nOut, e.id) >= 0 && nex < kRgGPinMax) exitG[nex++] = gx;
            if (hD && !hS && gpin_slot(g.inId,  g.nIn,  e.id) >= 0 && nen < kRgGPinMax) entryG[nen++] = gx;
        }
        if (nex == 0 && nen == 0 && collOwner[e.src] < 0 && collOwner[e.dst] < 0) return edgePoints(e, wp);   // plain edge
        // the border pins are required waypoints, the lane chain is not, it only dodges boxes. taking the
        // lanes unconditionally makes an already-clear run climb to the lane's y and read as a detour, so
        // build the lane-free path first and keep it when every span is clear.
        auto assemble = [&](bool withLanes) -> int {
            int nw2 = 0; wp[nw2++] = sp;
            if (withLanes)
                for (int t = 0; t < e.chainN; ++t) {
                    float lx = origin.x + lnode[e.chain[t]].x, ly = origin.y + lnode[e.chain[t]].y;
                    wp[nw2++] = ImVec2(lx, ly); wp[nw2++] = ImVec2(lx + kBoxW, ly);
                }
            for (int i2 = 0; i2 < nex; ++i2) { GView& g = groups[exitG[i2]];  wp[nw2++] = g.outC[gpin_slot(g.outId, g.nOut, e.id)]; }
            for (int i2 = 0; i2 < nen; ++i2) { GView& g = groups[entryG[i2]]; wp[nw2++] = g.inC [gpin_slot(g.inId,  g.nIn,  e.id)]; }
            wp[nw2++] = dp;
            for (int a2 = 1; a2 < nw2; ++a2) { ImVec2 v = wp[a2]; int b2 = a2 - 1; while (b2 >= 0 && wp[b2].x > v.x) { wp[b2 + 1] = wp[b2]; --b2; } wp[b2 + 1] = v; }
            return nw2;
        };
        int nw = assemble(false);
        bool clear = true;
        for (int t = 0; t + 1 < nw && clear; ++t) clear = directClear(wp[t], wp[t + 1], e.src, e.dst);
        if (!clear && e.chainN) nw = assemble(true);
        return nw;
    };

    // ---- hovered edge, only when not over a pin or box
    int hovEdge = -1;
    if (hovB < 0 && hovBox < 0 && canvasHovered && !canvasActive) {
        float best = 6.0f * 6.0f;
        for (int ei = 0; ei < (int)edge.size(); ++ei) {
            if (collOwner[edge[ei].src] >= 0 && collOwner[edge[ei].src] == collOwner[edge[ei].dst]) continue;   // vanished
            ImVec2 pts[3 * kRgDagMax]; int np = buildEdgePath(edge[ei], pts);
            for (int t = 0; t + 1 < np; ++t) {
                ImVec2 a2 = pts[t], b2 = pts[t + 1]; float dx = rg_edge_tangent(a2, b2);
                ImVec2 c1(a2.x + dx, a2.y), c2(b2.x - dx, b2.y), prev = a2;
                for (int s2 = 1; s2 <= 10; ++s2) {   // sample the cubic
                    float tt = s2 / 10.0f, u = 1 - tt, w0 = u * u * u, w1 = 3 * u * u * tt, w2 = 3 * u * tt * tt, w3 = tt * tt * tt;
                    ImVec2 q(w0 * a2.x + w1 * c1.x + w2 * c2.x + w3 * b2.x, w0 * a2.y + w1 * c1.y + w2 * c2.y + w3 * b2.y);
                    float d2 = rg_seg_d2(io.MousePos, prev, q); if (d2 < best) { best = d2; hovEdge = ei; }
                    prev = q;
                }
            }
        }
    }

    // ---- cones, seeded from the hovered pin, else the locked one. upstream is the producers needed to
    // make the resource, and an output pin also marks downstream.
    int coneB = hovB >= 0 ? hovB : lockB;
    bool coneWrite = hovB >= 0 ? hovWrite : lockWrite;
    uint32_t coneId = hovB >= 0 ? hovId : lockId;
    const bool conePass = hovB < 0 && lockB >= 0 && lockSlot == -1;   // whole-pass selection: both cones from the pass
    bool inCone[kRgDagMax] = {}, downCone[kRgDagMax] = {}; bool coneActive = false;
    if (coneB >= 0) {
        int seed = (coneWrite || conePass) ? coneB : rg_producer_of(box, n, box[coneB].p, coneId);
        if (seed >= 0) { rg_mark_cone(box, n, seed, inCone); inCone[coneB] = true; coneActive = true; }   // include the hovered node so its immediate edge lights
        if (coneActive && (coneWrite || conePass)) {   // downstream descendants over the data edges
            int st[kRgDagMax], sp = 0; st[sp++] = coneB;
            while (sp) { int u = st[--sp]; for (REdge& e : edge) if (e.src == u && !downCone[e.dst]) { downCone[e.dst] = true; st[sp++] = e.dst; } }
        }
    }
    auto fout  = [&](int i) { return filterActive && !matchBox[i]; };
    auto isDim = [&](int i) { return fout(i) || (coneActive && !inCone[i] && !downCone[i] && !(coneWrite && i == coneB)); };
    auto inUp  = [&](int i) { return coneActive && inCone[i] && !fout(i); };
    auto inDn  = [&](int i) { return coneActive && (downCone[i] || (coneWrite && i == coneB)) && !fout(i); };
    // by cone state: gold upstream, teal down, grey normal, faint dim. shared by the top-level and interior
    // edge loops, so an edge inside a group reads the same as one outside.
    auto edgeCol = [&](int s, int d, float* th) {
        bool eup = inUp(s) && inUp(d), edn = inDn(s) && inDn(d);
        if (eup)                          { if (th) *th = 2.5f; return IM_COL32(245, 222, 120, 235); }
        if (edn)                          { if (th) *th = 2.5f; return IM_COL32(120, 222, 180, 235); }
        if (isDim(s) || isDim(d))         { if (th) *th = 2.0f; return IM_COL32(150, 150, 150, 34);  }
                                            if (th) *th = 2.0f; return IM_COL32(170, 170, 170, 200);
    };

    // ---- data edges. an edge interior to a collapsed group vanishes, one crossing a border threads through
    // that group's interface pin, everything else routes through its dummy waypoints.
    for (int ei = 0; ei < (int)edge.size(); ++ei) {
        REdge& e = edge[ei];
        if (collOwner[e.src] >= 0 && collOwner[e.src] == collOwner[e.dst]) continue;   // interior to one collapsed group (any level) -> vanishes
        ImU32 col; float th;
        if (ei == hovEdge) { col = IM_COL32(255, 255, 255, 255); th = 3.0f; }
        else col = edgeCol(e.src, e.dst, &th);
        ImVec2 wp[3 * kRgDagMax]; int nw = buildEdgePath(e, wp);
        for (int t = 0; t + 1 < nw; ++t) { ImVec2 a2 = wp[t], b2 = wp[t + 1]; float dx = rg_edge_tangent(a2, b2); dl->AddBezierCubic(a2, ImVec2(a2.x + dx, a2.y), ImVec2(b2.x - dx, b2.y), b2, col, th); }
    }

    // no interior group edges or subgroup stubs: an expanded group's members are first-class boxes, so their
    // intra-group flow is ordinary edge[] entries drawn above.

    // ---- boxes
    for (int i = 0; i < n; ++i) {
        if (drawHidden[i]) continue;   // hidden inside a collapsed group node (drawn separately below)
        ImVec2 tl(origin.x + rgn[i].pos.x, origin.y + rgn[i].pos.y), br(tl.x + rgn[i].w, tl.y + rgn[i].h);
        bool dim = isDim(i), up = inUp(i), dn = inDn(i);

        ImU32 fill = rg_kind_color(rgn[i].pass->kind);
        dl->AddRectFilled(tl, br, dim ? rg_with_alpha(fill, 55) : fill, 5.0f);
        ImU32 edgec = up ? IM_COL32(255, 255, 255, 255) : dn ? IM_COL32(120, 222, 180, 255) : dim ? IM_COL32(40, 40, 40, 120) : IM_COL32(20, 20, 20, 255);
        dl->AddRect(tl, br, edgec, 5.0f, 0, (up || dn) ? 2.5f : 1.0f);
        if (lockB == i && lockSlot == -1)   // whole-pass selection (body click): blue selection ring
            dl->AddRect(ImVec2(tl.x - 2, tl.y - 2), ImVec2(br.x + 2, br.y + 2), IM_COL32(90, 200, 230, 255), 6.0f, 0, 2.0f);
        dl->AddLine(ImVec2(tl.x, tl.y + kHeaderH), ImVec2(br.x, tl.y + kHeaderH), IM_COL32(255, 255, 255, dim ? 18 : 40));

        // imported writers only. history writers are sinks to the culler but not graph outputs.
        if (rg_pass_is_sink(rg, rgn[i].pass))
            for (int e = 3; e >= 1; --e)
                dl->AddRect(ImVec2(tl.x - e, tl.y - e), ImVec2(br.x + e, br.y + e), IM_COL32(255, 100, 0, dim ? 80 : 200), 6.0f, 0, 1.0f);

        WGPUStringView nm = rgn[i].pass->id.name;
        char head[96];
        std::snprintf(head, sizeof head, "P%d  %.*s", i, (int)nm.length, nm.data ? nm.data : "");
        dl->PushClipRect(tl, ImVec2(br.x - 4, br.y), true);
        dl->AddText(ImVec2(tl.x + 7, tl.y + 4), dim ? IM_COL32(190, 190, 190, 120) : IM_COL32(255, 255, 255, 255), head);
        dl->PopClipRect();

        const char* kn = rg_kind_name(rgn[i].pass->kind);
        ImVec2 ks = ImGui::CalcTextSize(kn);
        dl->AddText(ImVec2(tl.x + (rgn[i].w - ks.x) * 0.5f, br.y - kFooterH + 1),
            dim ? IM_COL32(200, 200, 200, 110) : IM_COL32(225, 225, 225, 220), kn);
    }

    // ---- pins + resource labels
    for (int i = 0; i < n; ++i) {
        if (drawHidden[i]) continue;   // hidden inside a collapsed group node (drawn separately below)
        PassNode* p = rgn[i].pass; bool dim = isDim(i);
        const float mid = origin.x + rgn[i].pos.x + rgn[i].w * 0.5f;
        int inS = 0, outS = 0;
        for (uint32_t k = 0; k < p->accessCount; ++k) {
            const ResourceAccess& acc = p->accesses[k];
            ResourceNode* r = find_node(rg, acc.handle);
            WGPUStringView rn = r ? r->id.name : WGPUStringView{};
            char lbl[48]; std::snprintf(lbl, sizeof lbl, "%.*s", (int)rn.length, rn.data ? rn.data : "?");
            const ImU32 lc = dim ? IM_COL32(190, 190, 190, 110) : IM_COL32(230, 230, 230, 255);
            const bool isBuf = r && r->kind == ResourceKind::Buffer;   // square pin vs round (texture)

            if (rg_access_reads(acc)) {   // input pin (left); hollow if no in-graph producer (external input)
                int slot = inS++; ImVec2 c = inPin(i, slot);
                ImU32 base = dim ? rg_with_alpha(kRGRead, 70) : kRGRead;
                rg_draw_pin(dl, c, kPinR, base, rg_producer_of(box, n, p, acc.handle.id) >= 0, isBuf);
                if (lockB == i && !lockWrite && slot == lockSlot) dl->AddCircle(c, kPinR + 3.0f, IM_COL32(90, 200, 230, 255), 16, 2.0f);
                if (i == hovB && !hovWrite && slot == hovSlot) dl->AddCircle(c, kPinR + 3.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
                const float lx = c.x + kPinR + 3;
                char t[48]; std::snprintf(t, sizeof t, "%s", lbl);
                rg_fit_text(t, mid - lx);   // reads own the left half of the row
                ImVec2 ts = ImGui::CalcTextSize(t);
                dl->PushClipRect(ImVec2(lx, c.y - kPinRowH * 0.5f), ImVec2(mid, c.y + kPinRowH * 0.5f), true);
                dl->AddText(ImVec2(lx, c.y - ts.y * 0.5f), lc, t);
                dl->PopClipRect();
            }
            if (access_is_write(acc.type)) {   // output pin (right)
                int slot = outS++; ImVec2 c = outPin(i, slot);
                ImU32 base = dim ? rg_with_alpha(kRGWrite, 70) : kRGWrite;
                rg_draw_pin(dl, c, kPinR, base, true, isBuf);
                if (lockB == i && lockWrite && slot == lockSlot) dl->AddCircle(c, kPinR + 3.0f, IM_COL32(90, 200, 230, 255), 16, 2.0f);
                if (i == hovB && hovWrite && slot == hovSlot) dl->AddCircle(c, kPinR + 3.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
                const float rx = c.x - kPinR - 3;
                char t[48]; std::snprintf(t, sizeof t, "%s", lbl);
                rg_fit_text(t, rx - mid);   // writes own the right half; fitted so the name keeps its head
                ImVec2 ts = ImGui::CalcTextSize(t);
                dl->PushClipRect(ImVec2(mid, c.y - kPinRowH * 0.5f), ImVec2(rx, c.y + kPinRowH * 0.5f), true);
                dl->AddText(ImVec2(rx - ts.x, c.y - ts.y * 0.5f), lc, t);
                dl->PopClipRect();
            }
        }
    }

    // ---- one synthetic box per collapsed group, drawn over the edges like a real pass, carrying the
    // external read pins left and write pins right
    for (GView& g : groups) {
        if (!g.collapsed) continue;
        ImVec2 a = g.bb0, b = g.bb1;
        const float mid = (a.x + b.x) * 0.5f;
        dl->AddRectFilled(a, b, IM_COL32(70, 70, 96, 255), 5.0f);
        dl->AddRect(a, b, IM_COL32(20, 20, 20, 255), 5.0f, 0, 1.0f);
        dl->AddLine(ImVec2(a.x, a.y + kHeaderH), ImVec2(b.x, a.y + kHeaderH), IM_COL32(255, 255, 255, 40));
        char head[64]; std::snprintf(head, sizeof head, "[+] %.*s  x%d", (int)sv_length(g.prefix), g.prefix.data, g.gj - g.gi);
        dl->PushClipRect(a, ImVec2(b.x - 4, b.y), true);
        dl->AddText(ImVec2(a.x + 7, a.y + 4), IM_COL32(255, 255, 255, 255), head);
        dl->PopClipRect();
        for (int s = 0; s < g.nIn; ++s) {
            ResourceNode* r = find_node(rg, { g.inId[s] });
            WGPUStringView rn = r ? r->id.name : WGPUStringView{};
            bool buf = r && r->kind == ResourceKind::Buffer;
            bool prod = false; for (int j = 0; j < n; ++j) if (rg_pass_writes(box[j].p, g.inId[s])) prod = true;
            rg_draw_pin(dl, g.inC[s], kPinR, kRGRead, prod, buf);   // hollow = external input (no in-graph producer)
            if (hovB >= g.gi && hovB < g.gj && !hovWrite && hovId == g.inId[s]) dl->AddCircle(g.inC[s], kPinR + 3.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            if (lockB >= g.gi && lockB < g.gj && !lockWrite && lockId == g.inId[s]) dl->AddCircle(g.inC[s], kPinR + 3.0f, IM_COL32(90, 200, 230, 255), 16, 2.0f);
            char lbl[48]; std::snprintf(lbl, sizeof lbl, "%.*s", (int)rn.length, rn.data ? rn.data : "?");
            ImVec2 ls = ImGui::CalcTextSize(lbl);
            dl->PushClipRect(ImVec2(g.inC[s].x + kPinR + 3, g.inC[s].y - kPinRowH * 0.5f), ImVec2(mid, g.inC[s].y + kPinRowH * 0.5f), true);
            dl->AddText(ImVec2(g.inC[s].x + kPinR + 3, g.inC[s].y - ls.y * 0.5f), IM_COL32(230, 230, 230, 255), lbl);
            dl->PopClipRect();
        }
        for (int s = 0; s < g.nOut; ++s) {
            ResourceNode* r = find_node(rg, { g.outId[s] });
            WGPUStringView rn = r ? r->id.name : WGPUStringView{};
            bool buf = r && r->kind == ResourceKind::Buffer;
            rg_draw_pin(dl, g.outC[s], kPinR, kRGWrite, true, buf);
            if (hovB >= g.gi && hovB < g.gj && hovWrite && hovId == g.outId[s]) dl->AddCircle(g.outC[s], kPinR + 3.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            if (lockB >= g.gi && lockB < g.gj && lockWrite && lockId == g.outId[s]) dl->AddCircle(g.outC[s], kPinR + 3.0f, IM_COL32(90, 200, 230, 255), 16, 2.0f);
            char lbl[48]; std::snprintf(lbl, sizeof lbl, "%.*s", (int)rn.length, rn.data ? rn.data : "?");
            ImVec2 ls = ImGui::CalcTextSize(lbl);
            dl->PushClipRect(ImVec2(mid, g.outC[s].y - kPinRowH * 0.5f), ImVec2(g.outC[s].x - kPinR - 3, g.outC[s].y + kPinRowH * 0.5f), true);
            dl->AddText(ImVec2(g.outC[s].x - kPinR - 3 - ls.x, g.outC[s].y - ls.y * 0.5f), IM_COL32(230, 230, 230, 255), lbl);
            dl->PopClipRect();
        }
    }

    // the dots on the hull edges that every boundary edge threads through. a ring lights when a member pin
    // for the same resource is hovered or locked.
    for (GView& g : groups) {
        if (g.collapsed) continue;
        for (int s = 0; s < g.nIn; ++s) {
            ResourceNode* r = find_node(rg, { g.inId[s] }); bool buf = r && r->kind == ResourceKind::Buffer;
            bool prod = false; for (int j = 0; j < n; ++j) if (rg_pass_writes(box[j].p, g.inId[s])) { prod = true; break; }
            rg_draw_pin(dl, g.inC[s], kPinR, kRGRead, prod, buf);
            if (hovB  >= g.gi && hovB  < g.gj && !hovWrite  && hovId  == g.inId[s]) dl->AddCircle(g.inC[s], kPinR + 3.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            if (lockB >= g.gi && lockB < g.gj && !lockWrite && lockId == g.inId[s]) dl->AddCircle(g.inC[s], kPinR + 3.0f, IM_COL32(90, 200, 230, 255), 16, 2.0f);
        }
        for (int s = 0; s < g.nOut; ++s) {
            ResourceNode* r = find_node(rg, { g.outId[s] }); bool buf = r && r->kind == ResourceKind::Buffer;
            rg_draw_pin(dl, g.outC[s], kPinR, kRGWrite, true, buf);
            if (hovB  >= g.gi && hovB  < g.gj && hovWrite  && hovId  == g.outId[s]) dl->AddCircle(g.outC[s], kPinR + 3.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            if (lockB >= g.gi && lockB < g.gj && lockWrite && lockId == g.outId[s]) dl->AddCircle(g.outC[s], kPinR + 3.0f, IM_COL32(90, 200, 230, 255), 16, 2.0f);
        }
    }

    // ---- virtual endpoint nodes + dashed links, from the IR mirror. a source feeds a pass input pin from
    // the left, a sink is fed by an output pin, an imported buffer fans one source to all readers.
    // this resolves the pin position on a collapsed group, or the box edge at `y` when the resource crosses
    // the boundary without a pin, e.g. a history write consumed only next frame.
    auto grpPin = [&](GView& g, uint32_t id, bool write, float y) -> ImVec2 {
        int sl = gpin_slot(write ? g.outId : g.inId, write ? g.nOut : g.nIn, id);
        if (sl >= 0) return write ? g.outC[sl] : g.inC[sl];
        return ImVec2(write ? g.bb1.x : g.bb0.x, y);
    };
    // the border pins between member pe and the outside, so a virtual link threads through the same pins the
    // data edges do. fills outermost-first, returns the count.
    auto memberPins = [&](int pe, uint32_t id, bool write, ImVec2* out) -> int {
        int idx[kRgGPinMax], ni = 0;
        for (int gx = 0; gx < (int)groups.size(); ++gx) {
            GView& g = groups[gx]; if (g.collapsed) continue;
            if (!(g.gi <= pe && pe < g.gj)) continue;
            if (gpin_slot(write ? g.outId : g.inId, write ? g.nOut : g.nIn, id) < 0) continue;
            if (ni < kRgGPinMax) idx[ni++] = gx;
        }
        for (int a = 1; a < ni; ++a) { int v = idx[a], b = a - 1; while (b >= 0 && groups[idx[b]].depth > groups[v].depth) { idx[b + 1] = idx[b]; --b; } idx[b + 1] = v; }
        for (int i = 0; i < ni; ++i) { GView& g = groups[idx[i]]; out[i] = write ? g.outC[gpin_slot(g.outId, g.nOut, id)] : g.inC[gpin_slot(g.inId, g.nIn, id)]; }
        return ni;
    };
    static std::vector<char> vLive; vLive.assign(rgn.size(), 0);
    for (RgEdge& ge : rge) {
        bool srcV = ge.srcNode >= n, dstV = ge.dstNode >= n;
        if (!srcV && !dstV) continue;                 // pass<->pass edge: drawn earlier
        int vi = srcV ? ge.srcNode : ge.dstNode, pe = srcV ? ge.dstNode : ge.srcNode;   // virtual node, anchor pass
        bool faint = ge.kind == RgEdge::Kind::Fanout;   // imported-buffer fan: one faint edge per reader
        if (fout(pe) || (faint && drawHidden[pe])) continue;
        RgNode& vn = rgn[vi];
        ImU32 tint = isDim(pe) ? rg_with_alpha(vn.tint, 40) : (faint ? rg_with_alpha(vn.tint, 90) : vn.tint);   // cone-dim with anchor
        float vcy = origin.y + vn.pos.y + vn.h * 0.5f;
        // thread the dashed link through the same border pins as a data edge. collapsed groups terminate at
        // the compact pin.
        ImVec2 wp[2 + kRgGPinMax]; int nw = 0; ImVec2 gp[kRgGPinMax];
        if (srcV) {   // read: node right edge -> reader input pin
            wp[nw++] = ImVec2(origin.x + vn.pos.x + vn.w, vcy);
            if (groupOf[pe] >= 0 && groups[groupOf[pe]].collapsed) {   // collapsed top-level group: its compact in-pin
                GView& g = groups[groupOf[pe]];
                if (faint) { int s = gpin_slot(g.inId, g.nIn, ge.resId); if (s < 0 || g.inDrawn[s]) continue; g.inDrawn[s] = true; wp[nw++] = g.inC[s]; }
                else wp[nw++] = grpPin(g, ge.resId, false, wp[0].y);
            }
            else if (collOwner[pe] >= 0) wp[nw++] = memberIn(pe, ge.resId);   // folded into a collapsed subgroup
            else {                                                            // expanded/ungrouped: thread group pins -> member
                int ng = memberPins(pe, ge.resId, false, gp);
                for (int i = 0; i < ng; ++i) wp[nw++] = gp[i];
                wp[nw++] = inPin(pe, ge.dstPin);
            }
        }
        else {        // write: writer output pin -> node left edge
            ImVec2 qq(origin.x + vn.pos.x, vcy);
            if (groupOf[pe] >= 0 && groups[groupOf[pe]].collapsed) wp[nw++] = grpPin(groups[groupOf[pe]], ge.resId, true, qq.y);
            else if (collOwner[pe] >= 0) wp[nw++] = memberOut(pe, ge.resId);
            else { wp[nw++] = outPin(pe, ge.srcPin); int ng = memberPins(pe, ge.resId, true, gp); for (int i = ng - 1; i >= 0; --i) wp[nw++] = gp[i]; }
            wp[nw++] = qq;
        }
        vLive[vi] = 1;
        for (int t = 0; t + 1 < nw; ++t) { ImVec2 a2 = wp[t], b2 = wp[t + 1]; float dx = rg_edge_tangent(a2, b2); rg_dashed_cubic(dl, a2, ImVec2(a2.x + dx, a2.y), ImVec2(b2.x - dx, b2.y), b2, tint, faint ? 1.5f : 2.0f); }
        ImVec2 tip = wp[nw - 1];
        rg_arrowhead(dl, ImVec2(tip.x - 8, tip.y), tip, tint, faint ? 6.0f : 7.0f);
    }
    for (int vi = n; vi < (int)rgn.size(); ++vi) {
        if (!vLive[vi]) continue;   // node with no surviving link (all readers filtered/deduped) stays hidden
        RgNode& vn = rgn[vi];
        ImVec2 a(origin.x + vn.pos.x, origin.y + vn.pos.y), b(a.x + vn.w, a.y + vn.h);
        char nm[48]; std::snprintf(nm, sizeof nm, "%.*s", (int)vn.res->id.name.length, vn.res->id.name.data ? vn.res->id.name.data : "?");
        const char* cap = vn.label.data ? vn.label.data : "";
        dl->AddRectFilled(a, b, IM_COL32(32, 30, 40, 240), 4.0f);
        dl->AddRect(a, b, vn.tint, 4.0f, 0, 1.5f);
        float cx = (a.x + b.x) * 0.5f;
        ImVec2 ns = ImGui::CalcTextSize(nm), cs = ImGui::CalcTextSize(cap);
        dl->AddText(ImVec2(cx - ns.x * 0.5f, a.y + 3), IM_COL32(238, 236, 242, 255), nm);
        dl->AddText(ImVec2(cx - cs.x * 0.5f, b.y - cs.y - 3), rg_with_alpha(vn.tint, 220), cap);
    }

    // ---- tooltip. a hovered pin wins, else the per-pass reads/writes list.
    if (hovB >= 0) {
        PassNode* p = rgn[hovB].pass; ResourceNode* r = find_node(rg, { hovId });
        WGPUStringView rn = r ? r->id.name : WGPUStringView{};
        ImGui::BeginTooltip();
        ImGui::Text("%.*s", (int)rn.length, rn.data ? rn.data : "?");
        if (r) {
            if (r->kind == ResourceKind::Texture) ImGui::Text("texture  %u x %u", r->resolved.width, r->resolved.height);
            else                                         ImGui::Text("buffer  %llu bytes", (unsigned long long)r->bufferSize);
        }
        // the subresource this pin touches: layer for an array, mip for a chain
        if (r && r->kind == ResourceKind::Texture) {
            uint32_t layers = r->resolved.depthOrArrayLayers ? r->resolved.depthOrArrayLayers : 1;
            uint32_t mips   = r->mipLevelCount ? r->mipLevelCount : 1;
            if (layers > 1 || mips > 1 || hovLayer > 0 || hovMip > 0) {
                char sub[64]; int o = 0;
                if (layers > 1 || hovLayer > 0) o += std::snprintf(sub + o, sizeof sub - o, "layer %u / %u", hovLayer, layers);
                if (mips > 1 || hovMip > 0)     o += std::snprintf(sub + o, sizeof sub - o, "%smip %u / %u", o ? "   " : "", hovMip, mips);
                ImGui::TextDisabled("%s", sub);
            }
        }
        const char* verb = hovWrite ? "write"
            : (hovType == AccessType::ColorAttachment || hovType == AccessType::DepthStencilAttachment) ? "load"
            : "read";
        ImGui::TextDisabled("%s on P%d  (%s)", verb, hovB, rg_access_name(hovType));
        ImGui::Separator();
        if (hovWrite) ImGui::Text("produced here");
        else {
            int prod = rg_producer_of(box, n, p, hovId);
            if (prod >= 0) {
                WGPUStringView pn = rgn[prod].pass->id.name;
                ImGui::Text("produced by P%d %.*s", prod, (int)pn.length, pn.data ? pn.data : "");
            }
            else ImGui::TextDisabled(r && r->imported ? "imported (external input)" : "external input (no producer)");
        }
        ImGui::EndTooltip();
    }
    else if (hovBox >= 0) {
        PassNode* p = rgn[hovBox].pass; WGPUStringView nm = p->id.name;
        const char* kn = rg_kind_name(p->kind);
        ImGui::BeginTooltip();
        ImGui::Text("%.*s  [%s]", (int)nm.length, nm.data ? nm.data : "", kn);
        ImGui::Separator();
        for (uint32_t k = 0; k < p->accessCount; ++k) {
            const ResourceAccess& acc = p->accesses[k];
            ResourceNode* r = find_node(rg, acc.handle);
            WGPUStringView rn = r ? r->id.name : WGPUStringView{};
            ImGui::Text("[%s] %.*s  (%s)%s", access_is_write(acc.type) ? "W" : "R",
                (int)rn.length, rn.data ? rn.data : "", rg_access_name(acc.type),
                r ? (r->imported ? "  [imported]" : r->history ? "  [history]" : r->persistent ? "  [persistent]" : "") : "");
        }
        ImGui::EndTooltip();
    }
    else if (hovEdge >= 0) {
        REdge& e = edge[hovEdge]; ResourceNode* r = find_node(rg, { e.id });
        WGPUStringView rn = r ? r->id.name : WGPUStringView{};
        WGPUStringView sn = rgn[e.src].pass->id.name, dn = rgn[e.dst].pass->id.name;
        ImGui::BeginTooltip();
        ImGui::Text("%.*s", (int)rn.length, rn.data ? rn.data : "?");
        ImGui::TextDisabled("P%d %.*s  ->  P%d %.*s", e.src, (int)sn.length, sn.data ? sn.data : "",
            e.dst, (int)dn.length, dn.data ? dn.data : "");
        ImGui::EndTooltip();
    }

    ImGui::SetWindowFontScale(1.0f);   // overlays (details panel, zoom indicator) stay readable at any zoom

    // ---- details panel for the locked pin + its pass. a top-right overlay on the canvas draw list, no
    // nested child. a pin click fills it, an empty-canvas click clears it.
    if (lockB >= 0 && lockB < n) {
        PassNode* p = rgn[lockB].pass;
        ResourceNode* r = find_node(rg, { lockId });
        // the access this pin stands for, matched on id + side, giving type + touched subresource
        AccessType selType{}; uint32_t selMip = 0, selLayer = 0;
        for (uint32_t k = 0; k < p->accessCount; ++k) {
            const ResourceAccess& a = p->accesses[k];
            if (a.handle.id == lockId && access_is_write(a.type) == lockWrite)
                { selType = a.type; selMip = a.baseMip; selLayer = a.baseLayer; break; }
        }

        char ln[48][128]; ImU32 lc[48]; int nl = 0;
        const ImU32 cTitle = IM_COL32(238, 236, 242, 255), cDim = IM_COL32(150, 150, 162, 255),
                    cHead = IM_COL32(120, 200, 190, 255);
        auto add = [&](ImU32 col, const char* fmt, ...) {
            if (nl >= 48) return;
            va_list ap; va_start(ap, fmt); std::vsnprintf(ln[nl], sizeof ln[nl], fmt, ap); va_end(ap);
            lc[nl++] = col;
        };

        const bool pinSel = lockSlot >= 0;   // a specific pin vs. a whole-pass selection (body click)
        WGPUStringView pn = p->id.name;
        add(cTitle, "P%d  %.*s  [%s]", lockB, (int)pn.length, pn.data ? pn.data : "", rg_kind_name(p->kind));
        add(cDim, "");
        if (pinSel) {
            add(cHead, "SELECTED PIN");
            WGPUStringView rn = r ? r->id.name : WGPUStringView{};
            add(r ? rg_resource_color(r->kind) : cDim, "%.*s", (int)rn.length, rn.data ? rn.data : "?");
            if (r) {
                if (r->kind == ResourceKind::Texture) add(cDim, "texture  %u x %u", r->resolved.width, r->resolved.height);
                else                                         add(cDim, "buffer  %llu bytes", (unsigned long long)r->bufferSize);
            }
            if (r && r->kind == ResourceKind::Texture) {
                uint32_t layers = r->resolved.depthOrArrayLayers ? r->resolved.depthOrArrayLayers : 1;
                uint32_t mips   = r->mipLevelCount ? r->mipLevelCount : 1;
                if (layers > 1 || mips > 1 || selLayer > 0 || selMip > 0)
                    add(cDim, "layer %u / %u    mip %u / %u", selLayer, layers, selMip, mips);
            }
            const char* verb = lockWrite ? "write"
                : (selType == AccessType::ColorAttachment || selType == AccessType::DepthStencilAttachment) ? "load" : "read";
            add(cDim, "%s  (%s)", verb, rg_access_name(selType));
            if (lockWrite) add(cDim, "produced here");
            else {
                int prod = rg_producer_of(box, n, p, lockId);
                if (prod >= 0) { WGPUStringView qn = rgn[prod].pass->id.name; add(cDim, "produced by P%d %.*s", prod, (int)qn.length, qn.data ? qn.data : ""); }
                else add(cDim, r && r->imported ? "imported (external input)" : "external input");
            }
            add(cDim, "");
        }
        add(cHead, "PASS ACCESSES");
        for (uint32_t k = 0; k < p->accessCount; ++k) {
            const ResourceAccess& acc = p->accesses[k];
            ResourceNode* ar = find_node(rg, acc.handle);
            WGPUStringView arn = ar ? ar->id.name : WGPUStringView{};
            bool sel = acc.handle.id == lockId && access_is_write(acc.type) == lockWrite;
            add(sel ? cTitle : cDim, "%s [%s] %.*s (%s)%s", sel ? ">" : " ",
                access_is_write(acc.type) ? "W" : "R", (int)arn.length, arn.data ? arn.data : "",
                rg_access_name(acc.type), ar ? (ar->imported ? " [imp]" : ar->history ? " [hist]" : ar->persistent ? " [pers]" : "") : "");
        }

        // size to content, place top-right clamped into the canvas, then draw
        float lineH = ImGui::GetTextLineHeightWithSpacing(), maxW = 0;
        for (int i = 0; i < nl; ++i) { float w = ImGui::CalcTextSize(ln[i]).x; if (w > maxW) maxW = w; }
        const float padX = 10, padY = 8;
        float panelW = maxW + padX * 2, panelH = nl * lineH + padY * 2;
        ImVec2 tl(winPos.x + winSize.x - panelW - 8, winPos.y + 8);
        if (tl.x < winPos.x + 8) tl.x = winPos.x + 8;
        dl->AddRectFilled(tl, ImVec2(tl.x + panelW, tl.y + panelH), IM_COL32(24, 26, 32, 235), 5.0f);
        dl->AddRect(tl, ImVec2(tl.x + panelW, tl.y + panelH), IM_COL32(80, 86, 100, 255), 5.0f);
        float y = tl.y + padY;
        for (int i = 0; i < nl; ++i) { dl->AddText(ImVec2(tl.x + padX, y), lc[i], ln[i]); y += lineH; }
    }

    // ---- zoom level, bottom-right corner
    char zb[16]; std::snprintf(zb, sizeof zb, "%d%%", (int)(zoom * 100.0f + 0.5f));
    ImVec2 zs = ImGui::CalcTextSize(zb);
    ImVec2 zp(winPos.x + winSize.x - zs.x - 10, winPos.y + winSize.y - zs.y - 8);
    dl->AddRectFilled(ImVec2(zp.x - 6, zp.y - 4), ImVec2(zp.x + zs.x + 6, zp.y + zs.y + 4), IM_COL32(24, 26, 32, 200), 4.0f);
    dl->AddText(zp, IM_COL32(220, 220, 230, 255), zb);

    ImGui::EndChild();
}

// what `p` does to `id`: bit 1 reads, bit 2 writes, 0 does not touch it
static int rg_pass_access(PassNode* p, uint32_t id)
{
    int a = 0;
    for (uint32_t i = 0; i < p->accessCount; ++i)
        if (p->accesses[i].handle.id == id)
            a |= access_is_write(p->accesses[i].type) ? 2 : 1;
    return a;
}

// colour per alias slot, so transients packed onto one slot read as one hue. cycles a small palette,
// keeping adjacent slot indices distinguishable.
static ImU32 rg_slot_color(uint32_t slot)
{
    static const ImU32 pal[] = {
        IM_COL32( 90, 170, 250, 255), IM_COL32(250, 170,  90, 255), IM_COL32(130, 220, 130, 255),
        IM_COL32(220, 130, 220, 255), IM_COL32(230, 210,  90, 255), IM_COL32(110, 210, 220, 255),
        IM_COL32(240, 140, 140, 255), IM_COL32(170, 160, 250, 255),
    };
    return pal[slot % (sizeof pal / sizeof pal[0])];
}

// short format tag for the slot row labels, display only
static const char* rg_format_short(WGPUTextureFormat f)
{
    switch (f) {
      case WGPUTextureFormat_RGBA8Unorm:   return "RGBA8";
      case WGPUTextureFormat_BGRA8Unorm:   return "BGRA8";
      case WGPUTextureFormat_RGBA16Float:  return "RGBA16F";
      case WGPUTextureFormat_R32Float:     return "R32F";
      case WGPUTextureFormat_Depth32Float: return "D32F";
      case WGPUTextureFormat_Depth24Plus:  return "D24+";
      case WGPUTextureFormat_R8Unorm:      return "R8";
      default:                             return "tex";
    }
}

// lifetime grid over the graph's own per-frame GPU textures, passes in execution order across the top.
// each row is ONE dedicated allocation, never a bare logical handle:
//   * aliasing ON  -> a physical slot, its bar showing every occupant's writes (warm) and reads (cool),
//     with empty gaps where the texture is free between occupants
//   * aliasing OFF -> each realized transient is its own texture, one row, named for the resource
// toggling 'alias transients' collapses N transient rows into M shared textures, and that drop plus the
// gaps is the aliasing made visible. imported, history/persistent and dead resources have no dedicated
// per-frame allocation and are NOT shown. firstUse/lastUse come from compile() phase 3, slots from 4.
static void rg_draw_lifetimes(RenderGraph* rg, RenderGraphStorage& s)
{
    constexpr int   kMax = kRgDagMax;   // one cap for every tab, so the same graph never shows a different pass set
    constexpr float kLabelW = 150.0f, kColW = 88.0f, kRowH = 24.0f, kHeaderH = 24.0f;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kRGWrite), "write");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kRGRead), "read");
    ImGui::SameLine();
    ImGui::TextDisabled("(rows = the graph's dedicated per-frame GPU objects: textures + buffers)");

    // rows below are physical slots or per-transient objects, either way only dedicated graph-owned
    // allocations. the row count + saved MB are the win.
    auto phBytes = [](const PhysicalResource& ph) -> uint64_t {
        return ph.kind == ResourceKind::Buffer ? ph.bufferSize : texture_bytes(ph.sig.size, ph.sig.format);
    };
    if (s.m_slotCount) {
        uint32_t logical = 0; uint64_t logicalBytes = 0, physicalBytes = 0;
        for (uint32_t i = 0; i < s.m_slotCount; ++i) physicalBytes += phBytes(s.m_slots[i]);
        for (ResourceNode* r = s.m_resouces; r; r = r->next)
            if (r->aliasSlot != ResourceNode::kNoSlot) { ++logical; logicalBytes += phBytes(s.m_slots[r->aliasSlot]); }
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(150, 230, 150, 255)),
            "aliasing ON: %u transients packed onto %u GPU objects, saved %.2f MB",
            logical, s.m_slotCount, (double)(logicalBytes - physicalBytes) / (1024.0 * 1024.0));
    } else {
        uint32_t live = 0;
        for (ResourceNode* r = s.m_resouces; r; r = r->next)
            if (!r->is_external() && r->firstUse != ResourceNode::kNoPass) ++live;
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(205, 205, 120, 255)),
            "aliasing OFF: %u dedicated transient objects (tick 'alias transients' in Demos to pack)", live);
    }

    PassNode* passAt[kMax];
    int nPass = 0;
    for (PassNode* p = s.m_passes; p && nPass < kMax; p = p->next) passAt[nPass++] = p;
    int passTotal = 0; for (PassNode* p = s.m_passes; p; p = p->next) ++passTotal;   // for the cap note below

    // every row is one physical texture, whether aliasing is on or off. a slot hosts >=1 disjoint-lifetime
    // transient, a non-aliased transient is its own single-occupant texture. imported/history/persistent and
    // dead transients are excluded, so only real GPU textures appear.
    struct Row { ResourceNode* r; int slot; uint32_t first, last; };
    Row row[kMax];
    int nRow = 0;
    for (uint32_t si = 0; si < s.m_slotCount && nRow < kMax; ++si) {
        uint32_t f = ResourceNode::kNoPass, l = 0;
        for (ResourceNode* o = s.m_resouces; o; o = o->next)
            if (o->aliasSlot == si) { if (o->firstUse < f) f = o->firstUse; if (o->lastUse > l) l = o->lastUse; }
        if (f != ResourceNode::kNoPass) row[nRow++] = { nullptr, (int)si, f, l };
    }
    for (ResourceNode* r = s.m_resouces; r && nRow < kMax; r = r->next)
        if (!r->is_external() && r->aliasSlot == ResourceNode::kNoSlot && r->firstUse != ResourceNode::kNoPass)
            row[nRow++] = { r, -1, r->firstUse, r->lastUse };

    // silent truncation is the one thing a debug view must not do
    if (passTotal > nPass || nRow == kMax) {
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.30f, 1),
            "view capped at %d: showing %d of %d passes%s", kMax, nPass, passTotal, nRow == kMax ? ", rows truncated" : "");
    }

    // uniform per-row queries over both kinds, so the draw + details code is identical either way
    auto rowFmt   = [&](const Row& rw) { return rw.slot >= 0 ? s.m_slots[rw.slot].sig.format : rw.r->format;   };
    auto rowSize  = [&](const Row& rw) { return rw.slot >= 0 ? s.m_slots[rw.slot].sig.size   : rw.r->resolved; };
    auto rowUsage = [&](const Row& rw) { return rw.slot >= 0 ? s.m_slots[rw.slot].texUsage : rw.r->texUsage; };
    auto rowKind  = [&](const Row& rw) { return rw.slot >= 0 ? s.m_slots[rw.slot].kind     : rw.r->kind;     };
    auto rowAccess = [&](const Row& rw, uint32_t c) -> int {
        if (rw.slot < 0) return rg_pass_access(passAt[c], rw.r->handle.id);
        int a = 0;
        for (ResourceNode* o = s.m_resouces; o; o = o->next)
            if (o->aliasSlot == (uint32_t)rw.slot) a |= rg_pass_access(passAt[c], o->handle.id);
        return a;
    };
    // a solo row spans [first,last], a slot row is live only while some occupant is, so the gaps between
    // occupants render empty: the texture freed and reused, on the timeline
    auto rowLive = [&](const Row& rw, uint32_t c) -> bool {
        if (rw.slot < 0) return rw.first <= c && c <= rw.last;
        for (ResourceNode* o = s.m_resouces; o; o = o->next)
            if (o->aliasSlot == (uint32_t)rw.slot && o->firstUse <= c && c <= o->lastUse) return true;
        return false;
    };
    // drives the label: a texture backing one resource shows its NAME, a shared one the synthetic "image N"
    auto rowOccupants = [&](const Row& rw, ResourceNode*& sole) -> int {
        if (rw.slot < 0) { sole = rw.r; return 1; }
        int n = 0; sole = nullptr;
        for (ResourceNode* o = s.m_resouces; o; o = o->next)
            if (o->aliasSlot == (uint32_t)rw.slot) { if (!n) sole = o; ++n; }
        return n;
    };

    // selection persists across frames, but the graph is rebuilt each frame and handle ids are not stable,
    // so key a slot row by its index and a solo row by its resource NAME
    static int  selSlot = -2;        // -2 = nothing; -1 = a solo row (selName); >= 0 = slot index
    static char selName[96] = {};
    auto rowSelected = [&](const Row& rw) -> bool {
        if (rw.slot >= 0) return rw.slot == selSlot;
        return selSlot == -1 && rw.r->id.name.data && std::strcmp(selName, rw.r->id.name.data) == 0;
    };

    // grid on top, details for the selected texture below, only when the tab is tall enough
    const float availY = ImGui::GetContentRegionAvail().y;
    const float gridH  = availY > 240.0f ? availY * 0.62f : availY;

    ImGui::BeginChild("rg_life_grid", ImVec2(0, gridH), true, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(kLabelW + nPass * kColW, kHeaderH + nRow * kRowH));   // reserve scroll region
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float gridR = origin.x + kLabelW + nPass * kColW;
    const float gridT = origin.y + kHeaderH;
    const float gridB = gridT + nRow * kRowH;

    // one clipped pass name per column + a faint column line, hover a header for the full name
    for (int c = 0; c < nPass; ++c) {
        float x = origin.x + kLabelW + c * kColW;
        dl->AddLine(ImVec2(x, origin.y), ImVec2(x, gridB), IM_COL32(255, 255, 255, 22));
        WGPUStringView nm = passAt[c]->id.name;
        if (nm.data) {
            dl->PushClipRect(ImVec2(x + 4, origin.y), ImVec2(x + kColW, gridT), true);
            dl->AddText(ImVec2(x + 6, origin.y + 5), IM_COL32(225, 225, 225, 255), nm.data, nm.data + nm.length);
            dl->PopClipRect();
        }
        ImGui::SetCursorScreenPos(ImVec2(x, origin.y));
        ImGui::PushID(c);
        ImGui::InvisibleButton("h", ImVec2(kColW, kHeaderH));
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("P%d  %.*s  [%s]", c, (int)nm.length, nm.data ? nm.data : "", rg_kind_name(passAt[c]->kind));
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    dl->AddLine(ImVec2(origin.x, gridT), ImVec2(gridR, gridT), IM_COL32(255, 255, 255, 40));   // axis underline

    // one row per physical texture, with occupant write/read cells across the timeline and empty gaps where
    // it is free. click a row to inspect it below.
    for (int i = 0; i < nRow; ++i) {
        const float y    = gridT + i * kRowH;
        ResourceNode* sole = nullptr;
        const int   nocc   = rowOccupants(row[i], sole);
        const bool  shared = nocc > 1;
        // shared textures get "image N" + a slot colour so they stand out, a single-occupant one keeps the
        // resource name. format and size are in the details panel.
        const ImU32 col  = shared ? rg_slot_color((uint32_t)i) : IM_COL32(210, 210, 210, 255);
        const bool  seld = rowSelected(row[i]);
        if (seld)       dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(gridR, y + kRowH), IM_COL32(255, 255, 255, 30));
        else if (i & 1) dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(gridR, y + kRowH), IM_COL32(255, 255, 255, 10));

        const bool isBuf = rowKind(row[i]) == ResourceKind::Buffer;
        char label[96];
        if (shared) std::snprintf(label, sizeof label, "%s %d (x%d)", isBuf ? "buffer" : "image", i, nocc);
        else        std::snprintf(label, sizeof label, "%.*s", (int)sole->id.name.length, sole->id.name.data ? sole->id.name.data : "");
        if (shared)   // gutter swatch only for shared (aliased) textures
            dl->AddRectFilled(ImVec2(origin.x + 1, y + 4), ImVec2(origin.x + 4, y + kRowH - 4), col);
        dl->PushClipRect(ImVec2(origin.x + 6, y), ImVec2(origin.x + kLabelW - 4, y + kRowH), true);
        dl->AddText(ImVec2(origin.x + 8, y + 4), col, label);
        dl->PopClipRect();

        // the whole row, gutter and grid, selects this texture for the details panel
        ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
        ImGui::PushID(kMax + i);
        if (ImGui::InvisibleButton("row", ImVec2(kLabelW + nPass * kColW, kRowH))) {
            selSlot = row[i].slot;
            if (row[i].slot < 0) std::snprintf(selName, sizeof selName, "%.*s",
                (int)row[i].r->id.name.length, row[i].r->id.name.data ? row[i].r->id.name.data : "");
        }
        const bool hov = ImGui::IsItemHovered();
        ImGui::PopID();

        const float x0 = origin.x + kLabelW + row[i].first * kColW + 3.0f;
        const float x1 = origin.x + kLabelW + (row[i].last + 1) * kColW - 3.0f;
        const float ty = y + 3.0f, by = y + kRowH - 3.0f;

        // gaps are left empty, so reuse reads off the timeline
        const ImU32 bandCol = rg_with_alpha(col, 45);
        for (uint32_t c = row[i].first; c <= row[i].last; ++c) {
            if (!rowLive(row[i], c)) continue;
            float sx0 = (c == row[i].first) ? x0 : origin.x + kLabelW + c * kColW;
            float sx1 = (c == row[i].last)  ? x1 : origin.x + kLabelW + (c + 1) * kColW;
            dl->AddRectFilled(ImVec2(sx0, ty), ImVec2(sx1, by), bandCol, 0.0f);
            int acc = rowAccess(row[i], c);
            if (!acc) continue;
            if (acc == 3) {   // read + write -> split top (write) / bottom (read)
                float mid = (ty + by) * 0.5f;
                dl->AddRectFilled(ImVec2(sx0, ty), ImVec2(sx1, mid), kRGWrite, 0.0f);
                dl->AddRectFilled(ImVec2(sx0, mid), ImVec2(sx1, by), kRGRead, 0.0f);
            } else
                dl->AddRectFilled(ImVec2(sx0, ty), ImVec2(sx1, by), acc == 2 ? kRGWrite : kRGRead, 0.0f);
        }

        // outline each contiguous occupancy run, so the gaps stay open and each occupant reads as its own
        // block
        const ImU32 edge = (seld || hov) ? IM_COL32(255, 255, 255, 255) : col;
        const float wth  = (seld || hov) ? 2.0f : 1.0f;
        for (uint32_t c = row[i].first; c <= row[i].last; ) {
            if (!rowLive(row[i], c)) { ++c; continue; }
            uint32_t segStart = c;
            while (c <= row[i].last && rowLive(row[i], c)) ++c;
            float sx0 = origin.x + kLabelW + segStart * kColW + 3.0f;
            float sx1 = origin.x + kLabelW + c * kColW - 3.0f;
            dl->AddRect(ImVec2(sx0, ty), ImVec2(sx1, by), edge, 0.0f, 0, wth);
        }
    }
    ImGui::EndChild();

    // ---- details panel: the selected physical texture and the logical resource(s) it backs ----
    if (gridH < availY) {
        ImGui::BeginChild("rg_life_details", ImVec2(0, 0), true);
        int selIdx = -1;
        for (int i = 0; i < nRow; ++i) if (rowSelected(row[i])) { selIdx = i; break; }
        if (selIdx < 0) {
            ImGui::TextDisabled("click a texture above to inspect it");
        } else {
            const Row&    rw    = row[selIdx];
            const bool    isBuf = rowKind(rw) == ResourceKind::Buffer;
            ResourceNode* psole = nullptr;
            const int     pnocc = rowOccupants(rw, psole);
            const ImU32   hcol  = pnocc > 1 ? rg_slot_color((uint32_t)selIdx) : IM_COL32(225, 225, 225, 255);
            if (pnocc > 1) ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(hcol), "%s %d  (shared by %d)",
                isBuf ? "buffer" : "image", selIdx, pnocc);
            else           ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(hcol), "%.*s",
                (int)psole->id.name.length, psole->id.name.data ? psole->id.name.data : "");
            ImGui::SameLine();

            char ub[160]; ub[0] = '\0';
            if (isBuf) {
                uint64_t        bytes = rw.slot >= 0 ? s.m_slots[rw.slot].bufferSize : rw.r->bufferSize;
                WGPUBufferUsage u     = rw.slot >= 0 ? s.m_slots[rw.slot].bufUsage   : rw.r->bufUsage;
                ImGui::Text("- buffer  -  %.1f KB", bytes / 1024.0);
                auto addb = [&](WGPUBufferUsage bit, const char* nm) { if (u & bit) { if (ub[0]) std::strcat(ub, " | "); std::strcat(ub, nm); } };
                addb(WGPUBufferUsage_Storage,  "Storage");   addb(WGPUBufferUsage_Uniform,  "Uniform");
                addb(WGPUBufferUsage_Vertex,   "Vertex");    addb(WGPUBufferUsage_Index,    "Index");
                addb(WGPUBufferUsage_Indirect, "Indirect");  addb(WGPUBufferUsage_CopySrc,  "CopySrc");
                addb(WGPUBufferUsage_CopyDst,  "CopyDst");
            } else {
                WGPUExtent3D      sz  = rowSize(rw);
                WGPUTextureFormat fmt = rowFmt(rw);
                WGPUTextureUsage  u   = rowUsage(rw);
                ImGui::Text("- %s  %u x %u  -  %.1f KB", rg_format_short(fmt), sz.width, sz.height, texture_bytes(sz, fmt) / 1024.0);
                auto addu = [&](WGPUTextureUsage bit, const char* nm) { if (u & bit) { if (ub[0]) std::strcat(ub, " | "); std::strcat(ub, nm); } };
                addu(WGPUTextureUsage_RenderAttachment, "RenderAttachment");
                addu(WGPUTextureUsage_TextureBinding,   "TextureBinding");
                addu(WGPUTextureUsage_StorageBinding,   "StorageBinding");
                addu(WGPUTextureUsage_CopySrc,          "CopySrc");
                addu(WGPUTextureUsage_CopyDst,          "CopyDst");
            }
            ImGui::TextDisabled("usage: %s", ub[0] ? ub : "(none)");

            int occ = 0;
            if (rw.slot >= 0) { for (ResourceNode* o = s.m_resouces; o; o = o->next) if (o->aliasSlot == (uint32_t)rw.slot) ++occ; }
            else occ = 1;
            ImGui::Separator();
            if (occ > 1) ImGui::Text("%d logical resources share this %s (disjoint lifetimes):", occ, isBuf ? "buffer" : "texture");
            else         ImGui::Text("backs 1 logical resource:");

            auto detailOne = [&](ResourceNode* o) {
                WGPUStringView f = pass_name_at(s.m_passes, o->firstUse);
                WGPUStringView l = pass_name_at(s.m_passes, o->lastUse);
                ImGui::BulletText("%.*s   [%.*s .. %.*s]",
                    (int)o->id.name.length, o->id.name.data ? o->id.name.data : "",
                    (int)f.length, f.data ? f.data : "", (int)l.length, l.data ? l.data : "");
                ImGui::Indent();
                for (uint32_t c = o->firstUse; c <= o->lastUse; ++c) {
                    int a = rg_pass_access(passAt[c], o->handle.id);
                    if (!a) continue;
                    WGPUStringView pn = passAt[c]->id.name;
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(a == 1 ? kRGRead : kRGWrite),
                        "%s  %.*s", a == 3 ? "rw" : a == 2 ? " w" : " r", (int)pn.length, pn.data ? pn.data : "");
                }
                ImGui::Unindent();
            };
            if (rw.slot >= 0) { for (ResourceNode* o = s.m_resouces; o; o = o->next) if (o->aliasSlot == (uint32_t)rw.slot) detailOne(o); }
            else detailOne(rw.r);
        }
        ImGui::EndChild();
    }
}

// GPU-memory view across every pool the graph owns: the transient pool, a descriptor-keyed cache of
// textures + buffers, and the history/persistent ping-pong. the grand total answers what the graph costs
// in VRAM, and the transient pool's create/evict log keeps steady-state reuse verifiable. drawn after
// realize() and before release_resources()/end_frame(), so every count is this frame's live allocation.
static void rg_draw_memory(RenderGraphStorage& s)
{
    TransientResourcePool&  tp   = s.m_allocator->transient;
    PersistentResourcePool& pool = s.m_allocator->pool;

    // one descriptor-keyed cache tagged by Entry::kind, one physical object per entry, idle ones retained
    // kRetain frames. held includes idle.
    int held = 0, inUse = 0, texHeld = 0, bufHeld = 0;
    uint64_t texBytes = 0, texInUseBytes = 0, bufBytes = 0, bufInUseBytes = 0;
    for (const TransientResourcePool::Entry* ep = tp.entries; ep; ep = ep->next) {
        const TransientResourcePool::Entry& e = *ep;
        ++held; if (e.inUse) ++inUse;
        if (e.kind == ResourceKind::Buffer) { ++bufHeld; bufBytes += e.bufferSize; if (e.inUse) bufInUseBytes += e.bufferSize; }
        else {
            const uint64_t b = rg_entry_bytes(e);
            ++texHeld; texBytes += b;
            if (e.inUse) texInUseBytes += b;
        }
    }

    // one name-keyed pool: a history entry holds kLayers physical objects, a persistent entry 1. the buffer
    // arm leaves size {} and format Undefined, so split on bufferSize. mem already scales by layers.
    int tmpTexCount = 0, tmpBufCount = 0;
    uint64_t tmpTexBytes = 0, tmpBufBytes = 0;
    for (const PersistentResourcePool::Entry* ep = pool.entries; ep; ep = ep->next) {
        const PersistentResourcePool::Entry& e = *ep;
        if (!e.created) continue;
        if (e.bufferSize) { ++tmpBufCount; tmpBufBytes += e.bufferSize * e.layers; }
        else              { ++tmpTexCount; tmpTexBytes += texture_bytes(e.sig.size, e.sig.format, e.sig.mipLevelCount, e.sig.sampleCount, e.sig.dim) * e.layers; }
    }

    const uint64_t grand = texBytes + bufBytes + tmpTexBytes + tmpBufBytes;

    ImGui::Text("frame %llu  |  transient pool: %d held, %d in use", (unsigned long long)tp.frame, held, inUse);
    ImGui::SameLine();
    if (tp.createdThisFrame == 0)
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1), "  |  0 created (reused from pool)");
    else
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.30f, 1), "  |  %u created this frame", tp.createdThisFrame);

    char gb[24]; rg_bytes_str(grand, gb, sizeof gb);
    ImGui::Text("VRAM %s total", gb);
    ImGui::SameLine(); ImGui::TextDisabled("(graph-allocated; imported/caller-owned excluded, listed below)");
    {
        char a[24], ib[24], idb[24], bb[24], bib[24], bidb[24], cb[24], cbb[24];
        rg_bytes_str(texBytes, a, sizeof a);
        rg_bytes_str(texInUseBytes, ib, sizeof ib);
        rg_bytes_str(texBytes - texInUseBytes, idb, sizeof idb);   // in use is a subset sum -> no underflow
        rg_bytes_str(bufBytes, bb, sizeof bb);
        rg_bytes_str(bufInUseBytes, bib, sizeof bib);
        rg_bytes_str(bufBytes - bufInUseBytes, bidb, sizeof bidb);
        rg_bytes_str(tmpTexBytes, cb, sizeof cb);
        rg_bytes_str(tmpBufBytes, cbb, sizeof cbb);
        ImGui::BulletText("transient tex  %-9s  %d held (%s in use, %s idle)", a, texHeld, ib, idb);
        ImGui::BulletText("transient buf  %-9s  %d held (%s in use, %s idle)", bb, bufHeld, bib, bidb);
        ImGui::BulletText("pool tex       %-9s  %d entries", cb, tmpTexCount);
        ImGui::BulletText("pool buf       %-9s  %d entries", cbb, tmpBufCount);
    }

    // each packed transient would otherwise own a slot-sized object, so the win is the slot bytes counted
    // once per member minus once per slot
    if (s.m_slotCount) {
        auto phBytes = [](const PhysicalResource& ph) -> uint64_t {
            return ph.kind == ResourceKind::Buffer ? ph.bufferSize : texture_bytes(ph.sig.size, ph.sig.format);
        };
        uint32_t logical = 0; uint64_t logicalBytes = 0, physicalBytes = 0;
        for (uint32_t i = 0; i < s.m_slotCount; ++i) physicalBytes += phBytes(s.m_slots[i]);
        for (ResourceNode* r = s.m_resouces; r; r = r->next)
            if (r->aliasSlot != ResourceNode::kNoSlot) { ++logical; logicalBytes += phBytes(s.m_slots[r->aliasSlot]); }
        char sb[24]; rg_bytes_str(logicalBytes - physicalBytes, sb, sizeof sb);
        ImGui::TextColored(ImVec4(0.59f, 0.90f, 0.59f, 1),
            "aliasing: %u transients packed onto %u objects, saved %s", logical, s.m_slotCount, sb);
    }

    // an attachment cleared+discarded in one pass and never read need not leave the GPU. the feature flag
    // decides whether the bit reaches the driver, this list shows what qualified.
    {
        if (s.m_allocator->transientFeatureOn)
            ImGui::TextColored(ImVec4(0.59f, 0.90f, 0.59f, 1),
                "transient attachments: feature ON  |  %u memoryless (TRANSIENT_ATTACHMENT emitted; driver may skip VRAM)",
                s.transientCount);
        else
            ImGui::TextColored(ImVec4(0.85f, 0.80f, 0.45f, 1),
                "transient attachments: feature OFF  |  %u candidate(s) (bit dropped -> realized as plain RENDER_ATTACHMENT)",
                s.transientCount);
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
            ImGui::TextUnformatted(
                "A graph-owned attachment cleared + discarded in one render pass, never sampled/copied/stored,\n"
                "never needs to leave the GPU. On a TBDR/tile-memory device it stays on-chip and skips its VRAM\n"
                "allocation. Inferred from usage; no API declares it. No-op on D3D12 (feature absent there).");
            ImGui::EndTooltip();
        }
        const ImGuiTableFlags ttf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
        if (s.transientCount && ImGui::BeginTable("rg_transient", 4, ttf)) {
            ImGui::TableSetupColumn("resource");
            ImGui::TableSetupColumn("size");
            ImGui::TableSetupColumn("format");
            ImGui::TableSetupColumn("samples");
            ImGui::TableHeadersRow();
            for (ResourceNode* r = s.m_resouces; r; r = r->next) {
                if (!r->transientAttachment) continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%.*s", (int)r->id.name.length, r->id.name.data ? r->id.name.data : "");
                ImGui::TableNextColumn(); ImGui::Text("%ux%u", r->resolved.width, r->resolved.height);
                ImGui::TableNextColumn(); ImGui::Text("%s", rg_format_name(r->format));
                ImGui::TableNextColumn(); ImGui::Text("%ux", r->sampleCount);
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
    }

    ImGui::TextDisabled("tex usage A=attach T=sampled S=storage r=copy-src w=copy-dst   |   buf usage U=uniform S=storage V=vertex I=index X=indirect   |   evict after %llu idle frames",
        (unsigned long long)TransientResourcePool::kRetain);
    ImGui::Separator();

    const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;

    // currently held physical textures
    ImGui::TextDisabled("transient textures");
    if (ImGui::BeginTable("tp_live", 9, tf)) {
        ImGui::TableSetupColumn("size");
        ImGui::TableSetupColumn("mips");
        ImGui::TableSetupColumn("layers");
        ImGui::TableSetupColumn("samples");
        ImGui::TableSetupColumn("format");
        ImGui::TableSetupColumn("usage");
        ImGui::TableSetupColumn("mem");
        ImGui::TableSetupColumn("state");
        ImGui::TableSetupColumn("last use");   // descriptor-keyed pool has no per-row name; show the last claimant
        ImGui::TableHeadersRow();
        int idx = 0;
        for (const TransientResourcePool::Entry* ep = tp.entries; ep; ep = ep->next) {
            const TransientResourcePool::Entry& e = *ep;
            if (e.kind == ResourceKind::Buffer) continue;   // buffers listed in their own table below
            char ub[8]; rg_usage_str(e.usage, ub, sizeof ub);
            const uint64_t eb = rg_entry_bytes(e);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%ux%u", e.sig.size.width, e.sig.size.height);
            ImGui::TableNextColumn(); ImGui::Text("%u", e.sig.mipLevelCount);
            ImGui::TableNextColumn(); ImGui::Text("%u", e.sig.size.depthOrArrayLayers);
            ImGui::TableNextColumn(); ImGui::Text("%ux", e.sig.sampleCount);
            ImGui::TableNextColumn(); ImGui::Text("%s", rg_format_name(e.sig.format));
            ImGui::TableNextColumn(); ImGui::Text("%s", ub);
            ImGui::TableNextColumn();
            if (eb) { char mb[24]; rg_bytes_str(eb, mb, sizeof mb); ImGui::Text("%s", mb); }
            else    ImGui::TextDisabled("?");
            ImGui::TableNextColumn();
            if (e.inUse) ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1), "in use");
            else         ImGui::TextDisabled("idle %lluf", (unsigned long long)(tp.frame - e.lastUsedFrame));
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", e.identity ? (const char*)e.identity : "-");   // interned, NUL-terminated
        }
        ImGui::EndTable();
    }

    // the buffer arm of the same pool
    ImGui::Spacing();
    ImGui::TextDisabled("transient buffers");
    if (ImGui::BeginTable("tp_buf", 4, tf)) {
        ImGui::TableSetupColumn("size");
        ImGui::TableSetupColumn("usage");
        ImGui::TableSetupColumn("mem");
        ImGui::TableSetupColumn("state");
        ImGui::TableHeadersRow();
        bool any = false;
        for (const TransientResourcePool::Entry* ep = tp.entries; ep; ep = ep->next) {
            const TransientResourcePool::Entry& e = *ep;
            if (e.kind != ResourceKind::Buffer) continue;
            any = true;
            char ub[12]; rg_buf_usage_str(e.bufUsage, ub, sizeof ub);
            char mb[24]; rg_bytes_str(e.bufferSize, mb, sizeof mb);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%llu B", (unsigned long long)e.bufferSize);
            ImGui::TableNextColumn(); ImGui::Text("%s", ub);
            ImGui::TableNextColumn(); ImGui::Text("%s", mb);
            ImGui::TableNextColumn();
            if (e.inUse) ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1), "in use");
            else         ImGui::TextDisabled("idle %lluf", (unsigned long long)(tp.frame - e.lastUsedFrame));
        }
        ImGui::EndTable();
        if (!any) ImGui::TextDisabled("(none)");
    }

    // one row per name. layers reads 2 for a ping-pong history entry, 1 for a persistent one.
    ImGui::Spacing();
    ImGui::TextDisabled("persistent + history textures  (layers: 2 = history ping-pong, 1 = persistent)");
    if (ImGui::BeginTable("tp_tmp", 8, tf)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("size");
        ImGui::TableSetupColumn("mips");
        ImGui::TableSetupColumn("layers");
        ImGui::TableSetupColumn("samples");
        ImGui::TableSetupColumn("format");
        ImGui::TableSetupColumn("usage");
        ImGui::TableSetupColumn("mem");
        ImGui::TableHeadersRow();
        bool any = false;
        for (const PersistentResourcePool::Entry* ep = pool.entries; ep; ep = ep->next) {
            const PersistentResourcePool::Entry& e = *ep;
            if (!e.created || e.bufferSize) continue;   // buffer-arm entries listed in their own table below
            any = true;
            char ub[8]; rg_usage_str(e.usage, ub, sizeof ub);
            const uint64_t eb = texture_bytes(e.sig.size, e.sig.format, e.sig.mipLevelCount, e.sig.sampleCount, e.sig.dim) * e.layers;
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%.*s", (int)e.name.length, e.name.data ? e.name.data : "");
            ImGui::TableNextColumn(); ImGui::Text("%ux%u", e.sig.size.width, e.sig.size.height);
            ImGui::TableNextColumn(); ImGui::Text("%u", e.sig.mipLevelCount);
            ImGui::TableNextColumn(); ImGui::Text("%u x%u", e.sig.size.depthOrArrayLayers, e.layers);
            ImGui::TableNextColumn(); ImGui::Text("%ux", e.sig.sampleCount);
            ImGui::TableNextColumn(); ImGui::Text("%s", rg_format_name(e.sig.format));
            ImGui::TableNextColumn(); ImGui::Text("%s", ub);
            ImGui::TableNextColumn();
            if (eb) { char mb[24]; rg_bytes_str(eb, mb, sizeof mb); ImGui::Text("%s", mb); }
            else    ImGui::TextDisabled("?");
        }
        ImGui::EndTable();
        if (!any) ImGui::TextDisabled("(none)");
    }

    // one row per name, mem already scaled by layers
    ImGui::Spacing();
    ImGui::TextDisabled("persistent + history buffers");
    if (ImGui::BeginTable("tp_tmpbuf", 3, tf)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("usage");
        ImGui::TableSetupColumn("mem (x layers)");
        ImGui::TableHeadersRow();
        bool any = false;
        for (const PersistentResourcePool::Entry* ep = pool.entries; ep; ep = ep->next) {
            const PersistentResourcePool::Entry& e = *ep;
            if (!e.created || !e.bufferSize) continue;
            any = true;
            char ub[12]; rg_buf_usage_str(e.bufUsage, ub, sizeof ub);
            char mb[24]; rg_bytes_str(e.bufferSize * e.layers, mb, sizeof mb);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%.*s", (int)e.name.length, e.name.data ? e.name.data : "");
            ImGui::TableNextColumn(); ImGui::Text("%s", ub);
            ImGui::TableNextColumn(); ImGui::Text("%s", mb);
        }
        ImGui::EndTable();
        if (!any) ImGui::TextDisabled("(none)");
    }

    // caller-owned: real GPU memory the graph writes but does NOT allocate, so it sits outside the total
    // above. the mem column stays blank, since the graph never owns the bytes and import records no format.
    // the point is to make the caller-owned surface visible, not to double-count it.
    ImGui::Spacing();
    ImGui::TextDisabled("imported (caller-owned, not in VRAM total)");
    if (ImGui::BeginTable("tp_imported", 3, tf)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("kind");
        ImGui::TableSetupColumn("size");
        ImGui::TableHeadersRow();
        bool any = false;
        for (ResourceNode* r = s.m_resouces; r; r = r->next) {
            if (!r->imported) continue;
            any = true;
            const bool isBuf = r->kind == ResourceKind::Buffer;
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%.*s", (int)r->id.name.length, r->id.name.data ? r->id.name.data : "");
            ImGui::TableNextColumn(); ImGui::Text("%s", isBuf ? "buffer" : "texture");
            ImGui::TableNextColumn();
            if (isBuf) ImGui::TextDisabled("-");
            else       ImGui::Text("%u x %u", r->resolved.width, r->resolved.height);
        }
        ImGui::EndTable();
        if (!any) ImGui::TextDisabled("(none)");
    }

    ImGui::Spacing();
    ImGui::Text("transient pool events (newest first)");
    if (ImGui::Button("reset log")) tp.log_reset();
    ImGui::BeginChild("tp_log", ImVec2(0, 0), true);
    const uint64_t total = tp.eventCount;
    const uint64_t shown = total < TransientResourcePool::kLog ? total : TransientResourcePool::kLog;
    for (uint64_t k = 0; k < shown; ++k) {
        const TransientResourcePool::LogRec& r = tp.eventLog[(total - 1 - k) % TransientResourcePool::kLog];
        const bool create = r.event == TransientResourcePool::Event::Create;
        const ImVec4 col = create ? ImVec4(0.95f, 0.70f, 0.30f, 1) : ImVec4(0.60f, 0.60f, 0.60f, 1);
        if (r.kind == ResourceKind::Buffer)
            ImGui::TextColored(col, "f%-6llu  %-6s  buf %llu B", (unsigned long long)r.frame, create ? "CREATE" : "evict",
                (unsigned long long)r.bufferSize);
        else
            ImGui::TextColored(col, "f%-6llu  %-6s  %ux%u  %s", (unsigned long long)r.frame, create ? "CREATE" : "evict",
                r.size.width, r.size.height, rg_format_name(r.format));
    }
    if (shown == 0) ImGui::TextDisabled("(no events yet)");
    ImGui::EndChild();
}

// one usage bar for a growable block-chained arena: `value` bytes against the arena's CURRENT capacity,
// which itself grows as blocks chain on. a block count > 1 means the arena outgrew its first block this
// run. label doubles as the InvisibleButton id, so keep them unique.
static void rg_draw_arena_bar(const char* label, const char* valueLabel, size_t value, const Arena& arena, ImU32 color)
{
    const double kib  = 1024.0;
    const double cap  = arena.totalCapacity ? (double)arena.totalCapacity : 1.0;
    float        frac = (float)((double)value / cap);
    if (frac > 1.0f) frac = 1.0f;
    if (frac < 0.0f) frac = 0.0f;

    constexpr float kLabelW = 56.0f;   // fixed gutter so both bars' left edges line up under each other
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(kLabelW);

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    float        w  = ImGui::GetContentRegionAvail().x;
    if (w < 1.0f) w = 1.0f;                                   // collapsed window -> keep InvisibleButton happy
    const float  h  = ImGui::GetFrameHeight();
    const ImVec2 p1(p0.x + w, p0.y + h);

    ImGui::InvisibleButton(label, ImVec2(w, h));
    const bool  hov = ImGui::IsItemHovered();
    ImDrawList* dl  = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p0, p1, IM_COL32(28, 28, 28, 255), 3.0f);                       // track
    if (frac > 0.0f)
        dl->AddRectFilled(p0, ImVec2(p0.x + w * frac, p1.y), color, 0.0f);            // fill
    dl->AddRect(p0, p1, hov ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 20, 180), 3.0f);

    char ov[80];
    std::snprintf(ov, sizeof ov, "%.1f / %.0f KB  x%llu blk", value / kib, cap / kib,
        (unsigned long long)arena.blockCount);
    const ImVec2 ts = ImGui::CalcTextSize(ov);
    dl->AddText(ImVec2(p0.x + (w - ts.x) * 0.5f, p0.y + (h - ts.y) * 0.5f), IM_COL32(235, 235, 235, 255), ov);

    if (hov) {
        ImGui::BeginTooltip();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%s", label);
        ImGui::Text("%-9s%llu B  (%.2f KB)", valueLabel, (unsigned long long)value, value / kib);
        ImGui::Text("capacity %.2f KB  (grows)", cap / kib);
        ImGui::Text("blocks   %llu  x %.0f KB", (unsigned long long)arena.blockCount, arena.blockSize / kib);
        ImGui::EndTooltip();
    }
}

// the GraphAllocator as three stacked bars, one per independent arena. `front` holds the frame's permanent
// objects, `scratch` holds compile()'s temporaries and is rewound per scope, so it shows a per-frame
// high-water rather than the empty live value, and `persist` backs the pool Entry cells and is never
// per-frame reset, so its live value is the pools' bounded footprint.
static void rg_draw_arena(GraphAllocator& a)
{
    rg_draw_arena_bar("front",   "used", a.front.live_used(),   a.front,   kRGRead);
    rg_draw_arena_bar("scratch", "peak", a.scratch.peakUsage,   a.scratch, kRGWrite);
    rg_draw_arena_bar("persist", "used", a.persist.live_used(), a.persist, kRGExt);
}

// cycled by series index, kept bright so thin polylines read against the dark canvas
static ImU32 rg_series_color(uint32_t i)
{
    static const ImU32 pal[] = {
        IM_COL32(235, 110, 110, 255), IM_COL32(110, 200, 120, 255), IM_COL32(110, 170, 240, 255),
        IM_COL32(235, 200, 90, 255),  IM_COL32(200, 130, 230, 255), IM_COL32(90, 215, 215, 255),
        IM_COL32(240, 150, 90, 255),  IM_COL32(170, 220, 110, 255), IM_COL32(150, 150, 240, 255),
        IM_COL32(230, 130, 180, 255), IM_COL32(120, 220, 170, 255), IM_COL32(210, 210, 130, 255),
    };
    return pal[i % (sizeof(pal) / sizeof(pal[0]))];
}

// per-pass GPU us over time as a running multi-line graph, one line per pass. sampling happens in
// imgui_layer_draw_graph, so it keeps running while another tab is foregrounded.
static void rg_draw_timings(RenderGraphStorage& s)
{
    GpuProfiler& gp = s.m_allocator->profiler;

    if (!gp.initialized) {
        ImGui::TextDisabled("Enable \"gpu timings\" in the Demos window to record per-pass GPU time.");
        return;
    }

    if (gp.recording) { if (ImGui::Button("Stop recording")) gp.recording = false; }
    else              { if (ImGui::Button("Start recording")) gp.recording = true; }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) gp.clear_history();
    ImGui::SameLine();
    ImGui::Text("%u / %u frames%s", gp.historyLen, GpuProfiler::kHistory, gp.recording ? "  (rec)" : "");

    // peak us across the ENABLED series over the retained window, giving the Y scale
    float maxV = 0.0f;
    for (uint32_t si = 0; si < gp.seriesCount; ++si)
        if (gp.series[si].enabled)
            for (uint32_t c = 0; c < gp.historyLen; ++c)
                if (gp.series[si].v[c] > maxV) maxV = gp.series[si].v[c];
    if (maxV <= 0.0f) maxV = 1.0f;   // avoid div-by-zero before any data

    // ---- canvas ----
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float  w  = ImGui::GetContentRegionAvail().x;
    const float  h  = 200.0f;
    ImGui::InvisibleButton("rg_timings_canvas", ImVec2(w > 0 ? w : 1, h));
    const bool  canvasHovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + w, p0.y + h);
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 255), 3.0f);
    dl->AddRect(p0, p1, IM_COL32(80, 86, 100, 255), 3.0f);
    dl->PushClipRect(p0, p1, true);

    // faint mid gridline + the peak label
    dl->AddLine(ImVec2(p0.x, (p0.y + p1.y) * 0.5f), ImVec2(p1.x, (p0.y + p1.y) * 0.5f), IM_COL32(255, 255, 255, 24));
    char lbl[32];
    std::snprintf(lbl, sizeof(lbl), "%.1f us", maxV);
    dl->AddText(ImVec2(p0.x + 4, p0.y + 3), IM_COL32(180, 180, 190, 220), lbl);

    // ring geometry, shared by the line draw + the scrub cursor. valid only with >=1 column.
    const uint32_t oldest = (gp.historyHead + GpuProfiler::kHistory - gp.historyLen) % GpuProfiler::kHistory;
    const float    stepX  = w / float(GpuProfiler::kHistory - 1);
    auto colY = [&](float v) { return p1.y - (v / maxV) * (h - 4.0f) - 2.0f; };

    // snap the mouse X to the nearest column while hovering, driving the cursor line + list readout
    int scrubCol = -1;   // logical column in [0, historyLen)
    if (canvasHovered && gp.historyLen >= 1) {
        int c = (int)((ImGui::GetIO().MousePos.x - p0.x) / stepX + 0.5f);
        scrubCol = c < 0 ? 0 : (c > (int)gp.historyLen - 1 ? (int)gp.historyLen - 1 : c);
    }

    // oldest valid column in the ring, so the draw runs left to right
    if (gp.historyLen >= 2) {
        static ImVec2 pts[GpuProfiler::kHistory];
        for (uint32_t si = 0; si < gp.seriesCount; ++si) {
            if (!gp.series[si].enabled) continue;
            for (uint32_t c = 0; c < gp.historyLen; ++c)
                pts[c] = ImVec2(p0.x + c * stepX, colY(gp.series[si].v[(oldest + c) % GpuProfiler::kHistory]));
            dl->AddPolyline(pts, (int)gp.historyLen, rg_series_color(si), 0, 1.5f);
        }
    }

    // vertical line + a dot on each enabled series at the snapped column
    if (scrubCol >= 0) {
        const float cx = p0.x + scrubCol * stepX;
        dl->AddLine(ImVec2(cx, p0.y), ImVec2(cx, p1.y), IM_COL32(230, 230, 235, 160));
        const uint32_t col = (oldest + (uint32_t)scrubCol) % GpuProfiler::kHistory;
        for (uint32_t si = 0; si < gp.seriesCount; ++si)
            if (gp.series[si].enabled)
                dl->AddCircleFilled(ImVec2(cx, colY(gp.series[si].v[col])), 2.5f, rg_series_color(si));
    }
    dl->PopClipRect();

    // the list reads the scrubbed column while hovering, else the latest sample
    const uint32_t readCol = gp.historyLen
        ? (oldest + (uint32_t)(scrubCol >= 0 ? scrubCol : (int)gp.historyLen - 1)) % GpuProfiler::kHistory
        : 0;
    ImGui::Spacing();
    if (scrubCol >= 0) ImGui::TextDisabled("scrub: %d frames ago", (int)gp.historyLen - 1 - scrubCol);
    else               ImGui::TextDisabled("latest sample (hover the graph to scrub)");

    // ---- per-pass list: color swatch + enable checkbox + value at readCol ----
    for (uint32_t si = 0; si < gp.seriesCount; ++si) {
        const ImVec2 cur = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(ImVec2(cur.x, cur.y + 3), ImVec2(cur.x + 11, cur.y + 14), rg_series_color(si), 2.0f);
        ImGui::Dummy(ImVec2(15, 0));
        ImGui::SameLine();
        char id[80];
        std::snprintf(id, sizeof(id), "%.*s##series%u",
            (int)gp.series[si].name.length, gp.series[si].name.data ? gp.series[si].name.data : "", si);
        ImGui::Checkbox(id, &gp.series[si].enabled);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70.0f);
        ImGui::Text("%6.1f us", gp.historyLen ? gp.series[si].v[readCol] : 0.0f);
    }
}

namespace webgpu_app {

void RenderGraphPanel::draw()
{
    // consume the frame's graph unconditionally, its nodes die at the next begin_frame() and a hidden
    // window must not leave the global dangling
    webgpu::rg::RenderGraph* rg = g_render_graph;
    g_render_graph = nullptr;

    static uint32_t render_gaph_enabled = 0;
    if(ImGuiManager::FloatingToggleButton("ToggleRenderGraphButton", ICON_FA_PROJECT_DIAGRAM, "RenderGraph", &render_gaph_enabled))
    {
        render_gaph_enabled &= 1;
    }

    if(render_gaph_enabled == 0)
        return;

    ImGui::Begin("RenderGraph");

    // reachable whether or not a graph ran this frame, so the user can switch back from the legacy path,
    // which feeds no graph
    ImGui::Checkbox("Drive frame from render graph", &g_use_render_graph);
    ImGui::Separator();

    if (!rg) {
        ImGui::TextUnformatted("Render graph inactive (legacy render path).");
        ImGui::End();
        return;
    }

    RenderGraphStorage& s = *storage(rg);

    // append a history column each frame the window draws, whichever tab is open
    s.m_allocator->profiler.sample_history();

    ImGui::Text(" %1.1f FPS (%1.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Text(" compile %1.0f us  realize %1.0f us  execute %1.0f us", s.timing_compile_us, s.timing_realize_us, s.timing_execute_us);

    const bool hasErrors = rg->get_errors() != nullptr;
    if (ErrorMessage* e = rg->get_errors()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 90, 80, 255));
        ImGui::TextUnformatted("compile failed:");
        for (; e; e = e->next)
            ImGui::TextWrapped("  %.*s", (int)e->message.length, e->message.data ? e->message.data : "");
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    // opt-in: total of the last completed per-pass timestamp read-back, breakdown on hover
    if (const GpuProfiler& gp = s.m_allocator->profiler; gp.resultCount) {
        float totalUs = 0.0f;
        for (uint32_t i = 0; i < gp.resultCount; ++i)
            totalUs += gp.resultUs[i];
        ImGui::SameLine();
        ImGui::Text("  gpu %.2f ms", totalUs / 1000.0f);
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
            for (uint32_t i = 0; i < gp.resultCount; ++i)
                ImGui::Text("%6.1f us  %.*s", gp.resultUs[i],
                    (int)gp.resultNames[i].length, gp.resultNames[i].data ? gp.resultNames[i].data : "");
            ImGui::EndTooltip();
        }
    }

    rg_draw_arena(*s.m_allocator);
    ImGui::Separator();

    // a failed compile never realized: no slots, no pool entries, adjacency half-built. those views would
    // render misleading empties, so gate them behind a clean graph and let the errors stand alone.
    if (hasErrors) {
        ImGui::TextDisabled("Graph, Lifetimes and Memory views are hidden until the errors above are fixed.");
    }
    else if (ImGui::BeginTabBar("rg_tabs")) {
        if (ImGui::BeginTabItem("Graph")) {
            rg_draw_dag(rg, s);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Lifetimes")) {
            rg_draw_lifetimes(rg, s);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Memory")) {
            rg_draw_memory(s);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Timings")) {
            rg_draw_timings(s);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

} // namespace webgpu_app

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
