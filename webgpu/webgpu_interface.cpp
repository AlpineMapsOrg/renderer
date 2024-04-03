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
 * This is an extension of GLFW for WebGPU, abstracting away the details of
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

#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <webgpu/webgpu.h>

#ifdef __EMSCRIPTEN__
// needed for emscripten_sleep
#include <emscripten/emscripten.h>
#else
// include Dawn only for non-emscripten build
#include <dawn/native/DawnNative.h>
#include <dawn/dawn_proc.h>
#endif


#define WGPU_TARGET_MACOS 1
#define WGPU_TARGET_LINUX_X11 2
#define WGPU_TARGET_WINDOWS 3
#define WGPU_TARGET_LINUX_WAYLAND 4
#define WGPU_TARGET_EMSCRIPTEN 5

#if defined(__EMSCRIPTEN__)
#define WGPU_TARGET WGPU_TARGET_EMSCRIPTEN
#elif defined(_WIN32)
#define WGPU_TARGET WGPU_TARGET_WINDOWS
#elif defined(__APPLE__)
#define WGPU_TARGET WGPU_TARGET_MACOS
#elif defined(_GLFW_WAYLAND)
#define WGPU_TARGET WGPU_TARGET_LINUX_WAYLAND
#else
#define WGPU_TARGET WGPU_TARGET_LINUX_X11
#endif

#if WGPU_TARGET == WGPU_TARGET_MACOS
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#include <GLFW/glfw3.h>
#if WGPU_TARGET == WGPU_TARGET_MACOS
#define GLFW_EXPOSE_NATIVE_COCOA
#elif WGPU_TARGET == WGPU_TARGET_LINUX_X11
#define GLFW_EXPOSE_NATIVE_X11
#elif WGPU_TARGET == WGPU_TARGET_LINUX_WAYLAND
#define GLFW_EXPOSE_NATIVE_WAYLAND
#elif WGPU_TARGET == WGPU_TARGET_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#if !defined(__EMSCRIPTEN__)
#include <GLFW/glfw3native.h>
#endif

WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
    (void)window; // Cast to void to suppress unused parameter warning
#if WGPU_TARGET == WGPU_TARGET_MACOS
    {
        id metal_layer = [CAMetalLayer layer];
        NSWindow* ns_window = glfwGetCocoaWindow(window);
        [ns_window.contentView setWantsLayer : YES] ;
        [ns_window.contentView setLayer : metal_layer] ;

        WGPUSurfaceDescriptorFromMetalLayer fromMetalLayer;
        fromMetalLayer.chain.next = NULL;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
        fromMetalLayer.layer = metal_layer;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromMetalLayer.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif WGPU_TARGET == WGPU_TARGET_LINUX_X11
    {
        Display* x11_display = glfwGetX11Display();
        Window x11_window = glfwGetX11Window(window);

        WGPUSurfaceDescriptorFromXlibWindow fromXlibWindow;
        fromXlibWindow.chain.next = NULL;
        fromXlibWindow.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
        fromXlibWindow.display = x11_display;
        fromXlibWindow.window = x11_window;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromXlibWindow.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif WGPU_TARGET == WGPU_TARGET_LINUX_WAYLAND
    {
        struct wl_display* wayland_display = glfwGetWaylandDisplay();
        struct wl_surface* wayland_surface = glfwGetWaylandWindow(window);

        WGPUSurfaceDescriptorFromWaylandSurface fromWaylandSurface;
        fromWaylandSurface.chain.next = NULL;
        fromWaylandSurface.chain.sType = WGPUSType_SurfaceDescriptorFromWaylandSurface;
        fromWaylandSurface.display = wayland_display;
        fromWaylandSurface.surface = wayland_surface;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromWaylandSurface.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif WGPU_TARGET == WGPU_TARGET_WINDOWS
    {
        HWND hwnd = glfwGetWin32Window(window);
        HINSTANCE hinstance = GetModuleHandle(NULL);

        WGPUSurfaceDescriptorFromWindowsHWND fromWindowsHWND;
        fromWindowsHWND.chain.next = NULL;
        fromWindowsHWND.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
        fromWindowsHWND.hinstance = hinstance;
        fromWindowsHWND.hwnd = hwnd;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromWindowsHWND.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#elif WGPU_TARGET == WGPU_TARGET_EMSCRIPTEN
    {
        printf("Creating surface from canvas\n");
        WGPUSurfaceDescriptorFromCanvasHTMLSelector fromCanvasHTMLSelector;
        fromCanvasHTMLSelector.chain.next = NULL;
        fromCanvasHTMLSelector.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        fromCanvasHTMLSelector.selector = "#webgpucanvas";

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromCanvasHTMLSelector.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#else
#error "Unsupported WGPU_TARGET"
#endif
}

void webgpuPlatformInit() {

    // Dawn forwards all wgpu* function calls to function pointer members of some struct.
    // This is stored in some (Dawn internal) variable. In our current setup, all these
    // function pointers default to nullptr, resulting in access violations when called.
    // However, we can just set these pointers explicitly to use dawn_native.
#ifndef __EMSCRIPTEN__
    dawnProcSetProcs(&(dawn::native::GetProcs()));
#endif

}

// Request webgpu adapter synchronously. Adapted from webgpu.hpp to vanilla webGPU types.
WGPUAdapter requestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions& options)
{
    struct AdapterRequestEndedData {
        WGPUAdapter adapter = nullptr;
        bool request_ended = false;
    } request_ended_data;

    auto on_adapter_request_ended = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata) {
        AdapterRequestEndedData* request_ended_data = reinterpret_cast<AdapterRequestEndedData*>(userdata);
        if (status == WGPURequestAdapterStatus::WGPURequestAdapterStatus_Success) {
            request_ended_data->adapter = adapter;
        } else {
            std::cout << "Could not get WebGPU adapter: " << message << std::endl;
        }
        request_ended_data->request_ended = true;
    };

    wgpuInstanceRequestAdapter(instance, &options, on_adapter_request_ended, &request_ended_data);

#if __EMSCRIPTEN__
    while (!request_ended_data.request_ended) {
        emscripten_sleep(100);
    }
#endif

    assert(request_ended_data.request_ended);
    return request_ended_data.adapter;
}

// Request webgpu device synchronously. Adapted from webgpu.hpp to vanilla webGPU types.
WGPUDevice requestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor& descriptor)
{
    struct DeviceRequestEndedData {
        WGPUDevice device = nullptr;
        bool request_ended = false;
    } request_ended_data;

    auto on_device_request_ended = [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata) {
        DeviceRequestEndedData* request_ended_data = reinterpret_cast<DeviceRequestEndedData*>(userdata);

        if (status == WGPURequestDeviceStatus::WGPURequestDeviceStatus_Success) {
            request_ended_data->device = device;
        } else {
            std::cout << "Could not get WebGPU device: " << message << std::endl;
        }
        request_ended_data->request_ended = true;
    };

    wgpuAdapterRequestDevice(adapter, &descriptor, on_device_request_ended, &request_ended_data);

#if __EMSCRIPTEN__
    while (!request_ended_data.request_ended) {
        emscripten_sleep(100);
    }
#endif

    assert(request_ended_data.request_ended);
    return request_ended_data.device;
}
