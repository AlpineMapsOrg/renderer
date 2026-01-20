/*****************************************************************************
 * Alpine Renderer
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

#include "UnittestWebgpuContext.h"
#include "webgpu/webgpu_interface.hpp"
#include <cassert>
#include <iostream>
#include <limits>

WGPULimits UnittestWebgpuContext::default_limits()
{
    WGPULimits required_limits {};
    WGPULimits supported_limits {};
    wgpuAdapterGetLimits(adapter, &supported_limits);
    // irrelevant for us, but needs to be set
    required_limits.minStorageBufferOffsetAlignment = supported_limits.minStorageBufferOffsetAlignment;
    required_limits.minUniformBufferOffsetAlignment = supported_limits.minUniformBufferOffsetAlignment;
    required_limits.maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED; // required for current version of  Chrome Canary (2025-04-03)
    return required_limits;
}

void webgpu_device_error_callback(
    [[maybe_unused]] const WGPUDevice* device, WGPUErrorType type, WGPUStringView message, [[maybe_unused]] void* userdata1, [[maybe_unused]] void* userdata2)
{
    const char* typeStr = "Unknown";
    switch (type) {
        case WGPUErrorType_NoError: typeStr = "NoError"; break;
        case WGPUErrorType_Validation: typeStr = "Validation"; break;
        case WGPUErrorType_OutOfMemory: typeStr = "OutOfMemory"; break;
        case WGPUErrorType_Internal: typeStr = "Internal"; break;
        case WGPUErrorType_Unknown: typeStr = "Unknown"; break;
        default: break;
    }

    std::cout << "WebGPU Error [" << typeStr << "]: " << std::string_view(message.data, message.length) << std::endl;
}

void webgpu_device_lost_callback([[maybe_unused]] const WGPUDevice* device,
    WGPUDeviceLostReason reason,
    WGPUStringView message,
    [[maybe_unused]] void* userdata1,
    [[maybe_unused]] void* userdata2)
{
    const char* typeStr = "Unknown";
    switch (reason) {
        case WGPUDeviceLostReason_Unknown: typeStr = "Unknown"; break;
        case WGPUDeviceLostReason_Destroyed: typeStr = "Destroyed"; break;
        case WGPUDeviceLostReason_CallbackCancelled: typeStr = "CallbackCancelled"; break;
        case WGPUDeviceLostReason_FailedCreation: typeStr = "FailedCreation"; break;
        default: break;
    }

    std::cout << "WebGPU Device Lost [" << typeStr << "]: " << std::string_view(message.data, message.length) << std::endl;
}

UnittestWebgpuContext::UnittestWebgpuContext(bool use_default_limits, WGPULimits required_limits)
{
    instance_desc = {};
    instance_desc.nextInChain = nullptr;

#ifndef __EMSCRIPTEN__
    WGPUDawnTogglesDescriptor dawnToggles;
    dawnToggles.chain.next = nullptr;
    dawnToggles.chain.sType = WGPUSType_DawnTogglesDescriptor;

    std::vector<const char*> enabledToggles = { "allow_unsafe_apis" };

    dawnToggles.enabledToggles = enabledToggles.data();
    dawnToggles.enabledToggleCount = enabledToggles.size();
    dawnToggles.disabledToggleCount = 0;

    instance_desc.nextInChain = &dawnToggles.chain;
#endif

    const auto timed_wait_feature = WGPUInstanceFeatureName_TimedWaitAny;
    instance_desc.requiredFeatureCount = 1;
    instance_desc.requiredFeatures = &timed_wait_feature;

    instance = wgpuCreateInstance(&instance_desc);
    assert(instance);

    WGPURequestAdapterOptions adapter_opts {};
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;
    adapter_opts.compatibleSurface = nullptr;
    adapter = webgpu::requestAdapterSync(instance, adapter_opts);
    assert(adapter);

    std::vector<WGPUFeatureName> requiredFeatures;
    requiredFeatures.push_back(WGPUFeatureName_TimestampQuery);

        WGPUDeviceDescriptor device_desc {};
    device_desc.label = WGPUStringView { .data = "webigeo device", .length = WGPU_STRLEN };
    device_desc.requiredFeatures = requiredFeatures.data();
    device_desc.requiredFeatureCount = (uint32_t)requiredFeatures.size();
    if (use_default_limits)
        required_limits = default_limits();
    device_desc.requiredLimits = &required_limits;
    device_desc.defaultQueue.label = WGPUStringView { .data = "webigeo queue", .length = WGPU_STRLEN };
    device_desc.uncapturedErrorCallbackInfo = WGPUUncapturedErrorCallbackInfo {
        .nextInChain = nullptr,
        .callback = webgpu_device_error_callback,
        .userdata1 = nullptr,
        .userdata2 = nullptr,
    };
    device_desc.deviceLostCallbackInfo = WGPUDeviceLostCallbackInfo {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = webgpu_device_lost_callback,
        .userdata1 = nullptr,
        .userdata2 = nullptr,
    };

    device = webgpu::requestDeviceSync(instance, adapter, device_desc);
    assert(device);

    queue = wgpuDeviceGetQueue(device);
    assert(queue);

    shader_module_manager = std::make_unique<ShaderModuleManager>(device);
    assert(shader_module_manager);
}
