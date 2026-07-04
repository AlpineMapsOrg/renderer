/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include <type_traits>
#include <webgpu/webgpu.h>

namespace webgpu::raii {

template <typename HandleT, typename DescriptorT, typename ContextT> struct GpuFuncs {
public:
    // is_same_v needed to prevent evaluation of default template instantiation
    // see https://stackoverflow.com/questions/5246049/c11-static-assert-and-template-instantiation
    static HandleT create(ContextT, const DescriptorT&) { static_assert(std::is_same_v<HandleT, HandleT>, "default instantiation should not be used!"); };
    static void release(HandleT) { static_assert(std::is_same_v<HandleT, HandleT>, "default instantiation should not be used!"); }
};

template <> struct GpuFuncs<WGPUTexture, WGPUTextureDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateTexture(context, &descriptor); }
    static void release(auto handle)
    {
        wgpuTextureDestroy(handle);
        wgpuTextureRelease(handle);
    }
};

template <> struct GpuFuncs<WGPUTextureView, WGPUTextureViewDescriptor, WGPUTexture> {
    static auto create(auto context, auto descriptor) { return wgpuTextureCreateView(context, &descriptor); }
    static void release(auto handle) { wgpuTextureViewRelease(handle); }
};

template <> struct GpuFuncs<WGPUSampler, WGPUSamplerDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateSampler(context, &descriptor); }
    static void release(auto handle) { wgpuSamplerRelease(handle); }
};

template <> struct GpuFuncs<WGPUBuffer, WGPUBufferDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateBuffer(context, &descriptor); }
    static void release(auto handle)
    {
        wgpuBufferDestroy(handle);
        wgpuBufferRelease(handle);
    }
};

template <> struct GpuFuncs<WGPUShaderModule, WGPUShaderModuleDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateShaderModule(context, &descriptor); }
    static void release(auto handle) { wgpuShaderModuleRelease(handle); }
};

template <> struct GpuFuncs<WGPUBindGroup, WGPUBindGroupDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateBindGroup(context, &descriptor); }
    static void release(auto handle) { wgpuBindGroupRelease(handle); }
};

template <> struct GpuFuncs<WGPUBindGroupLayout, WGPUBindGroupLayoutDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateBindGroupLayout(context, &descriptor); }
    static void release(auto handle) { wgpuBindGroupLayoutRelease(handle); }
};

template <> struct GpuFuncs<WGPUPipelineLayout, WGPUPipelineLayoutDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreatePipelineLayout(context, &descriptor); }
    static void release(auto handle) { wgpuPipelineLayoutRelease(handle); }
};

template <> struct GpuFuncs<WGPURenderPipeline, WGPURenderPipelineDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateRenderPipeline(context, &descriptor); }
    static void release(auto handle) { wgpuRenderPipelineRelease(handle); }
};

template <> struct GpuFuncs<WGPURenderPassEncoder, WGPURenderPassDescriptor, WGPUCommandEncoder> {
    static auto create(auto context, auto descriptor) { return wgpuCommandEncoderBeginRenderPass(context, &descriptor); }
    static void release(auto handle)
    {
        wgpuRenderPassEncoderEnd(handle);
        wgpuRenderPassEncoderRelease(handle);
    }
};

template <> struct GpuFuncs<WGPUComputePassEncoder, WGPUComputePassDescriptor, WGPUCommandEncoder> {
    static auto create(auto context, auto descriptor) { return wgpuCommandEncoderBeginComputePass(context, &descriptor); }
    static void release(auto handle)
    {
        wgpuComputePassEncoderEnd(handle);
        wgpuComputePassEncoderRelease(handle);
    }
};

template <> struct GpuFuncs<WGPUComputePipeline, WGPUComputePipelineDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateComputePipeline(context, &descriptor); }
    static void release(auto handle) { wgpuComputePipelineRelease(handle); }
};

template <> struct GpuFuncs<WGPUCommandEncoder, WGPUCommandEncoderDescriptor, WGPUDevice> {
    static auto create(auto context, auto descriptor) { return wgpuDeviceCreateCommandEncoder(context, &descriptor); }
    static void release(auto handle) { wgpuCommandEncoderRelease(handle); }
};

/// TODO document
/// Represents a (web)GPU render pipeline object.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
template <typename HandleT, typename DescriptorT, typename ContextHandleT> class GpuResource {
public:
    GpuResource(ContextHandleT context, const DescriptorT& descriptor)
        : m_handle(GpuFuncs<HandleT, DescriptorT, ContextHandleT>::create(context, descriptor))
        , m_descriptor(descriptor)
    {
        // Warning/ToDo: m_descriptor might contain pointers to memory that is managed by the caller.
        // To make sure all of the pointer types inside the descriptor should be set to nullptr.
        // It might be possible with some dark template magic. If not we'll leave it as is, but
        // need to be aware that such pointers might cause issues.
    }

    ~GpuResource() { GpuFuncs<HandleT, DescriptorT, ContextHandleT>::release(m_handle); }

    // delete copy constructor and copy-assignment operator
    GpuResource(const GpuResource& other) = delete;
    GpuResource& operator=(const GpuResource& other) = delete;

    HandleT handle() const { return m_handle; }
    DescriptorT descriptor() const { return m_descriptor; }

protected:
    HandleT m_handle;
    DescriptorT m_descriptor;
};

// using BindGroup = GpuResource<WGPUBindGroup, WGPUBindGroupDescriptor, WGPUDevice>;
// using BindGroupLayout = GpuResource<WGPUBindGroupLayout, WGPUBindGroupLayoutDescriptor, WGPUDevice>;
using ShaderModule = GpuResource<WGPUShaderModule, WGPUShaderModuleDescriptor, WGPUDevice>;
// using PipelineLayout = GpuResource<WGPUPipelineLayout, WGPUPipelineLayoutDescriptor, WGPUDevice>;
using RenderPipeline = GpuResource<WGPURenderPipeline, WGPURenderPipelineDescriptor, WGPUDevice>;
// using Buffer = GpuResource<WGPUBuffer, WGPUBufferDescriptor, WGPUDevice>;
// using Texture = GpuResource<WGPUTexture, WGPUTextureDescriptor, WGPUDevice>;
// using TextureView = GpuResource<WGPUTextureView, WGPUTextureViewDescriptor, WGPUTexture>;
// using Sampler = GpuResource<WGPUSampler, WGPUSamplerDescriptor, WGPUDevice>;
// using RenderPassEncoder = GpuResource<WGPURenderPassEncoder, WGPURenderPassDescriptor, WGPUCommandEncoder>;
using ComputePipeline = GpuResource<WGPUComputePipeline, WGPUComputePipelineDescriptor, WGPUDevice>;
using ComputePassEncoder = GpuResource<WGPUComputePassEncoder, WGPUComputePassDescriptor, WGPUCommandEncoder>;
using CommandEncoder = GpuResource<WGPUCommandEncoder, WGPUCommandEncoderDescriptor, WGPUDevice>;

// Also SwapChain is different because it needs Device and surface as parameters...
//using SwapChain = GpuResource<WGPUSwapChain, WGPUSwapChainDescriptor, WGPUDevice>;

// Surface is not as easy, as we get it from sdl
// using Surface = GpuResource<WGPUSurface, WGPUSurfaceDescriptor, WGPUInstance>;

} // namespace webgpu::raii
