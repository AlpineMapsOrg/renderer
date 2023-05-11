/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include <algorithm>
#include <vector>
#include <filesystem>

#include <QFile>
#include <fmt/format.h>
#include <zpp_bits.h>

#include "sherpa/tile.h"
#include "tile_types.h"
#include "utils.h"

namespace glm {

template<typename T>
constexpr auto serialize(auto & archive, const glm::vec<2, T> & vec)
{
    return archive(vec.x, vec.y);
}

template<typename T>
constexpr auto serialize(auto & archive, glm::vec<2, T> & vec)
{
    return archive(vec.x, vec.y);
}

}

namespace nucleus::tile_scheduler {

template <tile_types::NamedTile T>
class Cache {
    struct CacheObject {
        uint64_t stamp;
        T data;
    };

    std::unordered_map<tile::Id, CacheObject, tile::Id::Hasher> m_data;
    unsigned m_capacity = 5000;

public:
    Cache() = default;
    void insert(const std::vector<T>& tiles);
    [[nodiscard]] bool contains(const tile::Id& id) const;
    void set_capacity(unsigned capacity);
    [[nodiscard]] unsigned n_cached_objects() const;
    template <typename VisitorFunction>
    void visit(const VisitorFunction& functor); // functor should return true, if the given tile should be marked visited. stops descending if false is returned.
    const T& peak_at(const tile::Id& id) const;
    std::vector<T> purge();

    void write_to_disk(const std::filesystem::path& path);
    void read_from_disk(const std::filesystem::path& path);

private:
    template <typename VisitorFunction>
    void visit(const tile::Id& start_node, const VisitorFunction& functor, uint64_t stamp); // functor should return true, if the given tile should be marked visited. stops descending if false is returned.
};

template <tile_types::NamedTile T>
void Cache<T>::insert(const std::vector<T>& tiles)
{
    const auto stamp = utils::time_since_epoch();
    for (const T& t : tiles) {
        m_data[t.id].stamp = stamp * 100 - t.id.zoom_level;
        m_data[t.id].data = t;
    }
}

template <tile_types::NamedTile T>
bool Cache<T>::contains(const tile::Id& id) const
{
    return m_data.contains(id);
}

template <tile_types::NamedTile T>
void Cache<T>::set_capacity(unsigned int capacity)
{
    m_capacity = capacity;
}

template <tile_types::NamedTile T>
unsigned int Cache<T>::n_cached_objects() const
{
    return m_data.size();
}

template <tile_types::NamedTile T>
const T& Cache<T>::peak_at(const tile::Id& id) const
{
    return m_data.at(id).data;
}

//template<tile_types::NamedTile T>
//void Cache<T>::write_to_disk(const std::filesystem::path& path)
//{
//    unsigned version = 0;
//    QByteArray data;
//    zpp::bits::out out(data);
//    out(version).or_throw();
//    out(m_data).or_throw();

//    QFile file(path);
//    const auto success = file.open(QIODeviceBase::WriteOnly);
//    if (!success)
//        throw std::runtime_error("Couldn't open cache file for writing!");

//    file.write(data);
//}

template<tile_types::NamedTile T>
void Cache<T>::write_to_disk(const std::filesystem::path& path) {
    assert(tile_types::SerialisableTile<T>);
    QFile file(path);
    const auto success = file.open(QIODeviceBase::WriteOnly);
    if (!success)
        throw std::runtime_error("Couldn't open file for writing!");

    {
        std::vector<char> meta_data_bytes;
        zpp::bits::out out(meta_data_bytes);
        const std::remove_cvref_t<decltype(T::version_information)> version = T::version_information;
        out(version).or_throw();
        u_int32_t n_tiles = m_data.size();
        out(n_tiles).or_throw();
        file.write(meta_data_bytes.data(), qint64(meta_data_bytes.size()));
    }
    std::vector<char> size_info_bytes;
    std::vector<char> cache_object_bytes;
    for (const auto& item : m_data) {
        zpp::bits::out out(cache_object_bytes);
        zpp::bits::out out_size(size_info_bytes);
        out(item.second).or_throw();
        const auto bytes_written = u_int32_t(cache_object_bytes.size());
        out_size(bytes_written).or_throw();
        file.write(size_info_bytes.data(), qint64(size_info_bytes.size()));
        file.write(cache_object_bytes.data(), qint64(cache_object_bytes.size()));
        cache_object_bytes.resize(0);
        size_info_bytes.resize(0);
    }
}

//template<tile_types::NamedTile T>
//void Cache<T>::read_from_disk(const std::filesystem::__cxx11::path& path)
//{

//    QFile file(path);
//    const auto success = file.open(QIODeviceBase::ReadOnly);
//    if (!success)
//        throw std::runtime_error("Couldn't open cache file for reading!");
//    const auto data = file.readAll();
//    zpp::bits::in in(data);
//    auto version = unsigned(-1);
//    in(version).or_throw();
//    if (version != 0)
//        throw std::runtime_error("Cache file has incompatible version.");
//    in(m_data).or_throw();
//}

template<tile_types::NamedTile T>
void Cache<T>::read_from_disk(const std::filesystem::path& path)
{
    assert(tile_types::SerialisableTile<T>);
    QFile file(path);
    const auto success = file.open(QIODeviceBase::ReadOnly);
    if (!success)
        throw std::runtime_error("Couldn't open cache file for reading!");

    {
        std::remove_cvref_t<decltype(T::version_information)> version_info = {};
        auto n_tiles = u_int32_t(-1);
        std::vector<char> meta_data_bytes (sizeof(version_info) + sizeof(n_tiles));
        file.read(meta_data_bytes.data(), qint64(meta_data_bytes.size()));
        zpp::bits::in in(meta_data_bytes);
        in(version_info).or_throw();

        if (version_info != T::version_information) {
            version_info[version_info.size() - 1] = 0;  // make sure that the string is 0 terminated.
            throw std::runtime_error(fmt::format("Cache file has incompatible version! Disk version is '{}', but we expected '{}'.",
                                                 version_info.data(),
                                                 T::version_information.data()));
        }

        in(n_tiles).or_throw();
        assert(n_tiles != u_int32_t(-1));
        m_data.reserve(n_tiles);
    }

    std::vector<char> size_info_bytes(sizeof(u_int32_t));
    std::vector<char> cache_object_bytes;
    while(file.read(size_info_bytes.data(), qint64(size_info_bytes.size()))) {
        zpp::bits::in in_size(size_info_bytes);
        u_int32_t bytes_to_read = 0;
        in_size(bytes_to_read).or_throw();

        cache_object_bytes.resize(bytes_to_read);
        const auto bytes_read = file.read(cache_object_bytes.data(), qint64(cache_object_bytes.size()));
        if (bytes_read != bytes_to_read)
            throw std::runtime_error("Cache file corrupted!");

        zpp::bits::in in(cache_object_bytes);
        CacheObject d;
        in(d).or_throw();
        m_data[d.data.id] = d;
    }
}

template <tile_types::NamedTile T>
template <typename VisitorFunction>
void Cache<T>::visit(const VisitorFunction& functor)
{
    const auto stamp = utils::time_since_epoch();
    static_assert(requires { { functor(T()) } -> utils::convertible_to<bool>; });
    const auto root = tile::Id { 0, { 0, 0 } };
    visit(root, functor, stamp);
}

template <tile_types::NamedTile T>
template <typename VisitorFunction>
void Cache<T>::visit(const tile::Id& node, const VisitorFunction& functor, uint64_t stamp)
{
    static_assert(requires { { functor(T()) } -> utils::convertible_to<bool>; });
    if (m_data.contains(node)) {
        const auto should_continue = functor(m_data[node].data);
        if (!should_continue)
            return;
        m_data[node].stamp = stamp * 100 - m_data[node].data.id.zoom_level;
        const auto children = node.children();
        for (const auto& id : children) {
            visit(id, functor, stamp);
        }
    }
}

template <tile_types::NamedTile T>
std::vector<T> Cache<T>::purge()
{
    if (m_capacity >= n_cached_objects())
        return {};
    std::vector<std::pair<tile::Id, uint64_t>> tiles;
    tiles.reserve(m_data.size());
    std::transform(m_data.cbegin(), m_data.cend(), std::back_inserter(tiles), [](const auto& entry) { return std::make_pair(entry.first, entry.second.stamp); });
    const auto nth_iter = tiles.begin() + m_capacity;
    std::nth_element(tiles.begin(), nth_iter, tiles.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::vector<T> purged_tiles;
    purged_tiles.reserve(tiles.size() - m_capacity);
    std::for_each(nth_iter, tiles.end(), [this, &purged_tiles](const auto& v) {
        purged_tiles.push_back(m_data[v.first].data);
        m_data.erase(v.first);
    });
    return purged_tiles;
}

} // namespace nucleus::tile_scheduler
