/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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
#include "UniformBuffer.h"
#include "UniformBufferObjects.h"

namespace webgpu_engine {

template<typename T>
UniformBuffer<T>::UniformBuffer() {

}


template <typename T>
UniformBuffer<T>::UniformBuffer::~UniformBuffer()
{
    m_buffer.release();
}

template <typename T> void UniformBuffer<T>::init(wgpu::Device& device) {
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.size = sizeof(T);
    bufferDesc.mappedAtCreation = false;
    m_buffer = device.createBuffer(bufferDesc);
    //this->update_gpu_data();
}

template <typename T> void UniformBuffer<T>::update_gpu_data(wgpu::Queue queue) {
    queue.writeBuffer(m_buffer, 0, &data, sizeof(T)); //TODO size in bytes or in num elements?
}

template <typename T> QString UniformBuffer<T>::data_as_string() {
    return ubo_as_string(data);
}

template <typename T> bool UniformBuffer<T>::data_from_string(const QString& base64String) {
    bool result = true;
    auto newData = ubo_from_string<T>(base64String,&result);
    if (result) data = newData;
    return result;
}

template<typename T>
wgpu::Buffer UniformBuffer<T>::handle() const {
    return m_buffer;
}

// IMPORTANT: All possible Template Classes need to be defined here:
template class UniformBuffer<uboSharedConfig>;
template class UniformBuffer<uboCameraConfig>;
//TODO
//template class UniformBuffer<uboShadowConfig>;
template class UniformBuffer<uboTestConfig>;


}
