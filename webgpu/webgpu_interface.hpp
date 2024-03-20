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


/* NOTE: This file offers platform-specific operations for WebGPU, depending on the
 * target (web/native), such that the code in webgpu_app and webgpu_engine can
 * be kept as generic as possible.
 */
#pragma once

#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>

extern "C" {

/**
 * Get a WGPUSurface from a GLFW window.
 */
WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

/**
 * Do platform specific stuff before webgpu is used. This function needs to be
 * called before any other webgpu function.
 */
void webgpuPlatformInit();

WGPUAdapter requestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions& options);
WGPUDevice requestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor& descriptor);
}
