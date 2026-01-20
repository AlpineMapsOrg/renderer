/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "webgpu_init.h"

#include "error_logging.h"
#include <QDebug>
#include <webgpu/webgpu_interface.hpp>

namespace webigeo_eval::util {

void update_required_gpu_limits(WGPULimits& limits, const WGPULimits& supported_limits)
{
    const uint32_t max_required_bind_groups = 4u;
    const uint32_t min_recommended_max_texture_array_layers = 1024u;
    const uint32_t min_required_max_color_attachment_bytes_per_sample = 32u;
    const uint64_t min_required_max_storage_buffer_binding_size = 268435456u;

    if (supported_limits.maxColorAttachmentBytesPerSample < min_required_max_color_attachment_bytes_per_sample) {
        qFatal() << "Minimum supported maxColorAttachmentBytesPerSample needs to be >=" << min_required_max_color_attachment_bytes_per_sample;
    }
    if (supported_limits.maxTextureArrayLayers < min_recommended_max_texture_array_layers) {
        qWarning() << "Minimum supported maxTextureArrayLayers is " << supported_limits.maxTextureArrayLayers << " (" << min_recommended_max_texture_array_layers << " recommended)!";
    }
    if (supported_limits.maxBindGroups < max_required_bind_groups) {
        qFatal() << "Maximum supported number of bind groups is " << supported_limits.maxBindGroups << " and " << max_required_bind_groups << " are required";
    }
    if (supported_limits.maxStorageBufferBindingSize < min_required_max_storage_buffer_binding_size) {
        qFatal() << "Maximum supported storage buffer binding size is " << supported_limits.maxStorageBufferBindingSize << " and " << min_required_max_storage_buffer_binding_size << " is required";
    }
    limits.maxBindGroups = std::max(limits.maxBindGroups, max_required_bind_groups);
    limits.maxColorAttachmentBytesPerSample = std::max(limits.maxColorAttachmentBytesPerSample, min_required_max_color_attachment_bytes_per_sample);
    limits.maxTextureArrayLayers = std::min(std::max(limits.maxTextureArrayLayers, min_recommended_max_texture_array_layers), supported_limits.maxTextureArrayLayers);
    limits.maxStorageBufferBindingSize = std::max(limits.maxStorageBufferBindingSize, supported_limits.maxStorageBufferBindingSize);
}

WGPUDevice init_webgpu_device(WGPUInstance instance, WGPUAdapter adapter)
{
    /*
    qDebug() << "Requesting surface...";
    WGPUSurface m_surface = SDL_GetWGPUSurface(m_instance, m_sdl_window);
    if (!m_surface) {
        qFatal("Could not create surface!");
    }
    qInfo() << "Got surface: " << m_surface;
    */

    qDebug() << "Requesting device...";
    WGPULimits required_limits {};
    WGPULimits supported_limits {};
    wgpuAdapterGetLimits(adapter, &supported_limits);

    // irrelevant for us, but needs to be set
    required_limits.minStorageBufferOffsetAlignment = supported_limits.minStorageBufferOffsetAlignment;
    required_limits.minUniformBufferOffsetAlignment = supported_limits.minUniformBufferOffsetAlignment;
    required_limits.maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED; // required for current version of  Chrome Canary (2025-04-03)

    // TODO window update limits
    update_required_gpu_limits(required_limits, supported_limits);

    std::vector<WGPUFeatureName> requiredFeatures;
    requiredFeatures.push_back(WGPUFeatureName_TimestampQuery);

    WGPUDeviceDescriptor device_desc {};
    device_desc.label = WGPUStringView { "webigeo device", WGPU_STRLEN };
    device_desc.requiredFeatures = requiredFeatures.data();
    device_desc.requiredFeatureCount = (uint32_t)requiredFeatures.size();
    device_desc.requiredLimits = &required_limits;
    device_desc.defaultQueue.label = WGPUStringView { "webigeo queue", WGPU_STRLEN };

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

    WGPUDevice device = webgpu::requestDeviceSync(instance, adapter, device_desc);
    if (!device) {
        qFatal("Could not get device!");
    }
    qInfo() << "Got device: " << device;

    webgpu::checkForTimingSupport(adapter, device);

    return device;
}

WGPUInstance init_webgpu_instance()
{
    qDebug() << "Creating WebGPU instance...";
    WGPUInstanceDescriptor instance_desc = {};
    instance_desc.nextInChain = nullptr;
    WGPUDawnTogglesDescriptor dawn_toggles;
    dawn_toggles.chain.next = nullptr;
    dawn_toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;

    std::vector<const char*> enabled_toggles = { "allow_unsafe_apis" };
#if defined(QT_DEBUG)
    // TODO: Figure out why this doesnt work
    enabled_toggles.push_back("use_user_defined_labels_in_backend");
    enabled_toggles.push_back("enable_vulkan_validation");
    enabled_toggles.push_back("disable_symbol_renaming");
#endif

    QStringList toggle_list;
    for (const auto& toggle : enabled_toggles)
        toggle_list << QString::fromStdString(toggle);
    qDebug() << "Dawn toggles:" << toggle_list.join(", ");

    dawn_toggles.enabledToggles = enabled_toggles.data();
    dawn_toggles.enabledToggleCount = enabled_toggles.size();
    dawn_toggles.disabledToggleCount = 0;
    instance_desc.nextInChain = &dawn_toggles.chain;

    const auto timed_wait_feature = WGPUInstanceFeatureName_TimedWaitAny;
    instance_desc.requiredFeatureCount = 1;
    instance_desc.requiredFeatures = &timed_wait_feature;

    WGPUInstance instance = wgpuCreateInstance(&instance_desc);

    if (!instance) {
        qFatal("Could not initialize WebGPU Instance!");
    }
    qInfo() << "Got instance: " << instance;
    return instance;
}

WGPUAdapter init_webgpu_adapter(WGPUInstance instance)
{
    qDebug() << "Requesting adapter...";
    WGPURequestAdapterOptions adapter_opts = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;
    WGPUAdapter adapter = webgpu::requestAdapterSync(instance, adapter_opts);
    if (!adapter) {
        qFatal("Could not get adapter!");
    }
    qInfo() << "Got adapter: " << adapter;

    return adapter;
}

} // namespace webigeo_eval::util
