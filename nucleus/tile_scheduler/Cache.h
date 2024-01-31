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
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

#include <QFile>
#include <fmt/format.h>
#include <tl/expected.hpp>
#include <zpp_bits.h>

#include "radix/tile.h"
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

/// This class is thread safe. be careful with the visit method as it writes the cache and therefore locks an internal mutex.
template<tile_types::NamedTile T>
class Cache
{
    struct MetaData {
        uint64_t visited;
        uint64_t created;
    };

    struct CacheObject {
        MetaData meta;
        T data;
    };

    std::unordered_map<tile::Id, CacheObject, tile::Id::Hasher> m_data;
    mutable std::shared_mutex m_data_mutex;
    std::unordered_map<tile::Id, MetaData, tile::Id::Hasher> m_disk_cached;
    mutable std::shared_mutex m_disk_cached_mutex;

public:
    Cache() = default;
    void insert(const T& tile);
    [[nodiscard]] bool contains(const tile::Id& id) const;
    [[nodiscard]] unsigned n_cached_objects() const;
    /// functor should return true, if the given tile should be marked visited. stops descending if false is returned. don't do heavy lifting in the functort, as it blocks all other access!
    template<typename VisitorFunction>
    void visit(const VisitorFunction& functor);
    const T& peak_at(const tile::Id& id) const;
    std::vector<T> purge(unsigned remaining_capacity);

    [[nodiscard]] tl::expected<void, std::string> write_to_disk(const std::filesystem::path& path);
    [[nodiscard]] tl::expected<void, std::string> read_from_disk(const std::filesystem::path& path);

private:
    template<typename VisitorFunction>
    void visit(const tile::Id& start_node,
               const VisitorFunction& functor,
               uint64_t visited_stamp); // must stay private or protected by mutex

    static std::filesystem::path tile_path(const std::filesystem::path& base_path, const tile::Id& id)
    {
        return base_path / fmt::format("{}_{}_{}.alp_tile", id.zoom_level, id.coords.x, id.coords.y);
    }

    static std::filesystem::path meta_info_path(const std::filesystem::path& base_path)
    {
        return base_path / "meta_info.alp";
    }
};

using MemoryCache = nucleus::tile_scheduler::Cache<nucleus::tile_scheduler::tile_types::TileQuad>;

template <tile_types::NamedTile T>
void Cache<T>::insert(const T& tile)
{
    auto locker = std::scoped_lock(m_data_mutex);
    const auto time_stamp = utils::time_since_epoch();
    m_data[tile.id].meta.visited = time_stamp * 100 - tile.id.zoom_level;
    m_data[tile.id].meta.created = time_stamp;
    m_data[tile.id].data = tile;
}

template <tile_types::NamedTile T>
bool Cache<T>::contains(const tile::Id& id) const
{
    auto locker = std::shared_lock(m_data_mutex);
    return m_data.contains(id);
}

template <tile_types::NamedTile T>
unsigned int Cache<T>::n_cached_objects() const
{
    auto locker = std::shared_lock(m_data_mutex);
    return unsigned(m_data.size());
}

template <tile_types::NamedTile T>
const T& Cache<T>::peak_at(const tile::Id& id) const
{
    auto locker = std::shared_lock(m_data_mutex);
    return m_data.at(id).data;
}

template <tile_types::NamedTile T>
tl::expected<void, std::string> Cache<T>::write_to_disk(const std::filesystem::path& base_path)
{
    static_assert(tile_types::SerialisableTile<T>);
    std::filesystem::create_directories(base_path);
    std::unordered_map<tile::Id, CacheObject, tile::Id::Hasher> data;
    {
        auto locker = std::scoped_lock(m_data_mutex);
        data = m_data; // copies only metadata and references to tiles
    }
    auto locker = std::scoped_lock(m_disk_cached_mutex);

    const auto write = [](const auto& bytes, const auto& path) -> tl::expected<void, std::string> {
        QFile file(path);
        const auto success = file.open(QIODeviceBase::WriteOnly);
        if (!success)
            return tl::unexpected<std::string>(
                fmt::format("Couldn't open file '{}' for writing!", path.string()));
        file.write(bytes.data(), qint64(bytes.size()));
        return {};
    };

    std::unordered_map<tile::Id, MetaData, tile::Id::Hasher> disk_cached_old;
    std::swap(m_disk_cached, disk_cached_old);
    m_disk_cached.reserve(data.size());

    // removing disk cache items, that were removed or updated in ram
    for (const auto& item : disk_cached_old) {
        const tile::Id& id = item.first;
        const MetaData& meta = item.second;
        if (data.contains(id) && data.at(id).meta.created == meta.created) {
            continue;
        }
        std::filesystem::remove(tile_path(base_path, id));
    }

        // write new or updated items to disk
        for (const auto& item : data) {
            const tile::Id& id = item.first;
            const CacheObject& cache_object = item.second;
            m_disk_cached[id] = cache_object.meta;

            if (disk_cached_old.contains(id) && disk_cached_old.at(id).created == cache_object.meta.created)
                continue;

            std::vector<char> bytes;
            zpp::bits::out out(bytes);
            const std::remove_cvref_t<decltype(T::version_information)> version = T::version_information;
            {
                const auto r = out(version);
                if (failure(r))
                    return tl::unexpected(std::make_error_code(r).message());
            }
            {
                const auto r = out(cache_object.data);
                if (failure(r))
                    return tl::unexpected(std::make_error_code(r).message());
            }
            {
                const auto r = write(bytes, tile_path(base_path, id));
                if (!r.has_value())
                    return r;
            }
        }

        std::vector<char> bytes;
        zpp::bits::out out(bytes);
        const std::remove_cvref_t<decltype(T::version_information)> version = T::version_information;
        {
            const auto r = out(version);
            if (failure(r))
                return tl::unexpected(std::make_error_code(r).message());
        }

        {
            const auto r = out(m_disk_cached);
            if (failure(r))
                return tl::unexpected(std::make_error_code(r).message());
        }

        const auto r = write(bytes, meta_info_path(base_path));
        if (!r.has_value())
            return r;

        return {};
}

template <tile_types::NamedTile T>
tl::expected<void, std::string> Cache<T>::read_from_disk(const std::filesystem::path& base_path)
{
    auto locker = std::scoped_lock(m_data_mutex, m_disk_cached_mutex);
    assert(tile_types::SerialisableTile<T>);
    const auto check_version = [](auto* in, const auto& path) -> tl::expected<void, std::string> {
        std::remove_cvref_t<decltype(T::version_information)> version_info = {};
        {
            const auto r = (*in)(version_info);
            if (failure(r))
                return tl::unexpected(std::make_error_code(r).message());
        }
        if (version_info != T::version_information) {
            version_info[version_info.size() - 1] = 0;  // make sure that the string is 0 terminated.

            return tl::unexpected(fmt::format("Cache file '{}' has incompatible version! Disk "
                                              "version is '{}', but we expected '{}'.",
                path.string(),
                version_info.data(),
                T::version_information.data()));
        }
        return {};
    };
    const auto read_all = [](const auto& path) -> tl::expected<QByteArray, std::string> {
        QFile file(path);
        const auto success = file.open(QIODeviceBase::ReadOnly);
        if (!success)
            return tl::unexpected(fmt::format("Couldn't open file '{}' for reading!", path.string()));
        return file.readAll();
    };
    const auto clean_up = [&]() {
        m_disk_cached.clear();
        m_data.clear();
    };

    clean_up();
    {
        const auto path = meta_info_path(base_path);
        const auto bytes = read_all(path);
        if (!bytes.has_value()) {
            clean_up();
            return tl::unexpected(bytes.error());
        }
        zpp::bits::in in(bytes.value());
        {
            const auto r = check_version(&in, path);
            if (!r.has_value()) {
                clean_up();
                return r;
            }
        }
        {
            const auto r = in(m_disk_cached);
            if (failure(r)) {
                clean_up();
                return tl::unexpected(std::make_error_code(r).message());
            }
        }
    }

    for (const auto& entry : m_disk_cached) {
        const tile::Id& id = entry.first;
        const MetaData& meta = entry.second;

        const auto path = tile_path(base_path, id);
        const auto bytes = read_all(path);
        if (!bytes.has_value()) {
            clean_up();
            return tl::unexpected(bytes.error());
        }
        zpp::bits::in in(bytes.value());
        {
            const auto r = check_version(&in, path);
            if (!r.has_value()) {
                clean_up();
                return r;
            }
        }

        CacheObject d;
        {
            const auto r = in(d.data);
            if (failure(r)) {
                clean_up();
                return tl::unexpected(std::make_error_code(r).message());
            }
        }
        d.meta = meta;
        m_data[d.data.id] = d;
    }

    return {};
}

template <tile_types::NamedTile T>
template <typename VisitorFunction>
void Cache<T>::visit(const VisitorFunction& functor)
{
    auto locker = std::scoped_lock(m_data_mutex);
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

template<tile_types::NamedTile T>
std::vector<T> Cache<T>::purge(unsigned remaining_capacity)
{
    auto locker = std::scoped_lock(m_data_mutex);
    if (remaining_capacity >= m_data.size())
        return {};
    std::vector<std::pair<tile::Id, uint64_t>> tiles;
    tiles.reserve(m_data.size());
    std::transform(m_data.cbegin(), m_data.cend(), std::back_inserter(tiles), [](const auto& entry) { return std::make_pair(entry.first, entry.second.meta.visited); });
    const auto nth_iter = tiles.begin() + remaining_capacity;
    std::nth_element(tiles.begin(), nth_iter, tiles.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::vector<T> purged_tiles;
    purged_tiles.reserve(tiles.size() - remaining_capacity);
    std::for_each(nth_iter, tiles.end(), [this, &purged_tiles](const auto& v) {
        purged_tiles.push_back(m_data[v.first].data);
        m_data.erase(v.first);
    });
    return purged_tiles;
}

} // namespace nucleus::tile_scheduler
