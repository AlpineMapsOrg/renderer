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

#include "radix/tile.h"
#include <memory>
#include <webgpu/raii/RawBuffer.h>
#include <webgpu/webgpu.h>

namespace webgpu_engine::compute {

template <typename T, std::unsigned_integral HashType = uint16_t> HashType gpu_hash(const T&)
{
    static_assert(sizeof(T) != sizeof(T), "default implementation should not be called, add a template specialization for your types");
}
/// specialization for generating uint16_t hashes from radix::tile::Id
template <> uint16_t gpu_hash<radix::tile::Id, uint16_t>(const radix::tile::Id& id);

template <typename T, typename HashType = uint16_t>
concept GpuHashable = std::unsigned_integral<HashType> && requires(T a) {
    {
        gpu_hash<T, HashType>(a)
    } -> std::same_as<HashType>;
};

/// Hashmap storing values on the GPU.
///
/// Keys are hashed using free template function
///
///     webgpu_engine::compute::gpu_hash<KeyType, HashType>(const KeyType&) -> HashType
///
/// To add a new type usable as KeyType, you need to add a template specialization for webgpu_engine::compute::gpu_hash.
///
/// KeyType needs to be convertible to GpuKeyType (either through conversion function or converting constructor).
/// ValueType needs to be convertible to GpuValueType (either through conversion function or converting constructor).
///
/// Usage: see unit test test_GpuHashMap.cpp
template <typename KeyType, typename ValueType, typename GpuKeyType = KeyType, typename GpuValueType = ValueType, typename HashType = uint16_t>
requires GpuHashable<KeyType, HashType>
class GpuHashMap {
public:
    GpuHashMap(WGPUDevice device, const KeyType& empty_key, const ValueType& empty_value)
        : m_device { device }
        , m_queue { wgpuDeviceGetQueue(device) }
        , m_capacity { std::numeric_limits<HashType>::max() + 1 }
        , m_empty_gpu_key { empty_key }
        , m_empty_gpu_value { empty_value }
        , m_stored_map(m_capacity)
        , m_id_map { std::make_unique<webgpu::raii::RawBuffer<GpuKeyType>>(
              m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc, m_capacity, "hashmap id map buffer") }
        , m_value_map { std::make_unique<webgpu::raii::RawBuffer<GpuValueType>>(
              m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc, m_capacity, "hashmap value map buffer") }
    {
    }

    /// Stores value at id.
    /// Need to call update_gpu_data for effects to be visible on GPU side.
    void store(const KeyType& id, const ValueType& value) { m_stored_map[id] = value; }

    /// Clears value at id.
    /// Need to call update_gpu_data for effects to be visible on GPU side.
    void clear(const KeyType& id) { m_stored_map.erase(id); }

    /// Clears all values.
    /// Need to call update_gpu_data for effects to be visible on GPU side.
    void clear() { m_stored_map.clear(); }

    const ValueType& value_at(const KeyType& id) const { return m_stored_map.at(id); }

    const KeyType& key_with_value(const ValueType& value) const
    {
        // Iterate through all key value pairs and return the id for the given value
        for (const auto& key_value_pair : m_stored_map) {
            if (key_value_pair.second == value) {
                return key_value_pair.first;
            }
        }
        return KeyType {};
    }

    /// Update GPU buffers.
    void update_gpu_data()
    {
        assert(m_stored_map.size() <= m_capacity);

        // hash keys and write key/value into calculated position in key/value vector
        std::vector<GpuKeyType> keys(m_capacity, GpuKeyType(m_empty_gpu_key));
        std::vector<GpuValueType> values(m_capacity, GpuValueType(m_empty_gpu_value));
        for (const auto& key_value_pair : m_stored_map) {
            HashType hash = gpu_hash<KeyType, HashType>(key_value_pair.first);
            while (keys.at(hash) != m_empty_gpu_key) {
                hash++;
            }
            keys[hash] = GpuKeyType(key_value_pair.first);
            values[hash] = GpuValueType(key_value_pair.second);
        }

        // update GPU buffers
        m_id_map->write(m_queue, keys.data(), keys.size());
        m_value_map->write(m_queue, values.data(), values.size());
    }

    webgpu::raii::RawBuffer<GpuKeyType>& key_buffer() { return *m_id_map; }
    webgpu::raii::RawBuffer<GpuValueType>& value_buffer() { return *m_value_map; }

    const webgpu::raii::RawBuffer<GpuKeyType>& key_buffer() const { return *m_id_map; }
    const webgpu::raii::RawBuffer<GpuValueType>& value_buffer() const { return *m_value_map; }

private:
    WGPUDevice m_device;
    WGPUQueue m_queue;

    uint32_t m_capacity;
    KeyType m_empty_gpu_key;
    ValueType m_empty_gpu_value;
    // TODO for now KeyType::Hasher works for radix::tile::Id only
    //  either make this a concept and require it for KeyType or required std::hash<KeyType>
    //  or manage using vector of pairs instead of hashmap (tho that would be slower, probably)
    std::unordered_map<KeyType, ValueType, typename KeyType::Hasher> m_stored_map;

    std::unique_ptr<webgpu::raii::RawBuffer<GpuKeyType>> m_id_map;
    std::unique_ptr<webgpu::raii::RawBuffer<GpuValueType>> m_value_map;
};

} // namespace webgpu_engine::compute
