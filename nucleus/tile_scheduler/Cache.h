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
#include <unordered_set>

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
    struct MetaData {
        uint64_t visited;
        uint64_t created;
    };

    struct CacheObject {
        MetaData meta;
        T data;
    };

    std::unordered_map<tile::Id, CacheObject, tile::Id::Hasher> m_data;
    std::unordered_map<tile::Id, MetaData, tile::Id::Hasher> m_disk_cached;
    unsigned m_capacity = 5000;

public:
    Cache() = default;
    void insert(const T& tile);
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
    void visit(const tile::Id& start_node, const VisitorFunction& functor, uint64_t visited_stamp); // functor should return true, if the given tile should be marked visited. stops descending if false is returned.

    static std::filesystem::path tile_path(const std::filesystem::path& base_path, const tile::Id& id)
    {
        return base_path / fmt::format("{}_{}_{}.alp_tile", id.zoom_level, id.coords.x, id.coords.y);
    }

    static std::filesystem::path meta_info_path(const std::filesystem::path& base_path)
    {
        return base_path / "meta_info.alp";
    }
};

template <tile_types::NamedTile T>
void Cache<T>::insert(const T& tile)
{
    const auto time_stamp = utils::time_since_epoch();
    m_data[tile.id].meta.visited = time_stamp * 100 - tile.id.zoom_level;
    m_data[tile.id].meta.created = time_stamp;
    m_data[tile.id].data = tile;
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

template<tile_types::NamedTile T>
void Cache<T>::write_to_disk(const std::filesystem::path& base_path) {
    assert(tile_types::SerialisableTile<T>);
    std::filesystem::create_directories(base_path);

    const auto write = [](const auto& bytes, const auto& path) {
        QFile file(path);
        const auto success = file.open(QIODeviceBase::WriteOnly);
        if (!success)
            throw std::runtime_error(fmt::format("Couldn't open file '{}' for writing!", path.string()));
        file.write(bytes.data(), qint64(bytes.size()));
    };

    try {
        std::unordered_map<tile::Id, MetaData, tile::Id::Hasher> disk_cached_old;
        std::swap(m_disk_cached, disk_cached_old);
        m_disk_cached.reserve(m_data.size());

        // removing disk cache items, that were removed or updated in ram
        for (const auto& item : disk_cached_old) {
            const tile::Id& id = item.first;
            const MetaData& meta = item.second;
            if (m_data.contains(id) && m_data.at(id).meta.created == meta.created) {
                continue;
            }
            std::filesystem::remove(tile_path(base_path, id));
        }

        // write new or updated items to disk
        for (const auto& item : m_data) {
            const tile::Id& id = item.first;
            const CacheObject& cache_object = item.second;
            m_disk_cached[id] = cache_object.meta;

            if (disk_cached_old.contains(id) && disk_cached_old.at(id).created == cache_object.meta.created)
                continue;

            std::vector<char> bytes;
            zpp::bits::out out(bytes);
            const std::remove_cvref_t<decltype(T::version_information)> version = T::version_information;
            out(version).or_throw();
            out(cache_object.data).or_throw();

            write(bytes, tile_path(base_path, id));
        }

        std::vector<char> bytes;
        zpp::bits::out out(bytes);
        const std::remove_cvref_t<decltype(T::version_information)> version = T::version_information;
        out(version).or_throw();
        out(m_disk_cached).or_throw();

        write(bytes, meta_info_path(base_path));
    } catch (...) {
        m_disk_cached.clear();
        throw;
    }
}

template<tile_types::NamedTile T>
void Cache<T>::read_from_disk(const std::filesystem::path& base_path)
{
    assert(tile_types::SerialisableTile<T>);
    const auto check_version = [](auto* in, const auto& path) {
        std::remove_cvref_t<decltype(T::version_information)> version_info = {};
        (*in)(version_info).or_throw();
        if (version_info != T::version_information) {
            version_info[version_info.size() - 1] = 0;  // make sure that the string is 0 terminated.
            throw std::runtime_error(fmt::format("Cache file '{}' has incompatible version! Disk version is '{}', but we expected '{}'.",
                                                 path.string(),
                                                 version_info.data(),
                                                 T::version_information.data()));
        }
    };
    const auto read_all = [](const auto& path) {
        QFile file(path);
        const auto success = file.open(QIODeviceBase::ReadOnly);
        if (!success)
            throw std::runtime_error(fmt::format("Couldn't open file '{}' for reading!", path.string()));
        return file.readAll();
    };

    m_data.clear();
    m_disk_cached.clear();
    try {
        {
            const auto path = meta_info_path(base_path);
            const auto bytes = read_all(path);
            zpp::bits::in in(bytes);
            check_version(&in, path);
            in(m_disk_cached).or_throw();
        }

        for (const auto & entry : m_disk_cached) {
            const tile::Id& id = entry.first;
            const MetaData& meta = entry.second;

            const auto path = tile_path(base_path, id);
            const auto bytes = read_all(path);
            zpp::bits::in in(bytes);
            check_version(&in, path);

            CacheObject d;
            in(d.data).or_throw();
            d.meta = meta;
            m_data[d.data.id] = d;
        }
    } catch (...) {
        m_disk_cached.clear();
        m_data.clear();
        throw;
    }
}

template <tile_types::NamedTile T>
template <typename VisitorFunction>
void Cache<T>::visit(const VisitorFunction& functor)
{
    const auto visited = utils::time_since_epoch();
    static_assert(requires { { functor(T()) } -> utils::convertible_to<bool>; }, "VisitorFunction must accept a const NamedTile and return a bool.");
    const auto root = tile::Id { 0, { 0, 0 } };
    visit(root, functor, visited);
}

template <tile_types::NamedTile T>
template <typename VisitorFunction>
void Cache<T>::visit(const tile::Id& node, const VisitorFunction& functor, uint64_t visited_stamp)
{
    static_assert(requires { { functor(T()) } -> utils::convertible_to<bool>; });
    if (m_data.contains(node)) {
        const auto should_continue = functor(m_data[node].data);
        if (!should_continue)
            return;
        m_data[node].meta.visited = visited_stamp * 100 - m_data[node].data.id.zoom_level;
        const auto children = node.children();
        for (const auto& id : children) {
            visit(id, functor, visited_stamp);
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
    std::transform(m_data.cbegin(), m_data.cend(), std::back_inserter(tiles), [](const auto& entry) { return std::make_pair(entry.first, entry.second.meta.visited); });
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
