/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2022-2023 Elie Michel and the wgpu-native authors
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

/**
 * This is an extension of SDL for WebGPU, abstracting away the details of
 * OS-specific operations.
 *
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://eliemichel.github.io/LearnWebGPU
 *
 * Most of this code comes from the wgpu-native triangle example:
 *   https://github.com/gfx-rs/wgpu-native/blob/master/examples/triangle/main.c
 *
 * MIT License
 * Copyright (c) 2022-2023 Elie Michel and the wgpu-native authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "webgpu_interface.hpp"

#include "util/string_cast.h"
#include <QDebug>
#include <assert.h>
#include <webgpu/webgpu.h>

#ifdef __EMSCRIPTEN__
// needed for emscripten_sleep
#include <emscripten/emscripten.h>
#include <emscripten/val.h>
#else
// include Dawn only for non-emscripten build
#include <dawn/native/DawnNative.h>
#include <thread>
#endif


#define WGPU_TARGET_MACOS 1
#define WGPU_TARGET_LINUX_X11 2
#define WGPU_TARGET_WINDOWS 3
#define WGPU_TARGET_LINUX_WAYLAND 4
#define WGPU_TARGET_EMSCRIPTEN 5

#if defined(SDL_VIDEO_DRIVER_COCOA)
#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#elif defined(SDL_VIDEO_DRIVER_UIKIT)
#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>
#include <UIKit/UIKit.h>
#endif

#include <SDL2/SDL_syswm.h>

WGPUSurface SDL_GetWGPUSurface(WGPUInstance instance, SDL_Window* window)
{
    SDL_SysWMinfo windowWMInfo;
    SDL_VERSION(&windowWMInfo.version);
    SDL_GetWindowWMInfo(window, &windowWMInfo);

#if defined(SDL_VIDEO_DRIVER_COCOA)
    {
        id metal_layer = NULL;
        NSWindow* ns_window = windowWMInfo.info.cocoa.window;
        [ns_window.contentView setWantsLayer:YES];
        metal_layer = [CAMetalLayer layer];
        [ns_window.contentView setLayer:metal_layer];

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
#else
        WGPUSurfaceDescriptorFromMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
#endif
        fromMetalLayer.chain.next = NULL;
        fromMetalLayer.layer = metal_layer;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromMetalLayer.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif defined(SDL_VIDEO_DRIVER_UIKIT)
    {
        UIWindow* ui_window = windowWMInfo.info.uikit.window;
        UIView* ui_view = ui_window.rootViewController.view;
        CAMetalLayer* metal_layer = [CAMetalLayer new];
        metal_layer.opaque = true;
        metal_layer.frame = ui_view.frame;
        metal_layer.drawableSize = ui_view.frame.size;

        [ui_view.layer addSublayer:metal_layer];

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
#else
        WGPUSurfaceDescriptorFromMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
#endif
        fromMetalLayer.chain.next = NULL;
        fromMetalLayer.layer = metal_layer;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromMetalLayer.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif defined(SDL_VIDEO_DRIVER_X11)
    {
        Display* x11_display = windowWMInfo.info.x11.display;
        Window x11_window = windowWMInfo.info.x11.window;

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceXlibWindow fromXlibWindow;
        fromXlibWindow.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
#else
        WGPUSurfaceDescriptorFromXlibWindow fromXlibWindow;
        fromXlibWindow.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
#endif
        fromXlibWindow.chain.next = NULL;
        fromXlibWindow.display = x11_display;
        fromXlibWindow.window = x11_window;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromXlibWindow.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif defined(SDL_VIDEO_DRIVER_WAYLAND)
    {
        struct wl_display* wayland_display = windowWMInfo.info.wl.display;
        struct wl_surface* wayland_surface = windowWMInfo.info.wl.surface;

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceWaylandSurface fromWaylandSurface;
        fromWaylandSurface.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
#else
        WGPUSurfaceDescriptorFromWaylandSurface fromWaylandSurface;
        fromWaylandSurface.chain.sType = WGPUSType_SurfaceDescriptorFromWaylandSurface;
#endif
        fromWaylandSurface.chain.next = NULL;
        fromWaylandSurface.display = wayland_display;
        fromWaylandSurface.surface = wayland_surface;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromWaylandSurface.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif defined(SDL_VIDEO_DRIVER_WINDOWS)
    {
        HWND hwnd = windowWMInfo.info.win.window;
        HINSTANCE hinstance = GetModuleHandle(NULL);

        WGPUSurfaceSourceWindowsHWND hwndDesc {};
        hwndDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        hwndDesc.chain.next = NULL;
        hwndDesc.hinstance = hinstance;
        hwndDesc.hwnd = hwnd;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &hwndDesc.chain;
        surfaceDescriptor.label = WGPUStringView { .data = "default surface", .length = WGPU_STRLEN };

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif defined(SDL_VIDEO_DRIVER_EMSCRIPTEN)
    {
        WGPUEmscriptenSurfaceSourceCanvasHTMLSelector fromCanvasHTMLSelector {};
        fromCanvasHTMLSelector.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
        fromCanvasHTMLSelector.chain.next = nullptr;
        fromCanvasHTMLSelector.selector = WGPUStringView { .data = "#canvas", .length = WGPU_STRLEN };

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromCanvasHTMLSelector.chain;
        surfaceDescriptor.label = WGPUStringView { .data = "default surface", .length = WGPU_STRLEN };

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#else
// TODO: See SDL_syswm.h for other possible enum values!
#error "Unsupported WGPU_TARGET"
#endif
}

namespace webgpu {

bool timerSupportFlag = false;
std::atomic_int sleeping_counter = 0;

// NOTE: USE WITH CAUTION!
void sleep([[maybe_unused]] const WGPUDevice& device, [[maybe_unused]] int milliseconds)
{
    sleeping_counter++;
#ifdef __EMSCRIPTEN__
    emscripten_sleep(1); // using asyncify to return to js event loop
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    wgpuDeviceTick(device); // polling events for DAWN
#endif
    sleeping_counter--;
}

bool isSleeping() { return sleeping_counter > 0; }

void waitForFlag(const WGPUDevice& device, bool* flag, int sleepInterval, int timeout)
{
    int time = 0;
    while (!*flag) {
        webgpu::sleep(device, sleepInterval);
        time += sleepInterval;
        if (time > timeout) {
            qCritical() << "Timeout while waiting for flag";
            return;
        }
    }
}

void checkForTimingSupport(const WGPUAdapter& adapter, const WGPUDevice& device)
{
    // Check wether timing is supported
#ifdef __EMSCRIPTEN__
    timerSupportFlag = false;
    emscripten::val module_value = emscripten::val::module_property("webgpuTimingsAvailable");
    if (!module_value.isUndefined()) {
        timerSupportFlag = (module_value.as<int>() == 1);
        if (!timerSupportFlag)
            qWarning() << "Timestamp queries are not supported! (JS based check failed)";
    } else {
        qCritical() << "Timestamp query flag couldn't be found as a module property!";
    }
#else
    timerSupportFlag = true;
    if (!wgpuAdapterHasFeature(adapter, WGPUFeatureName_TimestampQuery)) {
        qWarning() << "Timestamp queries are not supported! (Not necessary adapter feature)";
        timerSupportFlag = false;
    } else if (!wgpuDeviceHasFeature(device, WGPUFeatureName_TimestampQuery)) {
        qWarning() << "Timestamp queries are not supported! (Not necessary device feature)";
        timerSupportFlag = false;
    }
#endif
}

bool isTimingSupported() { return timerSupportFlag; }

// Request webgpu adapter synchronously. Adapted from webgpu.hpp to vanilla webGPU types.
WGPUAdapter requestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions& options)
{
    struct AdapterRequestEndedData {
        WGPUAdapter adapter = nullptr;
        bool request_ended = false;
    } request_ended_data;

    auto on_adapter_request_ended
        = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              AdapterRequestEndedData* request_ended_data = reinterpret_cast<AdapterRequestEndedData*>(userdata);
              if (status == WGPURequestAdapterStatus::WGPURequestAdapterStatus_Success) {
                  request_ended_data->adapter = adapter;
              } else {
                  request_ended_data->adapter = nullptr;
              }
              request_ended_data->request_ended = true;
          };

    WGPURequestAdapterCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = on_adapter_request_ended,
        .userdata1 = &request_ended_data,
        .userdata2 = nullptr,
    };

    WGPUFuture request_adapter_future = wgpuInstanceRequestAdapter(instance, &options, callback_info);
    WGPUFutureWaitInfo future_wait_info { request_adapter_future, false };
    WGPUWaitStatus status = wgpuInstanceWaitAny(instance, 1, &future_wait_info, 10e9); // wait for 10s max
    if (status != WGPUWaitStatus_Success) {
        qFatal() << "Failed to obtain instance, WGPUWaitStatus was " << status;
    }

    assert(request_ended_data.request_ended);
    return request_ended_data.adapter;
}

// Request webgpu device synchronously. Adapted from webgpu.hpp to vanilla webGPU types.
WGPUDevice requestDeviceSync(WGPUInstance instance, WGPUAdapter adapter, const WGPUDeviceDescriptor& descriptor)
{
    WGPUDevice device = nullptr;

    auto on_device_request_ended
        = [](WGPURequestDeviceStatus status, WGPUDevice device, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              WGPUDevice* device_handle = reinterpret_cast<WGPUDevice*>(userdata);

              if (status == WGPURequestDeviceStatus::WGPURequestDeviceStatus_Success) {
                  *device_handle = device;
              } else {
                  qCritical() << "requesting WebGPU device failed, error message: " << message.data;
              }
          };

    WGPURequestDeviceCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = on_device_request_ended,
        .userdata1 = &device,
        .userdata2 = nullptr,
    };

    WGPUFuture device_request_future = wgpuAdapterRequestDevice(adapter, &descriptor, callback_info);
    WGPUFutureWaitInfo future_wait_info { device_request_future, false };
    WGPUWaitStatus status = wgpuInstanceWaitAny(instance, 1, &future_wait_info, 10e9); // timeout 10s
    if (status != WGPUWaitStatus_Success) {
        qFatal() << "Failed to obtain Webgpu device, WGPUWaitStatus was " << status;
    }

    return device;
}

} // namespace webgpu
